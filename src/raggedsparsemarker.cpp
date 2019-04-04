#include "raggedsparsemarker.h"

#include "compression.h"

#include <fstream>
#include <iostream>
#include <iterator>

void RaggedSparseMarker::updateEpsilon(VectorXd &epsilon, const double beta_old, const double beta)
{
    SparseMarker::updateEpsilon(epsilon, beta_old, beta);

    const double dBeta = beta_old - beta;
    const auto meanAdjustment = dBeta * mean / sd;
    // 1. Adjust for the means. If snp is 0, this will be the only adjustment made
    epsilon.array() -= meanAdjustment;

    // 2. Adjust for snp 1 values
    const double oneAdjustment = dBeta / sd;
    epsilon(Zones).array() += oneAdjustment;

    // 3. Adjust for snp 2 values
    epsilon(Ztwos).array() += 2 * oneAdjustment;

    // 4. For missing values, undo step 1
    epsilon(Zmissing).array() += meanAdjustment;
}

CompressedMarker RaggedSparseMarker::compress() const
{
    // Prepare a stream to write into
    const auto bufferSize = static_cast<unsigned int>(size());
    std::unique_ptr<char[]> buffer;
    buffer.reset(new char[bufferSize]);

    std::ostringstream stream;
    stream.rdbuf()->pubsetbuf(buffer.get(), size());

    // Write the marker to the stream
    write(&stream);

    // Compress the stream
    const auto maxCompressedOutputSize = maxCompressedDataSize<char>(bufferSize);

    CompressedMarker compressed;
    compressed.buffer.reset(new unsigned char[maxCompressedOutputSize]);
    compressed.index.originalSize = static_cast<unsigned long>(size());
    compressed.index.compressedSize = compressData(buffer.get(),
                                                   bufferSize,
                                                   compressed.buffer.get(),
                                                   maxCompressedOutputSize);
    return compressed;
}

void RaggedSparseMarker::decompress(unsigned char *data, const IndexEntry &index)
{
    // Prepare a buffer to decompress into
    const auto bufferSize = static_cast<unsigned int>(index.originalSize);
    std::unique_ptr<char[]> buffer;
    buffer.reset(new char[bufferSize]);

    // Decompress into the buffer
    extractData(data + index.pos,
                static_cast<unsigned int>(index.compressedSize),
                reinterpret_cast<unsigned char*>(buffer.get()),
                bufferSize);

    // Prepare a stream to read from
    std::istringstream stream;
    stream.rdbuf()->pubsetbuf(buffer.get(),
                              static_cast<std::streamsize>(index.originalSize));

    // Read the marker from the stream
    read(&stream);
}

std::streamsize RaggedSparseMarker::size() const
{
    const auto valueTypeSize = sizeof(RaggedSparseMarker::IndexVector::value_type);
    const auto sizeTypeSize = sizeof(RaggedSparseMarker::IndexVector::size_type);

    return SparseMarker::size() +
            static_cast<std::streamsize>(
                sizeTypeSize + (valueTypeSize * Zones.size()) +
                sizeTypeSize + (valueTypeSize * Ztwos.size()) +
                sizeTypeSize + (valueTypeSize * Zmissing.size()));
}

void RaggedSparseMarker::read(std::istream *inStream)
{
    if (inStream->fail()) {
        std::cerr << "Error: unable to read RaggedSparseMarker!" << std::endl;
        return;
    }

    SparseMarker::read(inStream);

    auto readIndexVector = [&](IndexVector &v) {
        IndexVector::size_type size = 0;
        inStream->read(reinterpret_cast<char *>(&size),
                         sizeof(IndexVector::size_type));

        v.clear();
        v.resize(size);
        if (size > 0)
            inStream->read(reinterpret_cast<char *>(v.data()),
                           static_cast<std::streamsize>(size * sizeof (IndexVector::value_type)));
    };

    readIndexVector(Zones);
    readIndexVector(Ztwos);
    readIndexVector(Zmissing);
}

void RaggedSparseMarker::write(std::ostream *outStream) const
{
    if (outStream->fail()) {
        std::cerr << "Error: unable to write RaggedSparseMarker!" << std::endl;
        return;
    }

    SparseMarker::write(outStream);

    using IndexVector = RaggedSparseMarker::IndexVector;
    auto writeIndexVector = [&](const IndexVector & v) {
        const IndexVector::size_type size = v.size();
        outStream->write(reinterpret_cast<const char *>(&size),
                         sizeof(IndexVector::size_type));

        if (size > 0)
            outStream->write(reinterpret_cast<const char *>(v.data()),
                             static_cast<std::streamsize>(size * sizeof (IndexVector::value_type)));
    };

    writeIndexVector(Zones);
    writeIndexVector(Ztwos);
    writeIndexVector(Zmissing);
}

bool RaggedSparseMarker::isValid() const
{
    const bool invalid = Zones.empty() && Ztwos.empty();
    if (invalid)
        std::cerr << "SNPs that do not vary are should be removed prior to analysis. "
                  << "Otherwise, this message indicates a decompression error"
                  << std::endl;

    return !invalid;
}

double RaggedSparseMarker::dot(const VectorXd &epsilon) const
{
    return (epsilon(Zones).sum() + 2 * epsilon(Ztwos).sum()) / sd;
}
