//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixReceptionContext.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include "inet/physicallayer/wireless/common/analogmodel/common/SpaceTimeCodeDescriptor.h"

namespace inet {
namespace physicallayer {

namespace {

void validatePositiveSemidefinite(const ComplexMatrix& matrix, const char *operation)
{
    ChannelMatrixAlgebra::validateSquare(matrix, operation);
    ChannelMatrixAlgebra::validateHermitian(matrix,
        ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE, operation);
    const int size = matrix.getNumRows();
    double maximumMagnitude = 0;
    for (const auto& coefficient : matrix.getCoefficients())
        maximumMagnitude = std::max(maximumMagnitude, std::abs(coefficient));
    const double tolerance = ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE * maximumMagnitude;
    std::vector<std::complex<double>> lower(size * size, std::complex<double>(0, 0));
    auto getLower = [&](int row, int column) -> std::complex<double>& {
        return lower[row * size + column];
    };
    for (int row = 0; row < size; row++) {
        for (int column = 0; column <= row; column++) {
            std::complex<double> value = matrix.get(row, column);
            for (int inner = 0; inner < column; inner++)
                value -= getLower(row, inner) * std::conj(getLower(column, inner));
            if (column == row) {
                if (std::abs(value.imag()) > tolerance || value.real() < -tolerance)
                    throw cRuntimeError("%s is not positive semidefinite", operation);
                if (value.real() <= tolerance)
                    getLower(row, column) = 0;
                else
                    getLower(row, column) = std::sqrt(value.real());
            }
            else {
                const double diagonal = getLower(column, column).real();
                if (diagonal <= 0) {
                    if (std::abs(value) > tolerance)
                        throw cRuntimeError("%s is not positive semidefinite", operation);
                    getLower(row, column) = 0;
                }
                else
                    getLower(row, column) = value / diagonal;
            }
        }
    }
}

void validateSignalPart(IRadioSignal::SignalPart signalPart)
{
    if (signalPart < IRadioSignal::SIGNAL_PART_NONE || signalPart > IRadioSignal::SIGNAL_PART_DATA)
        throw cRuntimeError("Channel-matrix reception context has an invalid signal part %d", (int)signalPart);
}

void validateRows(const std::vector<int>& rows, int count)
{
    if (rows.empty())
        throw cRuntimeError("Selected receive rows must be nonempty");
    int previous = -1;
    for (int row : rows) {
        if (row < 0 || row >= count)
            throw cRuntimeError("Selected receive row %d is outside [0,%d)", row, count);
        if (row <= previous)
            throw cRuntimeError("Selected receive rows must be strictly increasing and unique");
        previous = row;
    }
}

} // namespace

ChannelMatrixReceptionContext::Signal::Signal(const ComplexMatrix& response,
    const SpatialTransmissionPlan::Segment& spatialTransmissionSegment,
    WpHz largeScalePowerSpectralDensity, Hz basebandFrequency,
    int transmissionId, int64_t spaceTimeCodeBlockId, int spaceTimeCodeSlotIndex) :
    response(response), spatialTransmissionSegment(spatialTransmissionSegment),
    largeScalePowerSpectralDensity(largeScalePowerSpectralDensity), basebandFrequency(basebandFrequency),
    transmissionId(transmissionId), spaceTimeCodeBlockId(spaceTimeCodeBlockId),
    spaceTimeCodeSlotIndex(spaceTimeCodeSlotIndex)
{
    ChannelMatrixAlgebra::validateFinite(response, "Channel-matrix response");
    const double power = largeScalePowerSpectralDensity.get<WpHz>();
    if (!std::isfinite(power) || power < 0)
        throw cRuntimeError("Large-scale signal PSD must be finite and nonnegative, got %g", power);
    if (!std::isfinite(basebandFrequency.get<Hz>()))
        throw cRuntimeError("Channel-matrix baseband frequency must be finite");
    if (response.getNumColumns() != spatialTransmissionSegment.getTransmitMapping().getNumRows())
        throw cRuntimeError("Channel response has %d transmit columns instead of mapping antenna count %d",
            response.getNumColumns(), spatialTransmissionSegment.getTransmitMapping().getNumRows());
    const bool hasAnyIdentity = transmissionId >= 0 || spaceTimeCodeBlockId >= 0 || spaceTimeCodeSlotIndex >= 0;
    if (hasAnyIdentity && (transmissionId < 0 || spaceTimeCodeBlockId < 0 ||
        spaceTimeCodeSlotIndex < 0 || !spatialTransmissionSegment.hasSpaceTimeCode()))
        throw cRuntimeError("Correlated space-time signal identity must be complete and descriptor-owned");
    if (hasAnyIdentity && spaceTimeCodeSlotIndex >=
        spatialTransmissionSegment.getSpaceTimeCodeDescriptor()->getNumberOfSlots())
        throw cRuntimeError("Correlated space-time signal slot index is outside its descriptor");
}

ComplexMatrix ChannelMatrixReceptionContext::Signal::getEffectiveChannel() const
{
    if (spatialTransmissionSegment.hasSpaceTimeCode())
        throw cRuntimeError("Ordinary effective-channel construction cannot scalarize a space-time coded segment");
    const ComplexMatrix mapped = ChannelMatrixAlgebra::multiply(response,
        spatialTransmissionSegment.getTransmitMapping(basebandFrequency));
    ComplexMatrix result(mapped.getNumRows(), mapped.getNumColumns());
    const double power = largeScalePowerSpectralDensity.get<WpHz>();
    const auto& fractions = spatialTransmissionSegment.getSymbolPowerFractions();
    for (int row = 0; row < mapped.getNumRows(); row++)
        for (int column = 0; column < mapped.getNumColumns(); column++)
            result.get(row, column) = mapped.get(row, column) * std::sqrt(power * fractions[column]);
    ChannelMatrixAlgebra::validateFinite(result, "Effective channel");
    return result;
}

ComplexMatrix ChannelMatrixReceptionContext::Signal::getEffectiveSpaceTimeStreamChannel() const
{
    if (!spatialTransmissionSegment.hasSpaceTimeCode())
        throw cRuntimeError("Space-time effective-channel construction requires a code descriptor");
    const ComplexMatrix mapped = ChannelMatrixAlgebra::multiply(response,
        spatialTransmissionSegment.getTransmitMapping(basebandFrequency));
    return ChannelMatrixAlgebra::scale(mapped,
        std::complex<double>(std::sqrt(largeScalePowerSpectralDensity.get<WpHz>()), 0));
}

ComplexMatrix ChannelMatrixReceptionContext::Signal::getReceiveCovariance() const
{
    const ComplexMatrix mapped = ChannelMatrixAlgebra::multiply(response,
        spatialTransmissionSegment.getTransmitMapping(basebandFrequency));
    const ComplexMatrix mappedCovariance = ChannelMatrixAlgebra::multiply(
        ChannelMatrixAlgebra::multiply(mapped, spatialTransmissionSegment.getSpaceTimeStreamCovariance()),
        ChannelMatrixAlgebra::conjugateTranspose(mapped));
    return ChannelMatrixAlgebra::scale(mappedCovariance,
        std::complex<double>(largeScalePowerSpectralDensity.get<WpHz>(), 0));
}

ChannelMatrixReceptionContext::ChannelMatrixReceptionContext(const Signal& desiredSignal,
    const std::vector<Signal>& interferingSignals, const ComplexMatrix& backgroundCovariance,
    simtime_t time, Hz frequency, IRadioSignal::SignalPart signalPart) :
    desiredSignal(desiredSignal), interferingSignals(interferingSignals), backgroundCovariance(backgroundCovariance),
    time(time), frequency(frequency), signalPart(signalPart)
{
    if (!std::isfinite(time.dbl()))
        throw cRuntimeError("Channel-matrix reception time must be finite");
    const double frequencyValue = frequency.get<Hz>();
    if (!std::isfinite(frequencyValue))
        throw cRuntimeError("Channel-matrix reception frequency must be finite");
    validateSignalPart(signalPart);
    validateAggregateDimensions();
    validatePositiveSemidefinite(backgroundCovariance, "Background covariance");
    // Evaluate once to validate the aggregate covariance without storing a
    // mutable cache.  Every later call recomputes the same pure value.
    getInterferencePlusNoiseCovariance();
}

void ChannelMatrixReceptionContext::validateAggregateDimensions() const
{
    const int numberOfReceiveAntennas = desiredSignal.getResponse().getNumRows();
    if (backgroundCovariance.getNumRows() != numberOfReceiveAntennas ||
        backgroundCovariance.getNumColumns() != numberOfReceiveAntennas)
        throw cRuntimeError("Background covariance dimensions do not match desired response receive rows");
    for (const auto& signal : interferingSignals)
        if (signal.getResponse().getNumRows() != numberOfReceiveAntennas)
            throw cRuntimeError("Interfering response receive rows do not match desired response");
}

ComplexMatrix ChannelMatrixReceptionContext::getEffectiveDesiredChannel() const
{
    return desiredSignal.getEffectiveChannel();
}

ComplexMatrix ChannelMatrixReceptionContext::getInterferencePlusNoiseCovariance() const
{
    ComplexMatrix result = backgroundCovariance;
    for (const auto& signal : interferingSignals)
        result = ChannelMatrixAlgebra::add(result, signal.getReceiveCovariance());
    ChannelMatrixAlgebra::validatePositiveDefinite(result,
        ChannelMatrixAlgebra::DEFAULT_RELATIVE_TOLERANCE,
        "Interference-plus-noise covariance");
    return result;
}

ComplexMatrix ChannelMatrixReceptionContext::getSelectedEffectiveDesiredChannel(
    const std::vector<int>& selectedReceiveRows) const
{
    validateRows(selectedReceiveRows, desiredSignal.getResponse().getNumRows());
    return ChannelMatrixAlgebra::selectRows(getEffectiveDesiredChannel(), selectedReceiveRows);
}

ComplexMatrix ChannelMatrixReceptionContext::getSelectedInterferencePlusNoiseCovariance(
    const std::vector<int>& selectedReceiveRows) const
{
    validateRows(selectedReceiveRows, backgroundCovariance.getNumRows());
    return ChannelMatrixAlgebra::selectRowsAndColumns(getInterferencePlusNoiseCovariance(), selectedReceiveRows);
}

} // namespace physicallayer
} // namespace inet
