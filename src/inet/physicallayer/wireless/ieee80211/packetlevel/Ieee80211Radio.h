//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211RADIO_H
#define __INET_IEEE80211RADIO_H

#include "inet/physicallayer/wireless/common/base/packetlevel/FlatRadioBase.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Band.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Channel.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/physicallayer/wireless/ieee80211/contract/IIeee80211VhtPacketRadio.h"
#include "inet/physicallayer/wireless/ieee80211/contract/IIeee80211HePacketRadio.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"

namespace inet {
namespace physicallayer {

class INET_API Ieee80211Radio : public FlatRadioBase, public IIeee80211ModeSetProvider, public IIeee80211VhtPacketRadio, public IIeee80211HePacketRadio
{
  public:
    virtual int getVhtAntennaCount() const override;
    virtual Hz getVhtChannelWidth() const override;
    virtual Ieee80211VhtMuRxSelection getVhtMuRxSelection() const override { return vhtMuRxSelection; }
    virtual void setVhtMuRxSelection(const Ieee80211VhtMuRxSelection& selection) override { vhtMuRxSelection = selection; }
    virtual uint8_t getHeBssColor() const override { return heBssColor; }
    virtual void setHeBssColor(uint8_t value) override { heBssColor = value; }
    /**
     * This signal is emitted every time the radio channel changes.
     * The signal value is the new radio channel.
     */
    static simsignal_t radioChannelChangedSignal;
    static simsignal_t heRuIndexSignal;
    static simsignal_t heRuToneSizeSignal;
    static simsignal_t heRuToneOffsetSignal;
    static simsignal_t heStaIdSignal;
    static simsignal_t heSpatialStreamsSignal;
    static simsignal_t heStreamStartIndexSignal;
    static simsignal_t heScheduledPsduBytesSignal;
    static simsignal_t heUserPpduDurationSignal;
    static simsignal_t hePuncturedSubchannelMaskSignal;
    static simsignal_t acknowledgmentFrameTypeSignal;
    static simsignal_t acknowledgmentAirtimeSignal;
    static const Ptr<const Ieee80211PhyHeader> popIeee80211PhyHeaderAtFront(Packet *packet, b length = b(-1), int flags = 0);
    static const Ptr<const Ieee80211PhyHeader> peekIeee80211PhyHeaderAtFront(const Packet *packet, b length = b(-1), int flags = 0);

  protected:
    Ieee80211Primary80ChannelPosition primary80ChannelPosition = Ieee80211Primary80ChannelPosition::UNSPECIFIED;
    FcsMode fcsMode = FCS_MODE_UNDEFINED;
    const Ieee80211ModeSet *modeSet = nullptr;
    const IIeee80211Band *band = nullptr;
    std::string opMode;
    bool configurationUpdateInProgress = false;
    bool listeningChangePending = false;
    bool channelChangePending = false;
    int pendingChannelNumber = -1;
    Ieee80211VhtMuRxSelection vhtMuRxSelection;
    uint8_t heBssColor = 0;

  protected:
    virtual void initialize(int stage) override;

    virtual void handleUpperCommand(cMessage *message) override;

    virtual void insertFcs(const Ptr<Ieee80211PhyHeader>& phyHeader) const;
    virtual bool verifyFcs(const Ptr<const Ieee80211PhyHeader>& phyHeader) const;
    virtual void notifyListeningChanged();
    virtual void notifyChannelChanged(int channelNumber);
    virtual void finishConfigurationUpdate(bool publishModeSet);
    virtual void cancelConfigurationUpdate();

    virtual void encapsulate(Packet *packet) const override;
    virtual void decapsulate(Packet *packet) const override;
    virtual bool supportsParallelReception(const ITransmission *transmission) const override;

  public:
    Ieee80211Radio();

    virtual const Ieee80211ModeSet *getModeSet() const override { return modeSet; }
    virtual const IIeee80211Band *getBand() const override { return band; }
    virtual const Ieee80211Channel *getChannel() const override;
    virtual Ieee80211Primary80ChannelPosition getPrimary80ChannelPosition() const override { return primary80ChannelPosition; }
    virtual Hz getChannelWidth() const override;
    virtual Hz getModeBandwidth() const override;

    virtual void setModeSet(const Ieee80211ModeSet *modeSet);
    virtual void setModeSetAndMode(const Ieee80211ModeSet *modeSet, const IIeee80211Mode *mode);
    virtual void setMode(const IIeee80211Mode *mode);
    virtual void setBand(const IIeee80211Band *band);
    virtual void setChannel(const Ieee80211Channel *channel);
    virtual void setChannelNumber(int newChannelNumber);
};

} // namespace physicallayer
} // namespace inet

#endif
