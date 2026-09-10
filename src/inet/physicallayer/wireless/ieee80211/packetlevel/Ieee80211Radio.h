//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211RADIO_H
#define __INET_IEEE80211RADIO_H

#include "inet/physicallayer/wireless/common/base/packetlevel/FlatRadioBase.h"
#include "inet/physicallayer/wireless/ieee80211/contract/Ieee80211CcaSnapshot.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Band.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Channel.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"

namespace inet {
namespace physicallayer {

class INET_API Ieee80211Radio : public FlatRadioBase
{
  public:
    /**
     * This signal is emitted every time the radio channel changes.
     * The signal value is the new radio channel.
     */
    static simsignal_t radioChannelChangedSignal;
    /**
     * This signal is emitted every time the per-channel PHY-CCA state changes.
     * The signal value is the new Ieee80211CcaSnapshot.
     */
    static simsignal_t ccaStateChangedSignal;
    static const Ptr<const Ieee80211PhyHeader> popIeee80211PhyHeaderAtFront(Packet *packet, b length = b(-1), int flags = 0);
    static const Ptr<const Ieee80211PhyHeader> peekIeee80211PhyHeaderAtFront(const Packet *packet, b length = b(-1), int flags = 0);

  protected:
    bool changingModeSet = false;
    FcsMode fcsMode = FCS_MODE_UNDEFINED;
    Ieee80211SecondaryChannelOffset htSecondaryChannelOffset = IEEE80211_SECONDARY_CHANNEL_NONE;
    Ieee80211ChannelWidth channelWidth = IEEE80211_CHANNEL_WIDTH_20MHZ;
    int primaryChannelCenterFrequencyIndex = -1;
    int channelCenterFrequencyIndex0 = -1;
    int channelCenterFrequencyIndex1 = 0;
    uint64_t ccaConfigurationRevision = 0;
    std::unique_ptr<Ieee80211CcaSnapshot> ccaSnapshot;
    int configurationTransactionDepth = 0;
    bool configurationChanged = false;
    bool pendingChannelChanged = false;
    int pendingChannelNumber = -1;
    std::string opMode;
    const Ieee80211ModeSet *modeSet = nullptr;
    const IIeee80211Band *band = nullptr;

  protected:
    virtual void initialize(int stage) override;

    void changeModeSet(const Ieee80211ModeSet *modeSet, const IIeee80211Mode *mode, bool explicitMode,
            const std::function<void()>& applyConfiguration = {}, bool publishModeSet = true, int channelNumber = -1, int requestedRadioMode = -1);

    virtual void handleUpperCommand(cMessage *message) override;

    virtual void insertFcs(const Ptr<Ieee80211PhyHeader>& phyHeader) const;
    virtual bool verifyFcs(const Ptr<const Ieee80211PhyHeader>& phyHeader) const;

    virtual void encapsulate(Packet *packet) const override;
    virtual void decapsulate(Packet *packet) const override;

    virtual bool computeIsBandBusy(const FrequencyBand& band, Ieee80211CcaGroup group, bool legacyHt40 = false) const;
    virtual void updateCcaState();
    virtual void updateTransceiverState() override;

    virtual void beginConfigurationTransaction();
    virtual void endConfigurationTransaction();
    virtual void markConfigurationChanged(bool channelChanged = false, int channelNumber = -1);
    virtual bool isConfigurationTransactionActive() const { return configurationTransactionDepth != 0; }

  public:
    Ieee80211Radio();

    virtual const Ieee80211CcaSnapshot& getCcaSnapshot() const { return *ccaSnapshot; }
    virtual const Ieee80211ModeSet *getModeSet() const { return modeSet; }
    virtual const IIeee80211Band *getBand() const { return band; }
    virtual const Ieee80211Channel *getChannel() const { return check_and_cast<const Ieee80211Receiver *>(receiver)->getChannel(); }

    // These setters snapshot transactional mode-set consumers before applying
    // the catalog, and restore them if an update throws. Notifications publish
    // committed state; listener exceptions propagate without rolling it back.
    // Behavioral consumers implement IIeee80211ModeSetListener.
    virtual void setModeSet(const Ieee80211ModeSet *modeSet);
    virtual void setModeSetAndMode(const Ieee80211ModeSet *modeSet, const IIeee80211Mode *mode);
    virtual void setMode(const IIeee80211Mode *mode);
    virtual void setBand(const IIeee80211Band *band);
    virtual void setChannel(const Ieee80211Channel *channel);
    virtual void setChannelNumber(int newChannelNumber);
    virtual void setCenterFrequency(Hz newCenterFrequency) override;
    virtual void setBandwidth(Hz newBandwidth) override;
    virtual const Ieee80211Channel *createConfiguredChannel(int channelNumber) const;
};

} // namespace physicallayer
} // namespace inet

#endif
