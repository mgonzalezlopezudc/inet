// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef __INET_HESOUNDINGSERVICE_H
#define __INET_HESOUNDINGSERVICE_H

#include <functional>
#include <optional>
#include <vector>

#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HeLinkPhyContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeSoundingFs.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"

namespace inet {
namespace ieee80211 {

class HeSoundingCoordinator;

/** Owns HE NDPA/NDP/BFRP dialog correlation independently of HCF and PHY objects. */
class INET_API HeSoundingService final
{
  public:
    struct StartAction;
    class INET_API IActions
    {
      public:
        virtual ~IActions() {}
        virtual void startHeSoundingExchange(const StartAction& action,
                AccessCategory accessCategory) = 0;
    };

    struct StartAction {
        std::vector<HeSoundingFs::TargetSta> targets;
        Hz channelCenterFrequency = Hz(NaN);
        Hz channelBandwidth = Hz(NaN);
        uint8_t dialogToken = 0;
        uint32_t triggerId = 0;
    };

    struct CsiUpdateEvent {
        MacAddress peer;
        Hz channelBandwidth = Hz(NaN);
    };

    struct PeerSnapshot {
        MacAddress address;
        uint16_t associationId = 0;
        std::optional<Ieee80211NegotiatedHeCapabilities> negotiatedCapabilities;
    };

    struct ReceiveSnapshot {
        uint16_t localAssociationId = 0;
        MacAddress localAddress;
        uint8_t bssColor = 0;
        int soundingDimensions = 1;
        int fcsMode = 0;
        simtime_t sifs = SIMTIME_ZERO;
        std::optional<physicallayer::Ieee80211HeTxopDuration> solicitingTxopDuration;
        Ieee80211HeLinkPhySnapshot linkPhy;
        std::vector<PeerSnapshot> peers;
    };

    struct ReceiveActions {
        std::function<void(const CsiUpdateEvent&)> publishCsiUpdate;
        std::function<void(Packet *, const Ptr<const Ieee80211MacHeader>&, simtime_t)> transmitResponse;
    };

    struct StaSnapshot {
        bool ndpAnnouncementReceived = false;
        bool ndpReceived = false;
        uint8_t dialogToken = 0;
    };

    struct SoundingTarget {
        MacAddress address;
        uint16_t aid = 0;
        int maxNss = 1;
    };

  private:
    friend class HeSoundingCoordinator;
    bool ndpAnnouncementReceived = false;
    bool ndpReceived = false;
    uint8_t soundingDialogToken = 0;
    std::vector<SoundingTarget> soundingTargets;
    uint8_t nextSoundingDialogToken = 1;
    IActions *actions = nullptr;

  public:
    void configure(IActions *value) { actions = value; }
    std::optional<StartAction> prepareSounding(
            const IIeee80211HeDlScheduler::ScheduleContext& snapshot,
            const std::function<uint16_t(const MacAddress&)>& getAssociationId);
    std::optional<StartAction> prepareSounding(
            const HcfHeSoundingSnapshot& snapshot);
    void commitPreparedSounding(StartAction action,
            AccessCategory accessCategory);
    bool processNdpIndication(bool isHeSuNdp);
    bool processReceivedFrame(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header,
            const ReceiveSnapshot& snapshot, const ReceiveActions& actions);
    void resetStaState();
    StaSnapshot getStaSnapshot() const
        { return {ndpAnnouncementReceived, ndpReceived, soundingDialogToken}; }

};

} // namespace ieee80211
} // namespace inet

#endif
