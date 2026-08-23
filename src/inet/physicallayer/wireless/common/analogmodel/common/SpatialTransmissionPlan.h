//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_SPATIALTRANSMISSIONPLAN_H
#define __INET_SPATIALTRANSMISSIONPLAN_H

#include <memory>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h"

namespace inet {
namespace physicallayer {

class SpaceTimeCodeDescriptor;

class INET_API SpatialTransmissionPlan final
{
  public:
    enum class BoundarySide {
        LEFT_LIMIT,
        RIGHT_LIMIT
    };

    /**
     * Immutable non-STBC spatial transmission description for one relative
     * time segment.
     *
     * The mapping is transmit-antenna by space-time-stream. In this first
     * technology-neutral form the number of space-time streams equals the
     * number of spatial streams and the mapping is semi-unitary, which makes
     * the unit-trace stream covariance a unit-power transmit covariance.
     */
    class INET_API Segment final
    {
      private:
        simtime_t startOffset;
        simtime_t endOffset;
        int numberOfSpatialStreams;
        int numberOfSpaceTimeStreams;
        ComplexMatrix transmitMapping;
        std::vector<simtime_t> cyclicShiftDelays;
        std::vector<double> symbolPowerFractions;
        ComplexMatrix spaceTimeStreamCovariance;
        std::shared_ptr<const SpaceTimeCodeDescriptor> spaceTimeCodeDescriptor;
        simtime_t spaceTimeCodeSlotDuration;

      public:
        Segment(simtime_t startOffset, simtime_t endOffset,
            int numberOfSpatialStreams, int numberOfSpaceTimeStreams,
            const ComplexMatrix& transmitMapping, const std::vector<double>& symbolPowerFractions,
            const std::vector<simtime_t>& cyclicShiftDelays = {},
            const std::shared_ptr<const SpaceTimeCodeDescriptor>& spaceTimeCodeDescriptor = nullptr,
            simtime_t spaceTimeCodeSlotDuration = SIMTIME_ZERO);

        simtime_t getStartOffset() const { return startOffset; }
        simtime_t getEndOffset() const { return endOffset; }
        int getNumberOfSpatialStreams() const { return numberOfSpatialStreams; }
        int getNumberOfSpaceTimeStreams() const { return numberOfSpaceTimeStreams; }
        /** Returns the base mapping at zero baseband frequency. */
        const ComplexMatrix& getTransmitMapping() const { return transmitMapping; }
        /** Returns the frequency-resolved mapping Q(f) = Q(0)D(f). */
        ComplexMatrix getTransmitMapping(Hz basebandFrequency) const;
        const std::vector<simtime_t>& getCyclicShiftDelays() const { return cyclicShiftDelays; }
        const std::vector<double>& getSymbolPowerFractions() const { return symbolPowerFractions; }
        const ComplexMatrix& getSpaceTimeStreamCovariance() const { return spaceTimeStreamCovariance; }
        const std::shared_ptr<const SpaceTimeCodeDescriptor>& getSpaceTimeCodeDescriptor() const {
            return spaceTimeCodeDescriptor;
        }
        bool hasSpaceTimeCode() const { return spaceTimeCodeDescriptor != nullptr; }
        simtime_t getSpaceTimeCodeSlotDuration() const { return spaceTimeCodeSlotDuration; }

        /** Returns the resolved transmit-antenna covariance Q Csts Q^H. */
        ComplexMatrix getTransmitCovariance() const;
        /** Returns Q(f) Csts Q(f)^H at the requested baseband frequency. */
        ComplexMatrix getTransmitCovariance(Hz basebandFrequency) const;
    };

  private:
    int numberOfTransmitAntennas;
    std::vector<Segment> segments;

    void validateSegment(const Segment& segment, int index) const;

  public:
    SpatialTransmissionPlan(int numberOfTransmitAntennas, const std::vector<Segment>& segments);
    explicit SpatialTransmissionPlan(const std::vector<Segment>& segments);

    int getNumberOfTransmitAntennas() const { return numberOfTransmitAntennas; }
    const std::vector<Segment>& getSegments() const { return segments; }
    /**
     * Validates that the segments cover the complete half-open interval [0, duration).
     *
     * The plan itself may remain partial so that callers can build it incrementally;
     * this method is the explicit validation gate used before attaching it to a
     * transmission.
     */
    void validateCompleteCoverage(simtime_t duration) const;

    const Segment& getSegment(int index) const;
    const Segment& getSegmentAt(simtime_t offset, BoundarySide side) const;
    const Segment& getSegmentAt(simtime_t offset) const;
};

} // namespace physicallayer
} // namespace inet

#endif
