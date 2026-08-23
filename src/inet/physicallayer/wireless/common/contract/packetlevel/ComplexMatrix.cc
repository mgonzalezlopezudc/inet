//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/contract/packetlevel/ComplexMatrix.h"

#include <cmath>

namespace inet {
namespace physicallayer {

ComplexMatrix::ComplexMatrix(int numRows, int numColumns) :
    numRows(numRows), numColumns(numColumns), coefficients(numRows * numColumns)
{
    if (numRows <= 0 || numColumns <= 0)
        throw cRuntimeError("Complex matrix dimensions must be positive, got %d x %d", numRows, numColumns);
}

ComplexMatrix::ComplexMatrix(int numRows, int numColumns, const std::vector<std::complex<double>>& coefficients) :
    numRows(numRows), numColumns(numColumns), coefficients(coefficients)
{
    if (numRows <= 0 || numColumns <= 0)
        throw cRuntimeError("Complex matrix dimensions must be positive, got %d x %d", numRows, numColumns);
    if ((int)coefficients.size() != numRows * numColumns)
        throw cRuntimeError("Complex matrix coefficient count %zu does not match dimensions %d x %d", coefficients.size(), numRows, numColumns);
    if (!isFinite())
        throw cRuntimeError("Complex matrix contains a non-finite coefficient");
}

const std::complex<double>& ComplexMatrix::get(int row, int column) const
{
    if (row < 0 || row >= numRows || column < 0 || column >= numColumns)
        throw cRuntimeError("Complex matrix index (%d,%d) is outside %d x %d", row, column, numRows, numColumns);
    return coefficients[row * numColumns + column];
}

std::complex<double>& ComplexMatrix::get(int row, int column)
{
    if (row < 0 || row >= numRows || column < 0 || column >= numColumns)
        throw cRuntimeError("Complex matrix index (%d,%d) is outside %d x %d", row, column, numRows, numColumns);
    return coefficients[row * numColumns + column];
}

bool ComplexMatrix::isFinite() const
{
    for (const auto& coefficient : coefficients)
        if (!std::isfinite(coefficient.real()) || !std::isfinite(coefficient.imag()))
            return false;
    return true;
}

ComplexMatrix ComplexMatrix::transpose() const
{
    ComplexMatrix result(numColumns, numRows);
    for (int row = 0; row < numRows; row++)
        for (int column = 0; column < numColumns; column++)
            result.get(column, row) = get(row, column);
    return result;
}

} // namespace physicallayer
} // namespace inet
