//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211RECEIVER_H
#define __INET_IEEE80211RECEIVER_H

#include "inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixReceiver.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Channel.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtCapabilities.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/IIeee80211HtCapabilitiesConsumer.h"

namespace inet {

namespace physicallayer {

class Ieee80211Transmission;
class Ieee80211HtPpduDescription;
class Ieee80211OfdmMode;

class INET_API Ieee80211Receiver : public FlatReceiverBase,
    public virtual IChannelMatrixReceiver,
    public virtual IIeee80211HtCapabilitiesConsumer
{
  protected:
    const Ieee80211ModeSet *modeSet = nullptr;
    const IIeee80211Band *band = nullptr;
    const Ieee80211Channel *channel = nullptr;
    const IChannelMatrixReceptionProcessor *channelMatrixReceptionProcessor = nullptr;
    size_t maximumMaterializedResourceCells = 0;
    const Ieee80211HtCapabilities *htCapabilities = nullptr;

  protected:
    virtual void initialize(int stage) override;

    virtual bool computeIsReceptionPossible(const IListening *listening, const ITransmission *transmission) const override;
    virtual bool computeIsReceptionPossible(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const override;

    virtual bool supportsHtReception(const Ieee80211Transmission *transmission,
        IRadioSignal::SignalPart part) const;

    virtual const IReceptionDecision *computeReceptionDecision(const IListening *listening,
        const IReception *reception, IRadioSignal::SignalPart part,
        const IInterference *interference, const ISnir *snir) const override;

    virtual const IReceptionResult *computeReceptionResult(const IListening *listening, const IReception *reception, const IInterference *interference, const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const override;

  public:
    virtual ~Ieee80211Receiver();

    static std::vector<ChannelMatrixResourceCell> buildHtResourceCells(
        const Ieee80211HtPpduDescription& description, simtime_t ppduDuration);
    static std::vector<ChannelMatrixResourceCell> buildLegacyOfdmResourceCells(
        const Ieee80211OfdmMode& mode, b dataLength);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual void setModeSet(const Ieee80211ModeSet *modeSet);
    virtual void setBand(const IIeee80211Band *band);
    virtual void setChannel(const Ieee80211Channel *channel);
    virtual void setChannelNumber(int channelNumber);
    virtual void setHtCapabilities(const Ieee80211HtCapabilities *capabilities) override { htCapabilities = capabilities; }

    virtual const IChannelMatrixReceptionProcessor *getChannelMatrixReceptionProcessor() const override {
        return channelMatrixReceptionProcessor;
    }
    virtual size_t getMaximumMaterializedResourceCells() const override {
        return maximumMaterializedResourceCells;
    }
    virtual std::vector<ChannelMatrixResourceCell> getChannelMatrixResourceCells(
        const IReception& reception) const override;
};

} // namespace physicallayer

} // namespace inet

#endif
