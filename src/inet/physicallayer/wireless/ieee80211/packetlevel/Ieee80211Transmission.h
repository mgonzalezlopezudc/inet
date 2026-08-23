//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211TRANSMISSION_H
#define __INET_IEEE80211TRANSMISSION_H

#include <memory>

#include "inet/physicallayer/wireless/common/base/packetlevel/TransmissionBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ISpatialTransmission.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Channel.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtPpduDescription.h"

namespace inet {

namespace physicallayer {

class INET_API Ieee80211Transmission : public TransmissionBase, public ISpatialTransmission
{
  protected:
    const IIeee80211Mode *mode;
    const Ieee80211Channel *channel;
    std::shared_ptr<const SpatialTransmissionPlan> spatialTransmissionPlan;
    std::shared_ptr<const Ieee80211HtPpduDescription> htPpduDescription;

  public:
    static void validateSpatialMetadata(const IIeee80211Mode *mode,
        const Ieee80211Channel *channel, simtime_t duration,
        const std::shared_ptr<const SpatialTransmissionPlan>& spatialTransmissionPlan,
        const std::shared_ptr<const Ieee80211HtPpduDescription>& htPpduDescription);

    Ieee80211Transmission(const IRadio *transmitter, const Packet *packet, const simtime_t startTime, const simtime_t endTime, const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, const Coord startPosition, const Coord endPosition, const Quaternion startOrientation, const Quaternion endOrientation, const ITransmissionPacketModel *packetModel, const ITransmissionBitModel *bitModel, const ITransmissionSymbolModel *symbolModel, const ITransmissionSampleModel *sampleModel, const ITransmissionAnalogModel *analogModel, const IIeee80211Mode *mode, const Ieee80211Channel *channel, std::shared_ptr<const SpatialTransmissionPlan> spatialTransmissionPlan = nullptr, std::shared_ptr<const Ieee80211HtPpduDescription> htPpduDescription = nullptr);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual const IIeee80211Mode *getMode() const { return mode; }
    virtual const Ieee80211Channel *getChannel() const { return channel; }
    virtual const std::shared_ptr<const SpatialTransmissionPlan>& getSpatialTransmissionPlan() const override { return spatialTransmissionPlan; }
    const std::shared_ptr<const Ieee80211HtPpduDescription>& getHtPpduDescription() const { return htPpduDescription; }
};

} // namespace physicallayer

} // namespace inet

#endif
