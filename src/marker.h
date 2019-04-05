#ifndef MARKER_H
#define MARKER_H

#include "common.h"

#include <Eigen/Eigen>
#include <memory>

using namespace Eigen;

struct CompressedMarker
{
    std::shared_ptr<unsigned char[]> buffer = nullptr;
    IndexEntry index;
};

struct Marker
{
    virtual ~Marker();

    unsigned int i = 0;
    unsigned int numInds = 0;

    virtual double computeNum(VectorXd &epsilon,
                              const double beta_old) = 0;

    virtual void updateEpsilon(VectorXd &epsilon,
                               const double beta_old,
                               const double beta) = 0;

    virtual CompressedMarker compress() const;
    virtual void decompress(unsigned char* data,
                            const IndexEntry& index);

    virtual std::streamsize size() const = 0;
    virtual void read(std::istream *inStream) = 0;
    virtual void write(std::ostream *outStream) const = 0;

    virtual bool isValid() const = 0;
};

#endif // MARKER_H
