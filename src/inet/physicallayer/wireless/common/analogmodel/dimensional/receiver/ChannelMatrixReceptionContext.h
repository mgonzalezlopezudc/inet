//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXRECEPTIONCONTEXT_H
#define __INET_CHANNELMATRIXRECEPTIONCONTEXT_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixAlgebra.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/SpatialTransmissionPlan.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioSignal.h"

namespace inet {
namespace physicallayer {

/**
 * Immutable inputs for technology-neutral matrix receiver processing.
 * ComplexMatrix covariance entries are numeric W/Hz values; PSDs crossing
 * this value-type boundary use WpHz explicitly.
 */
class INET_API ChannelMatrixReceptionContext final
{
  public:
    class INET_API Signal final
    {
      private:
        ComplexMatrix response;
        SpatialTransmissionPlan::Segment spatialTransmissionSegment;
        WpHz largeScalePowerSpectralDensity;
        Hz basebandFrequency;
        int transmissionId;
        int64_t spaceTimeCodeBlockId;
        int spaceTimeCodeSlotIndex;

      public:
        Signal(const ComplexMatrix& response,
            const SpatialTransmissionPlan::Segment& spatialTransmissionSegment,
            WpHz largeScalePowerSpectralDensity,
            Hz basebandFrequency = Hz(0), int transmissionId = -1,
            int64_t spaceTimeCodeBlockId = -1, int spaceTimeCodeSlotIndex = -1);

        const ComplexMatrix& getResponse() const { return response; }
        const SpatialTransmissionPlan::Segment& getSpatialTransmissionSegment() const { return spatialTransmissionSegment; }
        WpHz getLargeScalePowerSpectralDensity() const { return largeScalePowerSpectralDensity; }
        Hz getBasebandFrequency() const { return basebandFrequency; }
        int getTransmissionId() const { return transmissionId; }
        int64_t getSpaceTimeCodeBlockId() const { return spaceTimeCodeBlockId; }
        int getSpaceTimeCodeSlotIndex() const { return spaceTimeCodeSlotIndex; }
        bool hasCorrelatedSpaceTimeCodeIdentity() const {
            return transmissionId >= 0 && spaceTimeCodeBlockId >= 0 && spaceTimeCodeSlotIndex >= 0;
        }

        /** Computes A = H Q diag(sqrt(P p)) for this signal. */
        ComplexMatrix getEffectiveChannel() const;

        /** Computes sqrt(P) H Q for a descriptor-owned space-time code. */
        ComplexMatrix getEffectiveSpaceTimeStreamChannel() const;

        /** Computes P H Q C Q^H H^H, numerically in W/Hz. */
        ComplexMatrix getReceiveCovariance() const;
    };

  private:
    Signal desiredSignal;
    std::vector<Signal> interferingSignals;
    ComplexMatrix backgroundCovariance;
    simtime_t time;
    Hz frequency;
    IRadioSignal::SignalPart signalPart;

    void validateAggregateDimensions() const;

  public:
    ChannelMatrixReceptionContext(const Signal& desiredSignal,
        const std::vector<Signal>& interferingSignals, const ComplexMatrix& backgroundCovariance,
        simtime_t time, Hz frequency, IRadioSignal::SignalPart signalPart);

    const Signal& getDesiredSignal() const { return desiredSignal; }
    const std::vector<Signal>& getInterferingSignals() const { return interferingSignals; }
    const ComplexMatrix& getBackgroundCovariance() const { return backgroundCovariance; }
    simtime_t getTime() const { return time; }
    Hz getFrequency() const { return frequency; }
    IRadioSignal::SignalPart getSignalPart() const { return signalPart; }

    ComplexMatrix getEffectiveDesiredChannel() const;
    ComplexMatrix getInterferencePlusNoiseCovariance() const;
    ComplexMatrix getSelectedEffectiveDesiredChannel(const std::vector<int>& selectedReceiveRows) const;
    ComplexMatrix getSelectedInterferencePlusNoiseCovariance(const std::vector<int>& selectedReceiveRows) const;
};

} // namespace physicallayer
} // namespace inet

#endif
