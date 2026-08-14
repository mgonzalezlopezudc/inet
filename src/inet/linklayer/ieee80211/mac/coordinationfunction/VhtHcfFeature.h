//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTHCFFEATURE_H
#define __INET_VHTHCFFEATURE_H

#include <functional>
#include <memory>
#include <optional>

#include "inet/linklayer/ieee80211/mac/contract/IVhtDlMuExchangeCallback.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IVhtGroupIdManager.h"
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtSoundingService.h"
#include "inet/linklayer/ieee80211/mac/framesequence/VhtDlMuPlan.h"
#include "inet/physicallayer/wireless/ieee80211/contract/IIeee80211VhtPacketRadio.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

class Edca;
class Edcaf;
class FrameSequenceContext;
class IAckHandler;
class IFrameSequence;
class IFrameSequenceHandler;
class IOriginatorBlockAckAgreementHandler;
class IOriginatorBlockAckAgreementPolicy;
class IQosRateSelection;
class ITx;
class IVhtGroupIdManager;
class IIeee80211VhtDlMuScheduler;
class InProgressFrames;
class TxopProcedure;
class VhtDlMuTxOpFs;

/** Value-only decision captured once at the EDCAF grant boundary. */
struct INET_API VhtGrantSnapshot
{
    enum class StartKind {
        COMMON_SINGLE_USER,
        GROUP_MANAGEMENT,
        BLOCK_ACK_PREREQUISITE,
        MU_SOUNDING,
        DL_MULTIUSER,
        SU_SOUNDING,
    };

    StartKind startKind = StartKind::COMMON_SINGLE_USER;
    AccessCategory accessCategory = AC_BE;
    MacAddress peer;
    uint16_t associationId = 0;
    uint64_t associationGeneration = 0;
    uint8_t groupId = 1;
    uint8_t userPosition = 0;
    Hz channelWidth = Hz(0);
    int soundingNsts = 0;
    uint8_t dialogToken = 0;
    const physicallayer::IIeee80211Mode *soundingMode = nullptr;
    bool muFeedback = false;
    IOriginatorBlockAckAgreementHandler::PrerequisiteProbe blockAckProbe;
    HcfQueueToken sourceQueueToken;
    HcfPacketIdentity packetIdentity;
    std::optional<VhtDlMuPlan> dlMuPlan;
};

/** Executable VHT HCF feature. VhtHcf only supplies the common-HCF actions. */
class INET_API VhtHcfFeature : public IVhtDlMuExchangeCallback,
        public IVhtGroupIdManager::ILocalMembershipListener
{
  public:
    enum class GrantDisposition {
        STARTED,
        FINISHED_SYNCHRONOUSLY,
    };

    class INET_API IActions
    {
      public:
        virtual ~IActions() {}
        virtual Ieee80211Mac *getMac() const = 0;
        virtual physicallayer::Ieee80211ModeSet *getModeSet() const = 0;
        virtual Edca *getEdca() const = 0;
        virtual IQosRateSelection *getRateSelection() const = 0;
        virtual IOriginatorBlockAckAgreementHandler *getBlockAckHandler() const = 0;
        virtual IOriginatorBlockAckAgreementPolicy *getBlockAckPolicy() const = 0;
        virtual ITx *getTx() const = 0;
        virtual ITx::ICallback *getTxCallback() = 0;
        virtual IOriginatorMacDataService *getOriginatorDataService() const = 0;
        virtual const IFrameSequence *getActiveFrameSequence() const = 0;
        virtual IFrameSequenceHandler::ICallback *getFrameSequenceCallback() const = 0;
        virtual FrameSequenceContext *buildFrameSequenceContext(AccessCategory ac) = 0;
        virtual void startFeatureFrameSequence(IFrameSequence *sequence, AccessCategory ac) = 0;
        virtual void startFeatureFrameSequence(IFrameSequence *sequence,
                FrameSequenceContext *context) = 0;
        virtual void continueBaseFrameSequence(AccessCategory ac) = 0;
        virtual void continueBaseRecipientFrame(Packet *packet,
                const Ptr<const Ieee80211MacHeader>& header) = 0;
        virtual void continueBaseSetFrameMode(Packet *packet,
                const Ptr<const Ieee80211MacHeader>& header,
                const physicallayer::IIeee80211Mode *mode) const = 0;
        virtual void continueBaseTransmitFrame(Packet *packet, simtime_t ifs) = 0;
        virtual void continueBaseTransmittedFrame(Packet *packet) = 0;
        virtual void continueBaseReceivedFrame(Packet *packet,
                Packet *lastTransmittedPacket) = 0;
        virtual void continueBaseTransmissionComplete(Packet *packet,
                const Ptr<const Ieee80211MacHeader>& header) = 0;
        virtual void processTransmittedDataFrame(Packet *packet,
                const Ptr<const Ieee80211DataHeader>& header, AccessCategory ac) = 0;
        virtual void processFailedFrame(Packet *packet) = 0;
        virtual void reclaimPacketOwnership(Packet *packet) = 0;
    };

    class INET_API ITxOpFactory
    {
      public:
        virtual ~ITxOpFactory() {}
        virtual VhtDlMuTxOpFs *create(const VhtDlMuPlan& plan,
                physicallayer::Ieee80211ModeSet *modeSet, IAckHandler *ackHandler,
                IFrameSequenceHandler::ICallback *callback,
                IVhtDlMuExchangeCallback *vhtCallback,
                uint64_t exchangeId) = 0;
    };

  private:
    IActions *actions = nullptr;
    bool enableSuBeamforming = false;
    bool enableDlMuMimo = false;
    uint8_t dlMuGroupId = 1;
    double beamformingGainDb = 3;
    VhtSoundingService soundingService;
    IVhtGroupIdManager *groupIdManager = nullptr;
    IIeee80211VhtDlMuScheduler *dlMuScheduler = nullptr;
    physicallayer::IIeee80211VhtPacketRadio *radio = nullptr;
    std::unique_ptr<ITxOpFactory> defaultTxOpFactory;
    ITxOpFactory *txOpFactory = nullptr;
    enum class DlMuPhase {
        IDLE,
        ACTIVE,
        TERMINAL,
    };
    struct DlMuLifecycle {
        DlMuPhase phase = DlMuPhase::IDLE;
        uint64_t exchangeId = 0;
    };

    uint64_t nextDlMuExchangeId = 1;
    uint64_t lastRetiredDlMuExchangeId = 0;
    DlMuLifecycle dlMu;
    std::vector<bool> completedUsers;
    Packet *activeContainerPacket = nullptr;
    std::vector<std::vector<Packet *>> activeUserPackets;

    bool isAssociatedPeer(const MacAddress& peer) const;
    uint16_t getPeerAssociationId(const MacAddress& peer) const;
    bool isEligibleSu(const physicallayer::IIeee80211Mode *mode,
            const MacAddress& peer, int& soundingNsts) const;
    std::vector<MacAddress> getConstrainedMuPeers() const;
    std::optional<VhtGrantSnapshot> prepareBlockAckPrerequisite(AccessCategory ac) const;
    std::optional<VhtGrantSnapshot> prepareDlMu(AccessCategory ac) const;
    std::optional<VhtGrantSnapshot> prepareSuSounding(AccessCategory ac) const;
    virtual void localVhtGroupMembershipChanged(
            const std::optional<IVhtGroupIdManager::Membership>& membership) override;
    void commitBlockAckPrerequisite(const VhtGrantSnapshot& snapshot);
    GrantDisposition commitDlMu(const VhtGrantSnapshot& snapshot);
    uint64_t allocateDlMuExchangeId();
    void clearActiveDlMuExchange(uint64_t exchangeId);

  public:
    void configure(IActions *actions, bool enableSuBeamforming,
            bool enableDlMuMimo, uint8_t dlMuGroupId, double beamformingGainDb,
            simtime_t csiValidityDuration, IVhtSoundingCoordinator *coordinator,
            IVhtGroupIdManager *groupIdManager,
            IIeee80211VhtDlMuScheduler *dlMuScheduler,
            physicallayer::IIeee80211VhtPacketRadio *radio);
    static void validatePacketLevelRadio(cModule *radio);
    void setTxOpFactoryForTesting(ITxOpFactory *factory)
        { txOpFactory = factory == nullptr ? defaultTxOpFactory.get() : factory; }
    void modeSetChanged();
    VhtGrantSnapshot prepareGrantSnapshot(AccessCategory ac) const;
    void startSounding(const VhtGrantSnapshot& snapshot);
    GrantDisposition commitPreparedGrant(const VhtGrantSnapshot& snapshot);
    bool processHeaderlessNdpIndication(Packet *packet);
    void recipientProcessReceivedFrame(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header);
    void setFrameMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header,
            const physicallayer::IIeee80211Mode *mode) const;
    void transmitFrame(Packet *packet, simtime_t ifs);
    void originatorProcessTransmittedFrame(Packet *packet);
    void originatorProcessReceivedFrame(Packet *packet, Packet *lastTransmittedPacket);
    void transmissionComplete(Packet *packet,
            const Ptr<const Ieee80211MacHeader>& header);
    void invalidatePeer(const MacAddress& peer);

    static void installBlockAckPrerequisite(
            IOriginatorBlockAckAgreementHandler::PrerequisiteReservation& reservation,
            queueing::IPacketQueue *queue, InProgressFrames *inProgressFrames,
            const std::function<void(Packet *)>& reclaimOwnership);
    static void installAndStartBlockAckPrerequisite(
            IOriginatorBlockAckAgreementHandler::PrerequisiteReservation& reservation,
            queueing::IPacketQueue *queue, InProgressFrames *inProgressFrames,
            const std::function<void(Packet *)>& reclaimOwnership,
            const std::function<void()>& startSequence);
    static void configureProtectionAndStart(TxopProcedure *txop,
            const std::function<void()>& startSequence);

    virtual Ieee80211Mac *getVhtDlMuMac() const override;
    virtual IOriginatorMacDataService *getVhtDlMuOriginatorDataService() const override;
    virtual queueing::IPacketQueue *resolveVhtDlMuQueue(
            HcfQueueToken sourceQueueToken) const override;
    virtual void vhtDlMuPlanCommitted(uint64_t exchangeId,
            Packet *containerPacket,
            const std::vector<std::vector<Packet *>>& userPackets) override;
    virtual void processVhtDlMuFailedFrame(Packet *packet) override;
    virtual void processVhtDlMuUserResult(uint64_t exchangeId,
            unsigned int userIndex, UserResult result) override;

    VhtSoundingService& getSoundingServiceForTesting() { return soundingService; }
};

} // namespace ieee80211
} // namespace inet

#endif
