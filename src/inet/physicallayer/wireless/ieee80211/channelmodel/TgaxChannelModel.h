//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TGAXCHANNELMODEL_H
#define __INET_TGAXCHANNELMODEL_H

#include <map>
#include <memory>
#include <string>
#include <cstdint>

#include "inet/common/Module.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IWidebandChannelModel.h"
#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgaxChannelProfile.h"
#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgaxMimoChannel.h"
#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgaxSisoChannel.h"

namespace inet {

namespace physicallayer {

class INET_API TgaxChannelModel : public Module, public IWidebandChannelModel
{
  protected:
    struct LinkKey {
        int transmitterRadioId;
        int receiverRadioId;

        bool operator<(const LinkKey& other) const;
    };

    std::string channelModel;
    Hz systemBandwidth = Hz(NaN);
    bool reciprocal = true;
    int rngStream = 0;
    uint64_t realizationSeed = 0;
    Hz referenceFrequency = Hz(NaN);
    Hz frequencyResolution = Hz(NaN);
    bool timeVariation = false;
    mps environmentalSpeed = mps(NaN);
    int numDopplerOscillators = 0;
    simsec timeResolution = simsec(0);
    bool spatialChannel = false;
    std::unique_ptr<TgaxChannelProfile> profile;
    mutable std::map<LinkKey, std::shared_ptr<const TgaxSisoChannel>> channels;
    mutable std::map<LinkKey, std::shared_ptr<const TgaxMimoChannel>> matrixChannels;

  protected:
    virtual void initialize(int stage) override;
    virtual LinkKey makeLinkKey(int transmitterRadioId, int receiverRadioId) const;
    virtual std::shared_ptr<const TgaxSisoChannel> createChannel(const LinkKey& key) const;
    virtual const TgaxSisoChannel *getOrCreateChannel(int transmitterRadioId, int receiverRadioId) const;
    virtual std::shared_ptr<const TgaxMimoChannel> createMatrixChannel(const LinkKey& key,
            int numReceiveAntennas, int numTransmitAntennas) const;
    virtual std::shared_ptr<const TgaxMimoChannel> getOrCreateMatrixChannel(int transmitterRadioId,
            int receiverRadioId, int numTransmitAntennas, int numReceiveAntennas) const;
    virtual void validateTransmissionBand(Hz centerFrequency, Hz bandwidth) const;
    virtual Ptr<const IFunction<double, Domain<simsec, Hz>>> createPowerGain(const TgaxSisoChannel& channel,
            simtime_t startTime, simtime_t endTime, Hz centerFrequency, Hz bandwidth) const;

  public:
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
    virtual Ptr<const IChannelSnapshot> computeChannel(const IRadio *receiver, const ITransmission *transmission,
            const IArrival *arrival) const override;
};

} // namespace physicallayer

} // namespace inet

#endif
