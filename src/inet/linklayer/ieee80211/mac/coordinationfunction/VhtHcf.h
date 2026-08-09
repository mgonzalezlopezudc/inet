//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTHCF_H
#define __INET_VHTHCF_H

#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtCsiCache.h"
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211VhtDlMuScheduler.h"
#include "inet/linklayer/ieee80211/mac/contract/IVhtGroupIdManager.h"
#include "inet/linklayer/ieee80211/mac/contract/IVhtSoundingCoordinator.h"
#include "inet/physicallayer/wireless/ieee80211/contract/IIeee80211VhtPacketRadio.h"

namespace inet {
namespace ieee80211 {

class VhtDlMuPlan;
class VhtDlMuTxOpFs;

class INET_API VhtHcf : public Hcf, public IVhtGroupIdManager::ILocalMembershipListener
{
  protected:
    bool enableVhtSuBeamforming = false;
    bool enableVhtDlMuMimo = false;
    uint8_t vhtDlMuGroupId = 1;
    double beamformingGainDb = 3;
    uint8_t nextDialogToken = 1;
    VhtCsiCache csiCache;
    IVhtSoundingCoordinator *soundingCoordinator = nullptr;
    IVhtGroupIdManager *groupIdManager = nullptr;
    IIeee80211VhtDlMuScheduler *dlMuScheduler = nullptr;
    physicallayer::IIeee80211VhtPacketRadio *vhtRadio = nullptr;
    Hz lastEffectiveChannelWidth = Hz(0);

    virtual void initialize(int stage) override;
    static void updateEffectiveChannelWidth(VhtCsiCache& cache,
            Hz& previousWidth, Hz currentWidth);
    virtual void receiveSignal(cComponent *source, simsignal_t signalID,
            cObject *obj, cObject *details) override;
    virtual bool isAssociatedPeer(const MacAddress& peer) const;
    virtual uint16_t getPeerAssociationId(const MacAddress& peer) const;
    virtual bool isEligibleVhtSu(const physicallayer::IIeee80211Mode *mode,
            const MacAddress& peer, int& soundingNsts) const;
    virtual std::vector<MacAddress> getConstrainedVhtMuPeers() const;
    virtual VhtDlMuTxOpFs *createVhtDlMuTxOpFs(const VhtDlMuPlan& plan,
            IAckHandler *ackHandler);
    virtual bool tryStartVhtDlMu(AccessCategory ac);
    virtual void startFrameSequence(AccessCategory ac) override;
    virtual bool processHeaderlessNdpIndication(Packet *packet) override;
    virtual void recipientProcessReceivedFrame(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header) override;
    virtual void setFrameMode(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header,
            const physicallayer::IIeee80211Mode *mode) const override;
    virtual void transmitFrame(Packet *packet, simtime_t ifs) override;
    virtual void originatorProcessTransmittedFrame(Packet *packet) override;
    virtual void originatorProcessReceivedFrame(Packet *packet,
            Packet *lastTransmittedPacket) override;
    virtual void transmissionComplete(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header) override;
    virtual void localVhtGroupMembershipChanged(
            const std::optional<IVhtGroupIdManager::Membership>& membership) override;

  public:
    static void validatePacketLevelRadio(cModule *radio);
    virtual void invalidatePeerDerivedState(const MacAddress& peer) override;
};

} // namespace ieee80211
} // namespace inet

#endif
