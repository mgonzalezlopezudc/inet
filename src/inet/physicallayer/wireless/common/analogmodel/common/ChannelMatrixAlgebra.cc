//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixAlgebra.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace inet {
namespace physicallayer {

namespace {

void validateTolerance(double relativeTolerance)
{
    if (!std::isfinite(relativeTolerance) || relativeTolerance < 0)
        throw cRuntimeError("Matrix relative tolerance must be finite and nonnegative, got %g", relativeTolerance);
}

void validateScalar(const std::complex<double>& scalar, const char *operation)
{
    if (!std::isfinite(scalar.real()) || !std::isfinite(scalar.imag()))
        throw cRuntimeError("%s requires a finite scalar", operation);
}

void validateIndices(const ComplexMatrix& matrix, const std::vector<int>& indices, bool rows, const char *operation)
{
    ChannelMatrixAlgebra::validateFinite(matrix, operation);
    const int limit = rows ? matrix.getNumRows() : matrix.getNumColumns();
    if (indices.empty())
        throw cRuntimeError("%s requires a nonempty index set", operation);
    int previous = -1;
    for (int index : indices) {
        if (index < 0 || index >= limit)
            throw cRuntimeError("%s index %d is outside [0,%d)", operation, index, limit);
        if (index <= previous)
            throw cRuntimeError("%s indices must be strictly increasing and unique", operation);
        previous = index;
    }
}

double maximumCoefficientMagnitude(const ComplexMatrix& matrix)
{
    double result = 0;
    for (const auto& coefficient : matrix.getCoefficients())
        result = std::max(result, std::abs(coefficient));
    return result;
}

double scaleForTolerance(const ComplexMatrix& matrix)
{
    // A relative tolerance must scale with the matrix itself.  In particular,
    // a well-conditioned covariance matrix whose coefficients are much smaller
    // than one must not be rejected because of an unrelated unit-sized floor.
    return maximumCoefficientMagnitude(matrix);
}

bool isHermitianImpl(const ComplexMatrix& matrix, double relativeTolerance)
{
    if (matrix.getNumRows() <= 0 || matrix.getNumRows() != matrix.getNumColumns() || !matrix.isFinite())
        return false;
    const double tolerance = relativeTolerance * scaleForTolerance(matrix);
    for (int row = 0; row < matrix.getNumRows(); row++) {
        if (std::abs(matrix.get(row, row).imag()) > tolerance)
            return false;
        for (int column = row + 1; column < matrix.getNumColumns(); column++)
            if (std::abs(matrix.get(row, column) - std::conj(matrix.get(column, row))) > tolerance)
                return false;
    }
    return true;
}

bool choleskyFactor(const ComplexMatrix& matrix, double relativeTolerance, std::vector<std::complex<double>>& lower)
{
    if (matrix.getNumRows() <= 0 || matrix.getNumRows() != matrix.getNumColumns() || !matrix.isFinite() ||
        !isHermitianImpl(matrix, relativeTolerance))
        return false;

    const int size = matrix.getNumRows();
    const double tolerance = relativeTolerance * scaleForTolerance(matrix);
    lower.assign(size * size, std::complex<double>(0, 0));
    auto getLower = [&](int row, int column) -> std::complex<double>& {
        return lower[row * size + column];
    };
    for (int row = 0; row < size; row++) {
        for (int column = 0; column <= row; column++) {
            std::complex<double> value = matrix.get(row, column);
            for (int inner = 0; inner < column; inner++)
                value -= getLower(row, inner) * std::conj(getLower(column, inner));
            if (row == column) {
                if (std::abs(value.imag()) > tolerance || value.real() <= tolerance)
                    return false;
                getLower(row, column) = std::sqrt(value.real());
            }
            else {
                const double diagonal = getLower(column, column).real();
                if (diagonal <= 0 || !std::isfinite(diagonal))
                    return false;
                getLower(row, column) = value / diagonal;
            }
        }
    }
    return true;
}

} // namespace

void ChannelMatrixAlgebra::validateFinite(const ComplexMatrix& matrix, const char *operation)
{
    if (matrix.getNumRows() <= 0 || matrix.getNumColumns() <= 0)
        throw cRuntimeError("%s requires a nonempty matrix, got %d x %d", operation,
            matrix.getNumRows(), matrix.getNumColumns());
    if (!matrix.isFinite())
        throw cRuntimeError("%s contains a non-finite coefficient", operation);
}

void ChannelMatrixAlgebra::validateDimensions(const ComplexMatrix& matrix, int numRows, int numColumns,
    const char *operation)
{
    validateFinite(matrix, operation);
    if (numRows <= 0 || numColumns <= 0)
        throw cRuntimeError("%s expected positive dimensions, got %d x %d", operation, numRows, numColumns);
    if (matrix.getNumRows() != numRows || matrix.getNumColumns() != numColumns)
        throw cRuntimeError("%s expected %d x %d, got %d x %d", operation, numRows, numColumns,
            matrix.getNumRows(), matrix.getNumColumns());
}

void ChannelMatrixAlgebra::validateSquare(const ComplexMatrix& matrix, const char *operation)
{
    validateFinite(matrix, operation);
    if (matrix.getNumRows() <= 0 || matrix.getNumRows() != matrix.getNumColumns())
        throw cRuntimeError("%s requires a square matrix, got %d x %d", operation,
            matrix.getNumRows(), matrix.getNumColumns());
}

void ChannelMatrixAlgebra::validateHermitian(const ComplexMatrix& matrix, double relativeTolerance, const char *operation)
{
    validateTolerance(relativeTolerance);
    validateSquare(matrix, operation);
    if (!isHermitianImpl(matrix, relativeTolerance))
        throw cRuntimeError("%s is not Hermitian within relative tolerance %g", operation, relativeTolerance);
}

void ChannelMatrixAlgebra::validatePositiveDefinite(const ComplexMatrix& matrix, double relativeTolerance, const char *operation)
{
    validateTolerance(relativeTolerance);
    validateSquare(matrix, operation);
    std::vector<std::complex<double>> lower;
    if (!choleskyFactor(matrix, relativeTolerance, lower))
        throw cRuntimeError("%s is not Hermitian positive definite within relative tolerance %g", operation, relativeTolerance);
}

bool ChannelMatrixAlgebra::isHermitian(const ComplexMatrix& matrix, double relativeTolerance)
{
    validateTolerance(relativeTolerance);
    return isHermitianImpl(matrix, relativeTolerance);
}

bool ChannelMatrixAlgebra::isPositiveDefinite(const ComplexMatrix& matrix, double relativeTolerance)
{
    validateTolerance(relativeTolerance);
    std::vector<std::complex<double>> lower;
    return choleskyFactor(matrix, relativeTolerance, lower);
}

ComplexMatrix ChannelMatrixAlgebra::conjugateTranspose(const ComplexMatrix& matrix)
{
    validateFinite(matrix, "Conjugate transpose");
    ComplexMatrix result(matrix.getNumColumns(), matrix.getNumRows());
    for (int row = 0; row < matrix.getNumRows(); row++)
        for (int column = 0; column < matrix.getNumColumns(); column++)
            result.get(column, row) = std::conj(matrix.get(row, column));
    return result;
}

ComplexMatrix ChannelMatrixAlgebra::multiply(const ComplexMatrix& left, const ComplexMatrix& right)
{
    validateFinite(left, "Left matrix");
    validateFinite(right, "Right matrix");
    if (left.getNumColumns() != right.getNumRows())
        throw cRuntimeError("Cannot multiply matrices with inner dimensions %d and %d",
            left.getNumColumns(), right.getNumRows());
    ComplexMatrix result(left.getNumRows(), right.getNumColumns());
    for (int row = 0; row < result.getNumRows(); row++)
        for (int column = 0; column < result.getNumColumns(); column++) {
            std::complex<double> value = 0;
            for (int inner = 0; inner < left.getNumColumns(); inner++)
                value += left.get(row, inner) * right.get(inner, column);
            result.get(row, column) = value;
        }
    validateFinite(result, "Matrix multiplication result");
    return result;
}

ComplexMatrix ChannelMatrixAlgebra::hermitianProduct(const ComplexMatrix& matrix)
{
    return multiply(conjugateTranspose(matrix), matrix);
}

ComplexMatrix ChannelMatrixAlgebra::identity(int size)
{
    if (size <= 0)
        throw cRuntimeError("Identity matrix requires a positive size, got %d", size);
    ComplexMatrix result(size, size);
    for (int index = 0; index < size; index++)
        result.get(index, index) = 1;
    return result;
}

ComplexMatrix ChannelMatrixAlgebra::add(const ComplexMatrix& left, const ComplexMatrix& right)
{
    validateFinite(left, "Left matrix addition operand");
    validateFinite(right, "Right matrix addition operand");
    if (left.getNumRows() != right.getNumRows() || left.getNumColumns() != right.getNumColumns())
        throw cRuntimeError("Cannot add matrices with dimensions %d x %d and %d x %d",
            left.getNumRows(), left.getNumColumns(), right.getNumRows(), right.getNumColumns());
    ComplexMatrix result(left.getNumRows(), left.getNumColumns());
    for (int row = 0; row < result.getNumRows(); row++)
        for (int column = 0; column < result.getNumColumns(); column++)
            result.get(row, column) = left.get(row, column) + right.get(row, column);
    validateFinite(result, "Matrix addition result");
    return result;
}

ComplexMatrix ChannelMatrixAlgebra::scale(const ComplexMatrix& matrix, std::complex<double> scalar)
{
    validateFinite(matrix, "Matrix scale operand");
    validateScalar(scalar, "Matrix scaling");
    ComplexMatrix result(matrix.getNumRows(), matrix.getNumColumns());
    for (int row = 0; row < result.getNumRows(); row++)
        for (int column = 0; column < result.getNumColumns(); column++)
            result.get(row, column) = scalar * matrix.get(row, column);
    validateFinite(result, "Matrix scale result");
    return result;
}

ComplexMatrix ChannelMatrixAlgebra::selectRows(const ComplexMatrix& matrix, const std::vector<int>& rowIndices)
{
    validateIndices(matrix, rowIndices, true, "Row selection");
    ComplexMatrix result(rowIndices.size(), matrix.getNumColumns());
    for (int resultRow = 0; resultRow < (int)rowIndices.size(); resultRow++)
        for (int column = 0; column < matrix.getNumColumns(); column++)
            result.get(resultRow, column) = matrix.get(rowIndices[resultRow], column);
    return result;
}

ComplexMatrix ChannelMatrixAlgebra::selectColumns(const ComplexMatrix& matrix, const std::vector<int>& columnIndices)
{
    validateIndices(matrix, columnIndices, false, "Column selection");
    ComplexMatrix result(matrix.getNumRows(), columnIndices.size());
    for (int row = 0; row < matrix.getNumRows(); row++)
        for (int resultColumn = 0; resultColumn < (int)columnIndices.size(); resultColumn++)
            result.get(row, resultColumn) = matrix.get(row, columnIndices[resultColumn]);
    return result;
}

ComplexMatrix ChannelMatrixAlgebra::selectRowsAndColumns(const ComplexMatrix& matrix, const std::vector<int>& indices)
{
    validateSquare(matrix, "Square row-and-column selection");
    validateIndices(matrix, indices, true, "Square row-and-column selection");
    ComplexMatrix result(indices.size(), indices.size());
    for (int resultRow = 0; resultRow < (int)indices.size(); resultRow++)
        for (int resultColumn = 0; resultColumn < (int)indices.size(); resultColumn++)
            result.get(resultRow, resultColumn) = matrix.get(indices[resultRow], indices[resultColumn]);
    return result;
}

int ChannelMatrixAlgebra::computeRank(const ComplexMatrix& matrix, double relativeTolerance)
{
    validateTolerance(relativeTolerance);
    validateFinite(matrix, "Rank computation");
    const int numRows = matrix.getNumRows();
    const int numColumns = matrix.getNumColumns();
    const double tolerance = relativeTolerance * scaleForTolerance(matrix);
    std::vector<std::complex<double>> values = matrix.getCoefficients();
    int rank = 0;
    for (int column = 0; column < numColumns && rank < numRows; column++) {
        int pivotRow = rank;
        double pivotMagnitude = 0;
        for (int row = rank; row < numRows; row++) {
            const double magnitude = std::abs(values[row * numColumns + column]);
            if (magnitude > pivotMagnitude) {
                pivotMagnitude = magnitude;
                pivotRow = row;
            }
        }
        if (pivotMagnitude <= tolerance)
            continue;
        if (pivotRow != rank)
            for (int remainingColumn = column; remainingColumn < numColumns; remainingColumn++)
                std::swap(values[rank * numColumns + remainingColumn], values[pivotRow * numColumns + remainingColumn]);
        const std::complex<double> pivot = values[rank * numColumns + column];
        for (int row = rank + 1; row < numRows; row++) {
            const std::complex<double> factor = values[row * numColumns + column] / pivot;
            for (int remainingColumn = column; remainingColumn < numColumns; remainingColumn++)
                values[row * numColumns + remainingColumn] -= factor * values[rank * numColumns + remainingColumn];
        }
        rank++;
    }
    return rank;
}

double ChannelMatrixAlgebra::computeConditionNumber(const ComplexMatrix& matrix, double relativeTolerance)
{
    validateTolerance(relativeTolerance);
    validateSquare(matrix, "Condition-number computation");
    const int size = matrix.getNumRows();
    if (computeRank(matrix, relativeTolerance) < size)
        return std::numeric_limits<double>::infinity();

    double matrixNorm = 0;
    for (int column = 0; column < size; column++) {
        double columnNorm = 0;
        for (int row = 0; row < size; row++)
            columnNorm += std::abs(matrix.get(row, column));
        matrixNorm = std::max(matrixNorm, columnNorm);
    }

    double inverseNormEstimate = 0;
    for (int column = 0; column < size; column++) {
        ComplexMatrix rightHandSide(size, 1);
        rightHandSide.get(column, 0) = 1;
        const ComplexMatrix solution = solve(matrix, rightHandSide, relativeTolerance);
        double columnNorm = 0;
        for (int row = 0; row < size; row++)
            columnNorm += std::abs(solution.get(row, 0));
        inverseNormEstimate = std::max(inverseNormEstimate, columnNorm);
    }
    return matrixNorm * inverseNormEstimate;
}

ComplexMatrix ChannelMatrixAlgebra::solve(const ComplexMatrix& coefficient, const ComplexMatrix& rightHandSide,
    double relativeTolerance)
{
    validateTolerance(relativeTolerance);
    validateSquare(coefficient, "Linear solve coefficient");
    validateFinite(rightHandSide, "Linear solve right-hand side");
    if (coefficient.getNumRows() != rightHandSide.getNumRows())
        throw cRuntimeError("Linear solve dimensions do not match: coefficient is %d x %d, right-hand side is %d x %d",
            coefficient.getNumRows(), coefficient.getNumColumns(), rightHandSide.getNumRows(), rightHandSide.getNumColumns());

    const int size = coefficient.getNumRows();
    const int rightHandSideCount = rightHandSide.getNumColumns();
    const double tolerance = relativeTolerance * scaleForTolerance(coefficient);
    std::vector<std::complex<double>> values = coefficient.getCoefficients();
    std::vector<std::complex<double>> result = rightHandSide.getCoefficients();
    for (int column = 0; column < size; column++) {
        int pivotRow = column;
        double pivotMagnitude = 0;
        for (int row = column; row < size; row++) {
            const double magnitude = std::abs(values[row * size + column]);
            if (magnitude > pivotMagnitude) {
                pivotMagnitude = magnitude;
                pivotRow = row;
            }
        }
        if (pivotMagnitude <= tolerance)
            throw cRuntimeError("Linear solve coefficient is singular or ill-conditioned at pivot %d", column);
        if (pivotRow != column) {
            for (int remainingColumn = column; remainingColumn < size; remainingColumn++)
                std::swap(values[column * size + remainingColumn], values[pivotRow * size + remainingColumn]);
            for (int rightHandSideColumn = 0; rightHandSideColumn < rightHandSideCount; rightHandSideColumn++)
                std::swap(result[column * rightHandSideCount + rightHandSideColumn], result[pivotRow * rightHandSideCount + rightHandSideColumn]);
        }
        for (int row = column + 1; row < size; row++) {
            const std::complex<double> factor = values[row * size + column] / values[column * size + column];
            values[row * size + column] = 0;
            for (int remainingColumn = column + 1; remainingColumn < size; remainingColumn++)
                values[row * size + remainingColumn] -= factor * values[column * size + remainingColumn];
            for (int rightHandSideColumn = 0; rightHandSideColumn < rightHandSideCount; rightHandSideColumn++)
                result[row * rightHandSideCount + rightHandSideColumn] -= factor * result[column * rightHandSideCount + rightHandSideColumn];
        }
    }

    ComplexMatrix solution(size, rightHandSideCount);
    for (int row = size - 1; row >= 0; row--)
        for (int rightHandSideColumn = 0; rightHandSideColumn < rightHandSideCount; rightHandSideColumn++) {
            std::complex<double> value = result[row * rightHandSideCount + rightHandSideColumn];
            for (int column = row + 1; column < size; column++)
                value -= values[row * size + column] * solution.get(column, rightHandSideColumn);
            solution.get(row, rightHandSideColumn) = value / values[row * size + row];
        }
    return solution;
}

ComplexMatrix ChannelMatrixAlgebra::solveHermitianPositiveDefinite(const ComplexMatrix& coefficient,
    const ComplexMatrix& rightHandSide, double relativeTolerance)
{
    validateTolerance(relativeTolerance);
    validateSquare(coefficient, "Hermitian positive-definite solve coefficient");
    validateFinite(rightHandSide, "Hermitian positive-definite solve right-hand side");
    if (coefficient.getNumRows() != rightHandSide.getNumRows())
        throw cRuntimeError("Hermitian positive-definite solve dimensions do not match");

    std::vector<std::complex<double>> lower;
    if (!choleskyFactor(coefficient, relativeTolerance, lower))
        throw cRuntimeError("Hermitian positive-definite solve requires a Hermitian positive-definite coefficient");
    const int size = coefficient.getNumRows();
    const int rightHandSideCount = rightHandSide.getNumColumns();
    auto getLower = [&](int row, int column) -> const std::complex<double>& {
        return lower[row * size + column];
    };
    ComplexMatrix intermediate(size, rightHandSideCount);
    for (int row = 0; row < size; row++)
        for (int rightHandSideColumn = 0; rightHandSideColumn < rightHandSideCount; rightHandSideColumn++) {
            std::complex<double> value = rightHandSide.get(row, rightHandSideColumn);
            for (int column = 0; column < row; column++)
                value -= getLower(row, column) * intermediate.get(column, rightHandSideColumn);
            intermediate.get(row, rightHandSideColumn) = value / getLower(row, row).real();
        }

    ComplexMatrix solution(size, rightHandSideCount);
    for (int row = size - 1; row >= 0; row--)
        for (int rightHandSideColumn = 0; rightHandSideColumn < rightHandSideCount; rightHandSideColumn++) {
            std::complex<double> value = intermediate.get(row, rightHandSideColumn);
            for (int column = row + 1; column < size; column++)
                value -= std::conj(getLower(column, row)) * solution.get(column, rightHandSideColumn);
            solution.get(row, rightHandSideColumn) = value / getLower(row, row).real();
        }
    return solution;
}

} // namespace physicallayer
} // namespace inet
