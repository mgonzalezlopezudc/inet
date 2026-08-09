//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211TRANSMISSION_H
#define __INET_IEEE80211TRANSMISSION_H

#include "inet/physicallayer/wireless/common/base/packetlevel/TransmissionBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Channel.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtTxVector.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtTxVector.h"

namespace inet {

namespace physicallayer {

class INET_API Ieee80211Transmission : public TransmissionBase
{
  protected:
    const IIeee80211Mode *mode;
    const Ieee80211Channel *channel;
    const std::shared_ptr<const Ieee80211HeTxVector> heTxVector;
    const std::shared_ptr<const Ieee80211HePpduLayout> hePpduLayout;
    const uint32_t heTriggerCorrelationId;
    const std::shared_ptr<const Ieee80211VhtTxVector> vhtTxVector;
    const std::shared_ptr<const Ieee80211HtTxVector> htTxVector;

  public:
    Ieee80211Transmission(const IRadio *transmitter, const Packet *packet, const simtime_t startTime, const simtime_t endTime, const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, const Coord startPosition, const Coord endPosition, const Quaternion startOrientation, const Quaternion endOrientation, const ITransmissionPacketModel *packetModel, const ITransmissionBitModel *bitModel, const ITransmissionSymbolModel *symbolModel, const ITransmissionSampleModel *sampleModel, const ITransmissionAnalogModel *analogModel, const IIeee80211Mode *mode, const Ieee80211Channel *channel, std::shared_ptr<const Ieee80211HeTxVector> heTxVector = {}, std::shared_ptr<const Ieee80211HePpduLayout> hePpduLayout = {}, uint32_t heTriggerCorrelationId = 0, std::shared_ptr<const Ieee80211VhtTxVector> vhtTxVector = {}, std::shared_ptr<const Ieee80211HtTxVector> htTxVector = {});

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual const IIeee80211Mode *getMode() const { return mode; }
    virtual const Ieee80211Channel *getChannel() const { return channel; }
    virtual const std::shared_ptr<const Ieee80211HeTxVector>& getHeTxVector() const { return heTxVector; }
    virtual const std::shared_ptr<const Ieee80211HePpduLayout>& getHePpduLayout() const { return hePpduLayout; }
    virtual uint32_t getHeTriggerCorrelationId() const { return heTriggerCorrelationId; }
    virtual const std::shared_ptr<const Ieee80211VhtTxVector>& getVhtTxVector() const { return vhtTxVector; }
    virtual const std::shared_ptr<const Ieee80211HtTxVector>& getHtTxVector() const { return htTxVector; }
};

} // namespace physicallayer

} // namespace inet

#endif
