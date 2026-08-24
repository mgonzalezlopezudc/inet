//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgnMimoChannel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace inet {
namespace physicallayer {

namespace {

constexpr double PI = 3.141592653589793238462643383279502884;
constexpr double PSD_TOLERANCE = 1e-9;
constexpr double EIGEN_RESIDUAL_TOLERANCE = 1e-8;
constexpr double RECONSTRUCTION_TOLERANCE = 1e-8;

void requireSquare(const ComplexMatrix& matrix, const char *operation)
{
    if (matrix.getNumRows() <= 0 || matrix.getNumRows() != matrix.getNumColumns())
        throw cRuntimeError("%s requires a nonempty square matrix", operation);
    if (!matrix.isFinite())
        throw cRuntimeError("%s requires finite matrix coefficients", operation);
}

template<typename Function>
std::complex<double> integrateCircularLaplacianHalf(double begin, double end, Function function)
{
    const int panels = TgnMimoChannel::SPATIAL_QUADRATURE_PANELS;
    const double step = (end - begin) / panels;
    std::complex<double> sum = function(begin) + function(end);
    for (int i = 1; i < panels; i++)
        sum += (i & 1 ? 4.0 : 2.0) * function(begin + i * step);
    return sum * (step / 3.0);
}

template<typename Function>
std::complex<double> integrateCircularLaplacian(Function function)
{
    return integrateCircularLaplacianHalf(-PI, 0, function) +
           integrateCircularLaplacianHalf(0, PI, function);
}

} // namespace

TgnLorentzianProcess::TgnLorentzianProcess(const std::vector<double>& oscillatorFrequenciesHz,
    const std::vector<std::complex<double>>& coefficients) :
    oscillatorFrequenciesHz(oscillatorFrequenciesHz), coefficients(coefficients)
{
    if (oscillatorFrequenciesHz.empty() || oscillatorFrequenciesHz.size() != coefficients.size())
        throw cRuntimeError("A TGn oscillator process requires equally sized nonempty frequency and coefficient arrays");
    for (size_t i = 0; i < coefficients.size(); i++)
        if (!std::isfinite(oscillatorFrequenciesHz[i]) || !std::isfinite(coefficients[i].real()) || !std::isfinite(coefficients[i].imag()))
            throw cRuntimeError("A TGn oscillator process contains a nonfinite value at index %zu", i);
}

double TgnMimoChannel::frobeniusNorm(const ComplexMatrix& matrix)
{
    double squaredNorm = 0;
    for (const auto& coefficient : matrix.getCoefficients())
        squaredNorm += std::norm(coefficient);
    return std::sqrt(squaredNorm);
}

ComplexMatrix TgnMimoChannel::identity(int size)
{
    ComplexMatrix result(size, size);
    for (int i = 0; i < size; i++)
        result.get(i, i) = 1;
    return result;
}

ComplexMatrix TgnMimoChannel::add(const ComplexMatrix& left, const ComplexMatrix& right)
{
    if (left.getNumRows() != right.getNumRows() || left.getNumColumns() != right.getNumColumns())
        throw cRuntimeError("Cannot add TGn matrices with different dimensions");
    ComplexMatrix result(left.getNumRows(), left.getNumColumns());
    for (int row = 0; row < result.getNumRows(); row++)
        for (int column = 0; column < result.getNumColumns(); column++)
            result.get(row, column) = left.get(row, column) + right.get(row, column);
    return result;
}

ComplexMatrix TgnMimoChannel::scale(const ComplexMatrix& matrix, std::complex<double> factor)
{
    ComplexMatrix result(matrix.getNumRows(), matrix.getNumColumns());
    for (int row = 0; row < result.getNumRows(); row++)
        for (int column = 0; column < result.getNumColumns(); column++)
            result.get(row, column) = factor * matrix.get(row, column);
    return result;
}

ComplexMatrix TgnMimoChannel::multiply(const ComplexMatrix& left, const ComplexMatrix& right)
{
    if (left.getNumColumns() != right.getNumRows())
        throw cRuntimeError("Cannot multiply TGn matrices with inner dimensions %d and %d", left.getNumColumns(), right.getNumRows());
    ComplexMatrix result(left.getNumRows(), right.getNumColumns());
    for (int row = 0; row < result.getNumRows(); row++)
        for (int column = 0; column < result.getNumColumns(); column++) {
            std::complex<double> value = 0;
            for (int inner = 0; inner < left.getNumColumns(); inner++)
                value += left.get(row, inner) * right.get(inner, column);
            result.get(row, column) = value;
        }
    return result;
}

ComplexMatrix TgnMimoChannel::conjugateTranspose(const ComplexMatrix& matrix)
{
    ComplexMatrix result(matrix.getNumColumns(), matrix.getNumRows());
    for (int row = 0; row < matrix.getNumRows(); row++)
        for (int column = 0; column < matrix.getNumColumns(); column++)
            result.get(column, row) = std::conj(matrix.get(row, column));
    return result;
}

ComplexMatrix TgnMimoChannel::createSpatialCorrelation(int antennaCount, double spacingInWavelengths,
    double meanAngleDegrees, double angularSpreadDegrees)
{
    if (antennaCount < 1 || antennaCount > 8)
        throw cRuntimeError("TGn supports 1 through 8 antenna elements, got %d", antennaCount);
    if (!std::isfinite(spacingInWavelengths) || spacingInWavelengths <= 0 ||
        !std::isfinite(meanAngleDegrees) || !std::isfinite(angularSpreadDegrees) || angularSpreadDegrees <= 0)
        throw cRuntimeError("Invalid TGn ULA spacing or angular parameters");

    // INET policy: use a centered horizontal ULA and the positive steering
    // phase convention. Centering cancels from correlation position deltas.
    const double meanAngle = meanAngleDegrees * PI / 180.0;
    const double angularSpread = angularSpreadDegrees * PI / 180.0;
    auto density = [angularSpread](double offset) {
        return std::exp(-std::sqrt(2.0) * std::abs(offset) / angularSpread);
    };
    const double normalization = integrateCircularLaplacian([&](double offset) {
        return std::complex<double>(density(offset), 0);
    }).real();
    if (!std::isfinite(normalization) || normalization <= 0)
        throw cRuntimeError("TGn circular Laplacian normalization failed");

    ComplexMatrix result(antennaCount, antennaCount);
    for (int row = 0; row < antennaCount; row++) {
        for (int column = row; column < antennaCount; column++) {
            const double positionDifferenceInWavelengths = (row - column) * spacingInWavelengths;
            const double phaseScale = 2 * PI * positionDifferenceInWavelengths;
            std::complex<double> correlation = integrateCircularLaplacian([&](double offset) {
                const double phase = phaseScale * std::sin(offset + meanAngle);
                return density(offset) * std::exp(std::complex<double>(0, phase));
            }) / normalization;
            result.get(row, column) = correlation;
            result.get(column, row) = std::conj(correlation);
        }
    }
    for (int i = 0; i < antennaCount; i++) {
        if (std::abs(result.get(i, i) - std::complex<double>(1, 0)) > 1e-12)
            throw cRuntimeError("TGn correlation diagonal numerical error exceeds tolerance");
        result.get(i, i) = 1;
    }
    for (const auto& coefficient : result.getCoefficients())
        if (std::abs(coefficient) > 1 + 1e-12)
            throw cRuntimeError("TGn correlation magnitude exceeds one");
    return result;
}

ComplexMatrix TgnMimoChannel::principalSquareRoot(const ComplexMatrix& matrix)
{
    // INET policy: use the deterministic principal Hermitian PSD square root.
    requireSquare(matrix, "TGn principal square root");
    const int size = matrix.getNumRows();
    ComplexMatrix a = scale(add(matrix, conjugateTranspose(matrix)), 0.5);
    ComplexMatrix eigenvectors = identity(size);
    bool converged = size == 1;
    const int maximumSweeps = 100 * size * size;
    for (int sweep = 0; sweep < maximumSweeps && !converged; sweep++) {
        bool changed = false;
        const double threshold = std::numeric_limits<double>::epsilon() * size * frobeniusNorm(a);
        for (int p = 0; p < size; p++) {
            for (int q = p + 1; q < size; q++) {
                const std::complex<double> offDiagonal = a.get(p, q);
                const double magnitude = std::abs(offDiagonal);
                if (magnitude <= threshold)
                    continue;
                const std::complex<double> phase = offDiagonal / magnitude;
                const double app = a.get(p, p).real();
                const double aqq = a.get(q, q).real();
                const double angle = 0.5 * std::atan2(2 * magnitude, aqq - app);
                const double cosine = std::cos(angle);
                const double sine = std::sin(angle);
                ComplexMatrix rotation = identity(size);
                rotation.get(p, p) = cosine;
                rotation.get(p, q) = sine;
                rotation.get(q, p) = -std::conj(phase) * sine;
                rotation.get(q, q) = std::conj(phase) * cosine;
                a = multiply(multiply(conjugateTranspose(rotation), a), rotation);
                a = scale(add(a, conjugateTranspose(a)), 0.5);
                eigenvectors = multiply(eigenvectors, rotation);
                changed = true;
            }
        }
        converged = !changed;
    }
    if (!converged)
        throw cRuntimeError("Deterministic TGn Hermitian eigensolver failed to converge");

    std::vector<int> order(size);
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int left, int right) {
        return a.get(left, left).real() > a.get(right, right).real();
    });
    std::vector<double> eigenvalues(size);
    ComplexMatrix sortedEigenvectors(size, size);
    for (int column = 0; column < size; column++) {
        const int oldColumn = order[column];
        eigenvalues[column] = a.get(oldColumn, oldColumn).real();
        int pivot = 0;
        for (int row = 1; row < size; row++)
            if (std::abs(eigenvectors.get(row, oldColumn)) > std::abs(eigenvectors.get(pivot, oldColumn)))
                pivot = row;
        const std::complex<double> pivotValue = eigenvectors.get(pivot, oldColumn);
        const std::complex<double> phaseCorrection = std::abs(pivotValue) == 0 ? 1.0 : std::conj(pivotValue) / std::abs(pivotValue);
        for (int row = 0; row < size; row++)
            sortedEigenvectors.get(row, column) = eigenvectors.get(row, oldColumn) * phaseCorrection;
    }

    const ComplexMatrix residual = add(multiply(matrix, sortedEigenvectors),
        scale(multiply(sortedEigenvectors, [&]() {
            ComplexMatrix diagonal(size, size);
            for (int i = 0; i < size; i++)
                diagonal.get(i, i) = eigenvalues[i];
            return diagonal;
        }()), -1));
    if (frobeniusNorm(residual) > EIGEN_RESIDUAL_TOLERANCE * std::max(1.0, frobeniusNorm(matrix)))
        throw cRuntimeError("TGn correlation eigensolver residual exceeds tolerance");

    ComplexMatrix squareRootDiagonal(size, size);
    for (int i = 0; i < size; i++) {
        if (eigenvalues[i] < -PSD_TOLERANCE)
            throw cRuntimeError("TGn correlation matrix is not positive semidefinite (eigenvalue %g)", eigenvalues[i]);
        squareRootDiagonal.get(i, i) = std::sqrt(std::max(0.0, eigenvalues[i]));
    }
    ComplexMatrix result = multiply(multiply(sortedEigenvectors, squareRootDiagonal), conjugateTranspose(sortedEigenvectors));
    ComplexMatrix reconstructionError = add(multiply(result, conjugateTranspose(result)), scale(matrix, -1));
    if (frobeniusNorm(reconstructionError) > RECONSTRUCTION_TOLERANCE * std::max(1.0, frobeniusNorm(matrix)))
        throw cRuntimeError("TGn principal square-root reconstruction exceeds tolerance");
    return result;
}

ComplexMatrix TgnMimoChannel::createFixedLosMatrix(int numReceiveAntennas, int numTransmitAntennas,
    double receiverSpacingInWavelengths, double transmitterSpacingInWavelengths)
{
    if (numReceiveAntennas < 1 || numReceiveAntennas > 8 || numTransmitAntennas < 1 || numTransmitAntennas > 8 ||
        !std::isfinite(receiverSpacingInWavelengths) || receiverSpacingInWavelengths <= 0 ||
        !std::isfinite(transmitterSpacingInWavelengths) || transmitterSpacingInWavelengths <= 0)
        throw cRuntimeError("Invalid TGn LOS array dimensions or spacing");
    ComplexMatrix result(numReceiveAntennas, numTransmitAntennas);
    const double direction = std::sin(PI / 4);
    for (int row = 0; row < numReceiveAntennas; row++) {
        const double receiverPosition = (row - (numReceiveAntennas - 1) / 2.0) * receiverSpacingInWavelengths;
        const std::complex<double> receiverSteering = std::exp(std::complex<double>(0, 2 * PI * receiverPosition * direction));
        for (int column = 0; column < numTransmitAntennas; column++) {
            const double transmitterPosition = (column - (numTransmitAntennas - 1) / 2.0) * transmitterSpacingInWavelengths;
            const std::complex<double> transmitterSteering = std::exp(std::complex<double>(0, 2 * PI * transmitterPosition * direction));
            result.get(row, column) = receiverSteering * transmitterSteering;
        }
    }
    return result;
}

std::complex<double> TgnMimoChannel::evaluate(const TgnLorentzianProcess& process, simtime_t absoluteTime)
{
    if (process.oscillatorFrequenciesHz.empty() || process.oscillatorFrequenciesHz.size() != process.coefficients.size())
        throw cRuntimeError("Cannot evaluate an invalid TGn oscillator process");
    std::complex<double> result = 0;
    const double time = absoluteTime.dbl();
    for (size_t i = 0; i < process.coefficients.size(); i++) {
        const double phase = std::remainder(2 * PI * process.oscillatorFrequenciesHz[i] * time, 2 * PI);
        result += process.coefficients[i] * std::exp(std::complex<double>(0, phase));
    }
    return result / std::sqrt((double)process.coefficients.size());
}

std::complex<double> TgnMimoChannel::evaluate(const TgnTemporalProcess& process, simtime_t absoluteTime)
{
    if (process.terms.empty())
        throw cRuntimeError("Cannot evaluate an empty TGn temporal process");
    std::complex<double> result = 0;
    for (const auto& term : process.terms) {
        if (!std::isfinite(term.amplitude) || term.amplitude < 0)
            throw cRuntimeError("Invalid TGn temporal-process amplitude");
        result += term.amplitude * evaluate(term.process, absoluteTime);
    }
    return result;
}

std::complex<double> TgnMimoChannel::evaluateFluorescent(const TgnChannelRealization& realization, simtime_t absoluteTime)
{
    if (!realization.fluorescent)
        return 0;
    static const double amplitudes[] = {1, 0.1778279410038923, 0.1};
    std::complex<double> result = 0;
    for (int harmonic = 0; harmonic < 3; harmonic++) {
        const double frequency = 2 * (2 * harmonic + 1) * realization.fluorescentMainsFrequencyHz;
        const double phase = std::remainder(2 * PI * frequency * absoluteTime.dbl() + realization.fluorescentPhases[harmonic], 2 * PI);
        result += amplitudes[harmonic] * std::exp(std::complex<double>(0, phase));
    }
    return result;
}

ComplexMatrix TgnMimoChannel::evaluate(const TgnChannelRealization& realization, simtime_t absoluteTime, Hz frequency)
{
    if (realization.numReceiveAntennas < 1 || realization.numReceiveAntennas > 8 ||
        realization.numTransmitAntennas < 1 || realization.numTransmitAntennas > 8 ||
        !std::isfinite(realization.referenceFrequency.get()) || realization.referenceFrequency <= Hz(0) ||
        !std::isfinite(frequency.get()) || frequency <= Hz(0) ||
        !std::isfinite(realization.shadowingPowerGain) || realization.shadowingPowerGain <= 0 ||
        !std::isfinite(realization.smallScalePowerNormalization) || realization.smallScalePowerNormalization <= 0)
        throw cRuntimeError("Invalid TGn channel realization metadata");
    const simtime_t processTime = realization.timeVariation ? absoluteTime : SIMTIME_ZERO;
    const std::complex<double> fluorescent = realization.fluorescent ?
        realization.fluorescentScale * evaluateFluorescent(realization, processTime) : std::complex<double>(0, 0);
    ComplexMatrix response(realization.numReceiveAntennas, realization.numTransmitAntennas);
    int previousComponentIndex = -1;
    for (const auto& component : realization.components) {
        if (component.stableComponentIndex <= previousComponentIndex || component.excessDelay < SIMTIME_ZERO ||
            !std::isfinite(component.normalizedLinearPower) || component.normalizedLinearPower <= 0 ||
            component.receiverSquareRoot.getNumRows() != realization.numReceiveAntennas ||
            component.receiverSquareRoot.getNumColumns() != realization.numReceiveAntennas ||
            component.transmitterSquareRoot.getNumRows() != realization.numTransmitAntennas ||
            component.transmitterSquareRoot.getNumColumns() != realization.numTransmitAntennas ||
            (int)component.temporalProcesses.size() != realization.numReceiveAntennas * realization.numTransmitAntennas)
            throw cRuntimeError("Invalid TGn component realization at stable index %d", component.stableComponentIndex);
        previousComponentIndex = component.stableComponentIndex;
        ComplexMatrix independent(realization.numReceiveAntennas, realization.numTransmitAntennas);
        for (int row = 0; row < realization.numReceiveAntennas; row++)
            for (int column = 0; column < realization.numTransmitAntennas; column++)
                independent.get(row, column) = evaluate(component.temporalProcesses[row * realization.numTransmitAntennas + column], processTime);
        // INET policy: the Kronecker transmit factor is an ordinary transpose.
        ComplexMatrix spatial = multiply(multiply(component.receiverSquareRoot, independent), component.transmitterSquareRoot.transpose());
        if (component.fluorescent)
            spatial = scale(spatial, 1.0 + fluorescent);
        const double frequencyOffset = frequency.get() - realization.referenceFrequency.get();
        const double phase = std::remainder(-2 * PI * frequencyOffset * component.excessDelay.dbl(), 2 * PI);
        const std::complex<double> componentScale = std::sqrt(component.normalizedLinearPower) * std::exp(std::complex<double>(0, phase));
        response = add(response, scale(spatial, componentScale));
    }
    if (realization.los) {
        if (realization.fixedLosMatrix.getNumRows() != realization.numReceiveAntennas ||
            realization.fixedLosMatrix.getNumColumns() != realization.numTransmitAntennas ||
            !std::isfinite(realization.firstTapKLinear) || realization.firstTapKLinear < 0 ||
            !std::isfinite(realization.firstTapDiffusePower) || realization.firstTapDiffusePower <= 0)
            throw cRuntimeError("Invalid TGn LOS realization");
        response = add(response, scale(realization.fixedLosMatrix,
            std::sqrt(realization.firstTapKLinear * realization.firstTapDiffusePower)));
    }
    response = scale(response, realization.smallScalePowerNormalization * std::sqrt(realization.shadowingPowerGain));
    if (!response.isFinite())
        throw cRuntimeError("TGn channel evaluation produced a nonfinite coefficient");
    return response;
}

Hz TgnMimoChannel::getActualMaximumTemporalFrequency(const TgnChannelRealization& realization)
{
    if (!realization.timeVariation)
        return Hz(0);
    double maximum = 0;
    for (const auto& component : realization.components)
        for (const auto& temporalProcess : component.temporalProcesses)
            for (const auto& term : temporalProcess.terms)
                for (double frequency : term.process.oscillatorFrequenciesHz)
                    maximum = std::max(maximum, std::abs(frequency));
    if (realization.fluorescent)
        maximum += 10 * realization.fluorescentMainsFrequencyHz;
    return Hz(maximum);
}

} // namespace physicallayer
} // namespace inet
