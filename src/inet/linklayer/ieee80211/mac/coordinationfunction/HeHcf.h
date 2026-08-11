//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEHCF_H
#define __INET_HEHCF_H

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>
#include <ostream>

#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTxopCoordinatorService.h"
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HeLinkPhyContext.h"
#include "inet/linklayer/ieee80211/mac/queue/StationQueueBankManager.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/queueing/contract/IPacketQueue.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeMuMimoCsiManager.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211HeOmi.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211AssociationState.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

struct INET_API HeTbResponseProtection
{
    simtime_t macDurationField = SIMTIME_ZERO;
    physicallayer::Ieee80211HeTxopDuration txopDuration;
};

class INET_API HeTbResponseEvent : public cObject
{
  public:
    enum Reason {
        DATA_SELECTED,
        NO_PENDING_DATA,
        NO_FITTING_PAYLOAD,
        BUFFER_STATUS_REPORTED,
        NDP_FEEDBACK_REPORTED,
        BLOCK_ACK_REQUESTED,
    };

    uint32_t triggerId = 0;
    IIeee80211HeUlTriggerPolicy::TriggerType triggerType =
            IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
    Reason reason = NO_PENDING_DATA;
    uint16_t associationId = 0;
    uint8_t tid = 0;
    AccessCategory accessCategory = AC_BE;
    int ruIndex = -1;
    int ruToneSize = 0;
    int ruToneOffset = 0;
    bool hadPendingPayload = false;
    int64_t pendingBytes = 0;
    int64_t selectedBytes = 0;
    int64_t reportedBytes = 0;
    int ackPolicy = -1;
};

class INET_API HePeerOperatingModeChangedEvent : public cObject
{
  public:
    MacAddress peerAddress;
    uint16_t associationId = 0;
    bool hadOldMode = false;
    Ieee80211HeOperatingMode oldMode;
    Ieee80211HeOperatingMode newMode;
};

/**
 * Returns the soliciting HE PPDU's decoded TXOP duration, if the incoming
 * packet was received in an HE PPDU.
 */
INET_API std::optional<physicallayer::Ieee80211HeTxopDuration>
getIeee80211HeSolicitingTxopDuration(const Packet *packet);

/**
 * IEEE 802.11-2024 26.11.5 response protection derived from the soliciting
 * frame Duration, SIFS, and the actual response PPDU TXTIME.
 */
INET_API HeTbResponseProtection deriveIeee80211HeTbResponseProtection(
        const std::optional<physicallayer::Ieee80211HeTxopDuration>& solicitingTxopDuration,
        simtime_t triggerDuration, simtime_t sifsTime, simtime_t responseTxTime);

/** Builds the compressed Block Ack bitmap requested by an MU-BAR User Info field. */
INET_API Ptr<Ieee80211CompressedBlockAck> buildHeMuBarCompressedBlockAck(
        const Ieee80211HeTriggerUserInfo& user, RecipientBlockAckAgreement *agreement,
        const MacAddress& receiverAddress, const MacAddress& transmitterAddress);

/** Attaches the immutable Trigger-derived HE-TB TXVECTOR and model-only controls. */
INET_API HeTbResponseProtection attachHeTbTxVectorFromTrigger(Packet *packet,
        const Ieee80211TriggerFrame& trigger, const Ieee80211HeTriggerUserInfo& user,
        uint16_t staId, Hz centerFrequency, W transmitPower, B psduLength,
        uint8_t bssColor, uint32_t triggerId,
        bool ndpFeedbackReport = false, uint8_t ndpFeedbackStatus = 0,
        uint8_t ndpRuToneSetIndex = 0, uint8_t ndpStartingStsNumber = 0,
        const std::optional<physicallayer::Ieee80211HeTxopDuration>& solicitingTxopDuration =
                std::nullopt,
        simtime_t sifsTime = SIMTIME_ZERO);

/** Computes DL path loss from Trigger AP power and total received PPDU power. */
INET_API double computeIeee80211HeTriggerPathLossDb(int apTxPowerDbm20Mhz,
        W receivedPower, Hz receivedBandwidth);

/** Applies HE-TB target-receive-power control and the local maximum-power cap. */
INET_API W computeIeee80211HeTbTransmitPower(W maximumPower, int targetReceivePowerDbm,
        double pathLossDb, bool useMaximumTransmitPower);

/** Release-active structural validation for a decoded HE Basic/BSRP/NFRP Trigger. */
INET_API std::optional<std::string> validateIeee80211HeUlTrigger(
        const Ieee80211TriggerFrame& trigger, Hz centerFrequency);

/** Validated result of resolving a scheduler policy into Trigger wire timing. */
struct INET_API HeUlScheduleFinalizationResult
{
    bool valid = false;
    IIeee80211HeUlScheduler::Schedule schedule;
    simtime_t resolvedTxTime = SIMTIME_ZERO;
    std::string error;

    explicit operator bool() const { return valid; }
};

/**
 * Extends Hcf to support IEEE 802.11ax Downlink OFDMA multi-user scheduling.
 *
 * When the winning EDCAF's pending queue contains packets for two or more
 * distinct destination STAs and the "ax" modeSet is active, HeHcf replaces
 * the standard HcfFs frame sequence with HeDlMuTxOpFs, which:
 *   1. Calls the DL OFDMA scheduler to obtain per-STA RU assignments.
 *   2. Dequeues one packet per selected STA.
 *   3. Assembles a container packet with explicit HE MU RU payload sections.
 *   4. Passes the container to the existing Tx pipeline where the packet-level
 *      PHY models the PPDU as a single transmission with per-RU reception.
 *
 * When fewer than two unique destination STAs are queued (or the modeSet is
 * not "ax"), HeHcf falls back transparently to the standard Hcf::startFrameSequence().
 */
class INET_API HeHcf : public Hcf, public IIeee80211PeerAssociationListener
{
  protected:
    IIeee80211HeDlScheduler *dlScheduler = nullptr;
    HeUlCoordinator *ulCoordinator = nullptr;
    HeTxopCoordinatorService txopCoordinator;
    std::unique_ptr<StationQueueBankManager> queueBankManager;
    std::unique_ptr<IIeee80211HeLinkPhyContext> linkPhyContext;
    bool peerAssociationListenerRegistered = false;
    cMessage *ulTriggerTimer = nullptr;
    cMessage *triggeredUlResponseTimer = nullptr;
    IIeee80211HeUlTriggerPolicy::TriggerType pendingUlTrigger = IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
    bool ulTriggerAccessRequested = false;
    simsignal_t heTbResponseCommittedSignal;
    simsignal_t heTbResponseTriggerIdSignal;
    simsignal_t heTbResponseReasonSignal;
    simsignal_t heTbResponseHadPendingPayloadSignal;
    simsignal_t heTbResponsePendingBytesSignal;
    simsignal_t heTbResponseSelectedBytesSignal;
    simsignal_t heTbResponseReportedBytesSignal;
    simsignal_t peerOperatingModeChangedSignal;
    simsignal_t peerOperatingModeAssociationIdSignal;
    simsignal_t peerOperatingModeRxNssSignal;
    simsignal_t peerOperatingModeChannelWidthSignal;
    simsignal_t peerOperatingModeUlMuDisableSignal;
    struct TriggeredUlExchange {
        enum class RecoveryKind {
            NONE,
            COMPRESSED_BLOCK_ACK_REQUEST,
        };

        Tid tid = 0;
        queueing::IPacketQueue *sourceQueue = nullptr;
        std::vector<Packet *> packets;
        std::vector<int> sequenceNumbers;
        RecoveryKind recoveryKind = RecoveryKind::NONE;
        AccessCategory recoveryAccessCategory = AC_BE;
        Ptr<const Ieee80211CompressedBlockAckReq> blockAckReq;
        physicallayer::Ieee80211HeRu ru;
        bool randomAccess = false;
        simtime_t expectedResponseTime = SIMTIME_ZERO;

        friend std::ostream& operator<<(std::ostream& os, const TriggeredUlExchange& exchange)
        {
            os << "tid=" << (int)exchange.tid 
               << " packets=" << exchange.packets.size() 
               << " randomAccess=" << (exchange.randomAccess ? "yes" : "no") 
               << " expectedResponse=" << exchange.expectedResponseTime;
            return os;
        }
    };
    // A response is retained by Trigger ID until its Multi-STA Block Ack is
    // processed.  This is intentionally not a single global packet: an HE-TB
    // response can be a single-TID A-MPDU and must be retried per MPDU.
    std::map<uint32_t, TriggeredUlExchange> triggeredUlExchanges;
    bool forceNextSingleUser[4] = {};
    std::map<MacAddress, Ieee80211HeOperatingMode> peerOperatingModes;
    // Packet identities captured while a frame sequence is active. A peer may
    // reassociate before that sequence finishes, so deferring by MAC address
    // would incorrectly retire packets from the new association epoch.
    std::map<Packet *, StationQueueBankManager::AssociationKey> packetsPendingRetirement;

    HeMuMimoCsiManager csiManager;
    bool enableDlMuMimo = false;
    simtime_t csiValidityDuration = SimTime(0.1);
    double defaultCsiLeakage = 0.1;
    std::string csiLeakageOverrides = "";


  protected:
    virtual void initialize(int stage) override;
    virtual void peerAssociationChanged(
            const Ieee80211AssociationState::PeerTransition& transition) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual uint32_t getBufferedTrafficServiceBytes(
            Edcaf *edcaf, const MacAddress& peer, int tid = -1) const override;
    virtual queueing::IPacketQueue *getPerStaQueue(const MacAddress& staAddr, AccessCategory ac) override;
    virtual const char *getPendingUlTriggerName() const;
    virtual int getStationQueueBankCount() const;
    virtual std::string getCsiTableSummary() const;
    virtual std::string getHeHcfSummary() const;
    virtual AccessCategory mapTidToAccessCategory(Tid tid) const;
    virtual bool allAssociatedStationsSupportPreamblePuncturing() const;
    virtual bool supportsPreamblePuncturing(const IIeee80211HeUlScheduler::RuAllocation& allocation) const;
    virtual const IIeee80211HeLinkPhyContext& getLinkPhyContext() const;
    static HeUlScheduleFinalizationResult finalizeUlSchedule(
            const IIeee80211HeUlScheduler::Schedule& proposedSchedule,
            Hz centerFrequency, Hz channelBandwidth,
            IIeee80211HeUlTriggerPolicy::TriggerType triggerType);
    static Packet *buildHeTbAmpdu(const std::vector<Packet *>& mpdus);
    virtual void retryPendingTriggeredUlExchanges();
    virtual void processFailedTriggeredUlBlockAckReq(TriggeredUlExchange& exchange);
    virtual void processReceivedTriggeredUlBlockAck(
            TriggeredUlExchange& exchange,
            const Ptr<const Ieee80211CompressedBlockAck>& blockAck);
    virtual int retireQueuedPacketsForPeer(const MacAddress& peer);
    virtual int retireInProgressPacketsForPeer(const MacAddress& peer);
    virtual bool retireQueuedPacket(Packet *packet,
            const StationQueueBankManager::AssociationKey& association);
    virtual bool retireInProgressPacket(Packet *packet);
    virtual void retireDeferredPackets();
    virtual StationQueueBank *ensureAssociatedQueueBank(const MacAddress& peer, uint64_t associationEpoch);
    virtual void finalizeRetiredQueueBanksIfSafe();
    virtual void scheduleTriggeredUlResponseTimeout();
    virtual void handleTriggeredUlResponseTimeout();
    virtual void beforeTriggeredUlPacketCommit(int packetIndex) {}
    virtual void emitHeTbResponse(HeTbResponseEvent& event);
    virtual void updatePeerOperatingMode(const MacAddress& peer,
            const Ieee80211HeOperatingMode& mode);
    virtual bool reportHeDlMuTxResult(Packet *packet, AccessCategory ac, bool success);
    virtual void originatorProcessBlockAckResult(
            const Ptr<const Ieee80211BlockAck>& blockAck,
            const std::set<std::pair<MacAddress, std::pair<Tid, SequenceControlField>>>& ackedFrames,
            AccessCategory ac) override;
    virtual void sendTriggeredBlockAckResponse(Packet *packet, const Ptr<const Ieee80211TriggerFrame>& trigger,
            uint32_t triggerId);
    virtual Packet *buildTriggeredUlResponsePacket(Packet *sourcePacket, queueing::IPacketQueue *sourceQueue,
            AccessCategory selectedAc, uint8_t selectedTid, uint32_t queueBytes, int availableSlots,
            const Ieee80211HeTriggerUserInfo *selected, const Ptr<const Ieee80211TriggerFrame>& trigger,
            uint32_t triggerId, W transmitPower,
            const std::optional<physicallayer::Ieee80211HeTxopDuration>& solicitingTxopDuration,
            TriggeredUlExchange& exchange, Ptr<const Ieee80211MacHeader>& responseHeader,
            bool& committed);
    virtual void processReceivedTriggerFrame(Packet *packet, const Ptr<const Ieee80211TriggerFrame>& trigger);
    virtual void processReceivedMultiStaBlockAck(Packet *packet, const Ptr<const Ieee80211MultiStaBlockAck>& multiStaBlockAck);

  public:
    virtual void legacyPreambleReceived(Packet *packet);

    /**
     * Scans the shared EDCAF queue and all per-STA queues for this access
     * category, returning one candidate per eligible destination STA.
     */
    virtual IIeee80211HeDlScheduler::ScheduleContext collectScheduleContext(AccessCategory ac) const;

    virtual queueing::IPacketQueue *findOldestPerStaQueue(AccessCategory ac) const;
    virtual bool stagePerStaFrameForBlockAckBootstrap(AccessCategory ac);
    virtual bool stagePerStaFrameForSingleUserTransmission(AccessCategory ac);
    virtual bool tryStartUlMuFrameSequence(AccessCategory ac);
    virtual bool tryStartDlMuFrameSequence(AccessCategory ac);
    virtual bool releaseChannelIfNoFallbackFrame(AccessCategory ac);

    /**
     * Override: selects HeDlMuTxOpFs when ≥2 unique destination STAs are
     * queued and HE mode is active; otherwise delegates to Hcf::startFrameSequence().
     */
    virtual void startFrameSequence(AccessCategory ac) override;
    virtual void handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs) override;
    virtual bool hasFrameToTransmit() override;
    virtual bool hasFrameToTransmit(AccessCategory ac) override;
    virtual bool processHeaderlessNdpIndication(Packet *packet) override;
    virtual void recipientProcessReceivedFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) override;
    virtual void transmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) override;
    virtual void transmitFrame(Packet *packet, simtime_t ifs) override;
    virtual void frameSequenceFinished() override;

  public:
    virtual ~HeHcf();
    virtual StationQueueBank *getStationQueueBank(const MacAddress& staAddr) const;
    virtual void invalidatePeerDerivedState(const MacAddress& peer) override;
    virtual void originatorProcessTransmittedFrame(Packet *packet) override;
    virtual void originatorProcessTransmittedControlFrame(const Ptr<const Ieee80211MacHeader>& controlHeader, AccessCategory ac) override;
    virtual void originatorProcessReceivedFrame(Packet *receivedPacket, Packet *lastTransmittedPacket) override;
    virtual void originatorProcessFailedFrame(Packet *packet) override;
    virtual uint16_t getAssociationId(const MacAddress& address) const;
    virtual const Ptr<Ieee80211CompressedBlockAck> processTriggeredUlBlockAckReq(
            Packet *packet,
            const Ptr<const Ieee80211CompressedBlockAckReq>& blockAckReq,
            uint16_t aid);
    virtual bool getPeerOperatingMode(const MacAddress& address, Ieee80211HeOperatingMode& mode) const;
    void handleDlMuPlanningFailure(AccessCategory ac);
    virtual void processTriggeredUlFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& header, uint16_t aid);
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HEHCF_H
