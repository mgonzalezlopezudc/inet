//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXALGEBRA_H
#define __INET_CHANNELMATRIXALGEBRA_H

#include <complex>
#include <vector>

#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h"

namespace inet {
namespace physicallayer {

/**
 * Small, deterministic complex-matrix operations used by matrix-valued
 * wireless analog models.
 *
 * The class deliberately exposes solves and validation rather than an
 * inverse operation.  Receiver algorithms can therefore use a factorization
 * without materializing an inverse matrix.
 */
class INET_API ChannelMatrixAlgebra
{
  public:
    static constexpr double DEFAULT_RELATIVE_TOLERANCE = 1e-10;

    static void validateFinite(const ComplexMatrix& matrix, const char *operation = "Complex matrix");
    static void validateDimensions(const ComplexMatrix& matrix, int numRows, int numColumns,
        const char *operation = "Complex matrix");
    static void validateSquare(const ComplexMatrix& matrix, const char *operation = "Complex matrix");
    static void validateHermitian(const ComplexMatrix& matrix,
        double relativeTolerance = DEFAULT_RELATIVE_TOLERANCE, const char *operation = "Hermitian matrix");
    static void validatePositiveDefinite(const ComplexMatrix& matrix,
        double relativeTolerance = DEFAULT_RELATIVE_TOLERANCE, const char *operation = "Positive-definite matrix");

    static bool isHermitian(const ComplexMatrix& matrix,
        double relativeTolerance = DEFAULT_RELATIVE_TOLERANCE);
    static bool isPositiveDefinite(const ComplexMatrix& matrix,
        double relativeTolerance = DEFAULT_RELATIVE_TOLERANCE);

    static ComplexMatrix conjugateTranspose(const ComplexMatrix& matrix);
    static ComplexMatrix multiply(const ComplexMatrix& left, const ComplexMatrix& right);
    static ComplexMatrix hermitianProduct(const ComplexMatrix& matrix);
    static ComplexMatrix identity(int size);
    static ComplexMatrix add(const ComplexMatrix& left, const ComplexMatrix& right);
    static ComplexMatrix scale(const ComplexMatrix& matrix, std::complex<double> scalar);
    static ComplexMatrix selectRows(const ComplexMatrix& matrix, const std::vector<int>& rowIndices);
    static ComplexMatrix selectColumns(const ComplexMatrix& matrix, const std::vector<int>& columnIndices);
    static ComplexMatrix selectRowsAndColumns(const ComplexMatrix& matrix, const std::vector<int>& indices);

    static int computeRank(const ComplexMatrix& matrix,
        double relativeTolerance = DEFAULT_RELATIVE_TOLERANCE);
    static double computeConditionNumber(const ComplexMatrix& matrix,
        double relativeTolerance = DEFAULT_RELATIVE_TOLERANCE);

    /** Solves A * X = B using deterministic partial-pivoting elimination. */
    static ComplexMatrix solve(const ComplexMatrix& coefficient, const ComplexMatrix& rightHandSide,
        double relativeTolerance = DEFAULT_RELATIVE_TOLERANCE);

    /** Solves A * X = B using a Hermitian positive-definite Cholesky factorization. */
    static ComplexMatrix solveHermitianPositiveDefinite(const ComplexMatrix& coefficient,
        const ComplexMatrix& rightHandSide, double relativeTolerance = DEFAULT_RELATIVE_TOLERANCE);
};

} // namespace physicallayer
} // namespace inet

#endif
