//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXSNAPSHOT_H
#define __INET_CHANNELMATRIXSNAPSHOT_H

#include "inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixResponse.h"

namespace inet {
namespace physicallayer {

class INET_API ChannelMatrixSnapshot : public IChannelMatrixSnapshot
{
  protected:
    int numReceiveAntennas;
    int numTransmitAntennas;
    Hz referenceFrequency;
    simtime_t startTime;
    simtime_t endTime;
    double shadowingPowerGain;
    Hz actualMaximumTemporalFrequency;
    simtime_t maximumExcessDelay;
    std::shared_ptr<const ChannelMatrixResponse> response;

  public:
    ChannelMatrixSnapshot(int numReceiveAntennas, int numTransmitAntennas, Hz referenceFrequency,
        simtime_t startTime, simtime_t endTime, double shadowingPowerGain,
        Hz actualMaximumTemporalFrequency, simtime_t maximumExcessDelay,
        const std::shared_ptr<const ChannelMatrixResponse>& response);

    virtual int getNumReceiveAntennas() const override { return numReceiveAntennas; }
    virtual int getNumTransmitAntennas() const override { return numTransmitAntennas; }
    virtual Hz getReferenceFrequency() const override { return referenceFrequency; }
    virtual simtime_t getStartTime() const override { return startTime; }
    virtual simtime_t getEndTime() const override { return endTime; }
    virtual double getShadowingPowerGain() const override { return shadowingPowerGain; }
    virtual Hz getActualMaximumTemporalFrequency() const override { return actualMaximumTemporalFrequency; }
    virtual simtime_t getMaximumExcessDelay() const override { return maximumExcessDelay; }
    virtual ComplexMatrix getResponse(simtime_t absoluteTime, Hz frequency) const override;
    virtual std::shared_ptr<const IChannelMatrixSnapshot> transpose() const override;
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
