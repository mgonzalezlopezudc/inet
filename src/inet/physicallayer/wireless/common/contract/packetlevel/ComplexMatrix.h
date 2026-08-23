//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_COMPLEXMATRIX_H
#define __INET_COMPLEXMATRIX_H

#include <complex>
#include <vector>

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

class INET_API ComplexMatrix
{
  protected:
    int numRows = 0;
    int numColumns = 0;
    std::vector<std::complex<double>> coefficients;

  public:
    ComplexMatrix() = default;
    ComplexMatrix(int numRows, int numColumns);
    ComplexMatrix(int numRows, int numColumns, const std::vector<std::complex<double>>& coefficients);

    int getNumRows() const { return numRows; }
    int getNumColumns() const { return numColumns; }
    const std::vector<std::complex<double>>& getCoefficients() const { return coefficients; }
    const std::complex<double>& get(int row, int column) const;
    std::complex<double>& get(int row, int column);
    bool isFinite() const;
    ComplexMatrix transpose() const;
};

} // namespace physicallayer
} // namespace inet

#endif
