//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/common/analogmodel/common/SpatialTransmissionPlan.h"

#include <cmath>
#include <complex>

#include "inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixAlgebra.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/SpaceTimeCodeDescriptor.h"

namespace inet {
namespace physicallayer {

namespace {

constexpr double SOURCE_FRACTION_TOLERANCE = 1e-12;
constexpr double MAPPING_TOLERANCE = 1e-10;
constexpr double TWO_PI = 6.283185307179586476925286766559;

int validatePositiveStreamCount(int count)
{
    if (count <= 0)
        throw cRuntimeError("Spatial transmission stream counts must be positive, got %d", count);
    return count;
}

std::vector<simtime_t> makeZeroCyclicShiftDelays(int count)
{
    return std::vector<simtime_t>(validatePositiveStreamCount(count), SIMTIME_ZERO);
}

void validateOffsetSegment(simtime_t startOffset, simtime_t endOffset)
{
    if (startOffset < SIMTIME_ZERO || startOffset >= endOffset)
        throw cRuntimeError("Spatial transmission segment must have nonnegative start and positive duration, got [%s, %s)",
            startOffset.str().c_str(), endOffset.str().c_str());
}

void validateSourceFractions(int numberOfSpatialStreams, const std::vector<double>& sourceFractions)
{
    if ((int)sourceFractions.size() != numberOfSpatialStreams)
        throw cRuntimeError("Spatial transmission source fraction count %zu does not match NSS %d",
            sourceFractions.size(), numberOfSpatialStreams);
    double sum = 0;
    for (int stream = 0; stream < numberOfSpatialStreams; stream++) {
        const double fraction = sourceFractions[stream];
        if (!std::isfinite(fraction) || fraction < 0)
            throw cRuntimeError("Spatial transmission source fraction %d is invalid: %g", stream, fraction);
        sum += fraction;
    }
    if (!std::isfinite(sum) || std::abs(sum - 1) > SOURCE_FRACTION_TOLERANCE)
        throw cRuntimeError("Spatial transmission source fractions must sum to one, got %.17g", sum);
}

void validateSemiUnitaryMapping(const ComplexMatrix& mapping, int numberOfSpaceTimeStreams)
{
    if (mapping.getNumColumns() != numberOfSpaceTimeStreams)
        throw cRuntimeError("Spatial transmission mapping has %d columns instead of NSTS %d",
            mapping.getNumColumns(), numberOfSpaceTimeStreams);
    ChannelMatrixAlgebra::validateFinite(mapping, "Spatial transmission mapping");
    if (ChannelMatrixAlgebra::computeRank(mapping, MAPPING_TOLERANCE) != numberOfSpaceTimeStreams)
        throw cRuntimeError("Spatial transmission mapping must have full column rank");
    const ComplexMatrix gram = ChannelMatrixAlgebra::hermitianProduct(mapping);
    for (int row = 0; row < numberOfSpaceTimeStreams; row++)
        for (int column = 0; column < numberOfSpaceTimeStreams; column++) {
            const std::complex<double> expected = row == column ? std::complex<double>(1, 0) : std::complex<double>(0, 0);
            if (std::abs(gram.get(row, column) - expected) > MAPPING_TOLERANCE)
                throw cRuntimeError("Spatial transmission mapping must be semi-unitary; Q^H Q[%d,%d] is %g%+gj",
                    row, column, gram.get(row, column).real(), gram.get(row, column).imag());
        }
}

} // namespace

SpatialTransmissionPlan::Segment::Segment(simtime_t startOffset, simtime_t endOffset,
    int numberOfSpatialStreams, int numberOfSpaceTimeStreams,
    const ComplexMatrix& transmitMapping, const std::vector<double>& symbolPowerFractions,
    const std::vector<simtime_t>& cyclicShiftDelays,
    const std::shared_ptr<const SpaceTimeCodeDescriptor>& spaceTimeCodeDescriptor,
    simtime_t spaceTimeCodeSlotDuration) :
    startOffset(startOffset), endOffset(endOffset),
    numberOfSpatialStreams(numberOfSpatialStreams), numberOfSpaceTimeStreams(numberOfSpaceTimeStreams),
    transmitMapping(transmitMapping),
    cyclicShiftDelays(cyclicShiftDelays.empty() ? makeZeroCyclicShiftDelays(numberOfSpaceTimeStreams) : cyclicShiftDelays),
    symbolPowerFractions(symbolPowerFractions),
    spaceTimeStreamCovariance(validatePositiveStreamCount(numberOfSpaceTimeStreams),
        validatePositiveStreamCount(numberOfSpaceTimeStreams)),
    spaceTimeCodeDescriptor(spaceTimeCodeDescriptor),
    spaceTimeCodeSlotDuration(spaceTimeCodeSlotDuration)
{
    validateOffsetSegment(startOffset, endOffset);
    if (numberOfSpatialStreams <= 0 || numberOfSpaceTimeStreams <= 0)
        throw cRuntimeError("Spatial transmission stream counts must be positive, got NSS=%d NSTS=%d",
            numberOfSpatialStreams, numberOfSpaceTimeStreams);
    if (!spaceTimeCodeDescriptor && numberOfSpatialStreams != numberOfSpaceTimeStreams)
        throw cRuntimeError("Non-STBC spatial transmission requires NSS=NSTS, got NSS=%d NSTS=%d",
            numberOfSpatialStreams, numberOfSpaceTimeStreams);
    if (transmitMapping.getNumRows() <= 0)
        throw cRuntimeError("Spatial transmission mapping must have at least one transmit antenna");
    if ((int)this->cyclicShiftDelays.size() != numberOfSpaceTimeStreams)
        throw cRuntimeError("Spatial transmission cyclic-shift delay count %zu does not match NSTS %d",
            this->cyclicShiftDelays.size(), numberOfSpaceTimeStreams);
    for (int stream = 0; stream < numberOfSpaceTimeStreams; stream++) {
        if (!std::isfinite(this->cyclicShiftDelays[stream].dbl()))
            throw cRuntimeError("Spatial transmission cyclic-shift delay %d is not finite", stream);
    }
    validateSourceFractions(numberOfSpatialStreams, symbolPowerFractions);
    validateSemiUnitaryMapping(transmitMapping, numberOfSpaceTimeStreams);

    if (spaceTimeCodeDescriptor) {
        if (spaceTimeCodeSlotDuration < SIMTIME_ZERO)
            throw cRuntimeError("Space-time code slot duration must be nonnegative");
        if (spaceTimeCodeSlotDuration > SIMTIME_ZERO) {
            const simtime_t segmentDuration = endOffset - startOffset;
            const int64_t numberOfSlots = segmentDuration.raw() / spaceTimeCodeSlotDuration.raw();
            if (numberOfSlots <= 0 || segmentDuration.raw() % spaceTimeCodeSlotDuration.raw() != 0 ||
                numberOfSlots % spaceTimeCodeDescriptor->getNumberOfSlots() != 0)
                throw cRuntimeError("Space-time coded segment duration must contain a whole number of code blocks");
        }
        if (spaceTimeCodeDescriptor->getNumberOfSpatialStreams() != numberOfSpatialStreams ||
            spaceTimeCodeDescriptor->getNumberOfSpaceTimeStreams() != numberOfSpaceTimeStreams)
            throw cRuntimeError("Space-time descriptor dimensions disagree with spatial transmission segment");
        spaceTimeStreamCovariance = spaceTimeCodeDescriptor->computeSlotSpaceTimeStreamCovariance(0);
        for (int slot = 1; slot < spaceTimeCodeDescriptor->getNumberOfSlots(); slot++) {
            const auto slotCovariance = spaceTimeCodeDescriptor->computeSlotSpaceTimeStreamCovariance(slot);
            for (int row = 0; row < numberOfSpaceTimeStreams; row++)
                for (int column = 0; column < numberOfSpaceTimeStreams; column++)
                    if (std::abs(slotCovariance.get(row, column) -
                        spaceTimeStreamCovariance.get(row, column)) > MAPPING_TOLERANCE)
                        throw cRuntimeError("Space-time code slot covariances must match for physical materialization");
        }
    }
    else {
        if (spaceTimeCodeSlotDuration != SIMTIME_ZERO)
            throw cRuntimeError("A space-time code slot duration requires a code descriptor");
        for (int stream = 0; stream < numberOfSpaceTimeStreams; stream++)
            spaceTimeStreamCovariance.get(stream, stream) = symbolPowerFractions[stream];
    }
    ChannelMatrixAlgebra::validateHermitian(spaceTimeStreamCovariance,
        MAPPING_TOLERANCE, "Spatial transmission stream covariance");
}

ComplexMatrix SpatialTransmissionPlan::Segment::getTransmitMapping(Hz basebandFrequency) const
{
    const double frequency = basebandFrequency.get<Hz>();
    if (!std::isfinite(frequency))
        throw cRuntimeError("Spatial transmission baseband frequency must be finite, got %g", frequency);
    ComplexMatrix resolved = transmitMapping;
    for (int row = 0; row < resolved.getNumRows(); row++)
        for (int stream = 0; stream < resolved.getNumColumns(); stream++) {
            const double phase = -TWO_PI * frequency * cyclicShiftDelays[stream].dbl();
            const std::complex<double> rotation = std::polar(1.0, phase);
            resolved.get(row, stream) *= rotation;
        }
    // The diagonal phase rotation is unit magnitude, so this is also a
    // useful guard against non-finite results at extreme frequencies.
    validateSemiUnitaryMapping(resolved, numberOfSpaceTimeStreams);
    return resolved;
}

ComplexMatrix SpatialTransmissionPlan::Segment::getTransmitCovariance() const
{
    return ChannelMatrixAlgebra::multiply(
        ChannelMatrixAlgebra::multiply(transmitMapping, spaceTimeStreamCovariance),
        ChannelMatrixAlgebra::conjugateTranspose(transmitMapping));
}

ComplexMatrix SpatialTransmissionPlan::Segment::getTransmitCovariance(Hz basebandFrequency) const
{
    const ComplexMatrix resolved = getTransmitMapping(basebandFrequency);
    return ChannelMatrixAlgebra::multiply(
        ChannelMatrixAlgebra::multiply(resolved, spaceTimeStreamCovariance),
        ChannelMatrixAlgebra::conjugateTranspose(resolved));
}

SpatialTransmissionPlan::SpatialTransmissionPlan(int numberOfTransmitAntennas,
    const std::vector<SpatialTransmissionPlan::Segment>& segments) :
    numberOfTransmitAntennas(numberOfTransmitAntennas), segments(segments)
{
    if (numberOfTransmitAntennas <= 0)
        throw cRuntimeError("Spatial transmission plan requires a positive transmit antenna count, got %d",
            numberOfTransmitAntennas);
    if (segments.empty())
        throw cRuntimeError("Spatial transmission plan requires at least one segment");
    for (size_t index = 0; index < segments.size(); index++) {
        validateSegment(segments[index], index);
        if (index > 0 && segments[index - 1].getEndOffset() > segments[index].getStartOffset())
            throw cRuntimeError("Spatial transmission segments %zu and %zu overlap or are out of order",
                index - 1, index);
    }
}

SpatialTransmissionPlan::SpatialTransmissionPlan(const std::vector<SpatialTransmissionPlan::Segment>& segments) :
    SpatialTransmissionPlan(segments.empty() ? 0 : segments.front().getTransmitMapping().getNumRows(), segments)
{
}

void SpatialTransmissionPlan::validateSegment(const SpatialTransmissionPlan::Segment& segment, int index) const
{
    if (segment.getTransmitMapping().getNumRows() != numberOfTransmitAntennas)
        throw cRuntimeError("Spatial transmission segment %d has %d transmit antennas instead of %d",
            index, segment.getTransmitMapping().getNumRows(), numberOfTransmitAntennas);
}

void SpatialTransmissionPlan::validateCompleteCoverage(simtime_t duration) const
{
    if (duration <= SIMTIME_ZERO)
        throw cRuntimeError("Spatial transmission plan duration must be positive, got %s", duration.str().c_str());
    if (segments.empty())
        throw cRuntimeError("Spatial transmission plan requires at least one segment for complete coverage");
    if (segments.front().getStartOffset() != SIMTIME_ZERO)
        throw cRuntimeError("Spatial transmission plan must start at zero, got %s",
            segments.front().getStartOffset().str().c_str());
    for (size_t index = 1; index < segments.size(); index++) {
        if (segments[index - 1].getEndOffset() != segments[index].getStartOffset())
            throw cRuntimeError("Spatial transmission plan segments %zu and %zu are not exactly adjacent",
                index - 1, index);
    }
    if (segments.back().getEndOffset() != duration)
        throw cRuntimeError("Spatial transmission plan ends at %s instead of duration %s",
            segments.back().getEndOffset().str().c_str(), duration.str().c_str());
}

const SpatialTransmissionPlan::Segment& SpatialTransmissionPlan::getSegment(int index) const
{
    if (index < 0 || index >= (int)segments.size())
        throw cRuntimeError("Spatial transmission segment index %d is outside %zu segments", index, segments.size());
    return segments[index];
}

const SpatialTransmissionPlan::Segment& SpatialTransmissionPlan::getSegmentAt(simtime_t offset, BoundarySide side) const
{
    if (side == BoundarySide::RIGHT_LIMIT) {
        for (const auto& segment : segments)
            if (offset >= segment.getStartOffset() && offset < segment.getEndOffset())
                return segment;
    }
    else if (side == BoundarySide::LEFT_LIMIT) {
        for (const auto& segment : segments)
            if (offset > segment.getStartOffset() && offset <= segment.getEndOffset())
                return segment;
    }
    else
        throw cRuntimeError("Invalid spatial transmission boundary side %d", (int)side);
    throw cRuntimeError("No spatial transmission segment covers offset %s at the %s limit", offset.str().c_str(),
        side == BoundarySide::LEFT_LIMIT ? "left" : "right");
}

const SpatialTransmissionPlan::Segment& SpatialTransmissionPlan::getSegmentAt(simtime_t offset) const
{
    return getSegmentAt(offset, BoundarySide::RIGHT_LIMIT);
}

} // namespace physicallayer
} // namespace inet
