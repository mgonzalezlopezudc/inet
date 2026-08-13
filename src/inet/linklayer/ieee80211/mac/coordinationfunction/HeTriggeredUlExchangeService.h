//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HETRIGGEREDULEXCHANGESERVICE_H
#define __INET_HETRIGGEREDULEXCHANGESERVICE_H

#include <map>
#include <ostream>
#include <vector>

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/contract/ISequenceNumberAssignment.h"
#include "inet/linklayer/ieee80211/mac/contract/ITx.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfContext.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/IIeee80211HeUlTriggerPolicy.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {

class Packet;

namespace ieee80211 {

class HeQueueService;

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

struct INET_API HeTbResponseProtection
{
    simtime_t macDurationField = SIMTIME_ZERO;
    physicallayer::Ieee80211HeTxopDuration txopDuration;
};

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

/** Owns the terminal response window for committed HE-TB transmissions. */
class INET_API HeTriggeredUlExchangeService
{
  public:
    class INET_API IObserver
    {
      public:
        virtual ~IObserver() = default;
        virtual void preparedResponse(Packet *) {}
        virtual void beforeHandoff(Packet *) {}
        virtual void beforePacketCommit(int) {}
        virtual void beforeQueueCommit() {}
        virtual void beforeRandomAccessCommit() {}
    };

    struct TriggerReceptionSnapshot {
        MacAddress bssid;
        uint16_t associationId = 0;
        uint64_t associationEpoch = 0;
        bool ulEnabled = false;
        bool accessPoint = false;
        bool twtSleeping = false;
        bool ndpFeedbackEnabled = false;
        Hz centerFrequency = Hz(0);
        Hz channelBandwidth = Hz(0);
    };

    enum class TriggerDisposition {
        ACCEPT,
        FOREIGN_BSS,
        MISSING_CORRELATION,
        INELIGIBLE_STATION,
        UNSUPPORTED_NDP_FEEDBACK,
        LINK_BANDWIDTH_MISMATCH,
        MALFORMED,
        TWT_SLEEPING,
        EXCHANGE_PENDING,
    };

    struct TriggerSelection {
        TriggerDisposition disposition = TriggerDisposition::MALFORMED;
        uint32_t triggerId = 0;
        std::string diagnostic;

        explicit operator bool() const { return disposition == TriggerDisposition::ACCEPT; }
    };

    /** Immutable inputs for constructing the final HE-TB PSDU. The MPDUs are
     * prepared copies and no live queue packet is mutated by this operation. */
    struct ResponsePacketSnapshot {
        std::vector<Packet *> preparedMpdus;
        Ptr<const Ieee80211TriggerFrame> trigger;
        Ieee80211HeTriggerUserInfo selectedUser;
        uint16_t associationId = 0;
        Hz centerFrequency = Hz(0);
        W transmitPower = W(0);
        uint8_t bssColor = 0;
        uint32_t triggerId = 0;
        FcsMode fcsMode = FCS_DECLARED_CORRECT;
        std::optional<physicallayer::Ieee80211HeTxopDuration> solicitingTxopDuration;
        simtime_t sifsTime = SIMTIME_ZERO;
    };

    struct PreparedResponsePacket {
        Packet *packet = nullptr;
        Ptr<const Ieee80211MacHeader> header;
        simtime_t macDurationField = SIMTIME_ZERO;
        physicallayer::Ieee80211HeTxopDuration txopDuration;
    };

    struct AccessQueueSnapshot {
        AccessCategory accessCategory = AC_BE;
        HcfQueueToken queueToken;
        std::vector<Packet *> packets;
        int inProgressTid = -1;
    };

    struct ResponseSelectionSnapshot {
        IIeee80211HeUlTriggerPolicy::TriggerType triggerType =
                IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
        Ieee80211HeTriggerUserInfo selectedUser;
        bool hasSelectedUser = false;
        bool randomAccess = false;
        std::vector<AccessQueueSnapshot> queues;
    };

    struct TidTrafficSnapshot {
        AccessCategory accessCategory = AC_BE;
        Tid tid = 0;
        uint32_t bufferedBytes = 0;
        int availableBlockAckSlots = 0;
        bool hasBlockAckAgreement = false;
    };

    struct BlockAckCandidateSnapshot {
        AccessCategory accessCategory = AC_BE;
        bool hasDataHeader = false;
        bool qosData = false;
        MacAddress receiverAddress;
        Tid tid = 0;
        int fragmentNumber = 0;
        SequenceNumberCyclic sequenceNumber;
        bool hasAgreement = false;
        bool addbaResponseReceived = false;
        SequenceNumberCyclic agreementStartingSequenceNumber;
    };

    struct BlockAckRequestSelection {
        AccessCategory accessCategory = AC_BE;
        MacAddress receiverAddress;
        Tid tid = 0;
        SequenceNumberCyclic startingSequenceNumber;
    };

    /** Immutable capture of all live state consulted while processing one
     * Trigger. Decisions and protocol-state publication belong to this service. */
    struct TriggerProcessingSnapshot {
        Packet *packet = nullptr;
        Ptr<const Ieee80211TriggerFrame> trigger;
        TriggerReceptionSnapshot reception;
        ResponseSelectionSnapshot responseSelection;
        std::vector<TidTrafficSnapshot> traffic;
        std::vector<BlockAckCandidateSnapshot> blockAckCandidates;
        std::optional<Ieee80211NegotiatedHeCapabilities> negotiatedCapabilities;
        std::optional<physicallayer::Ieee80211HeTxopDuration> solicitingTxopDuration;
        std::optional<double> triggerPathLossDb;
        MacAddress localAddress;
        W maximumTransmitPower = W(0);
        uint8_t bssColor = 0;
        FcsMode fcsMode = FCS_DECLARED_CORRECT;
        bool ulMuDisabled = false;
        uint32_t totalBufferedBytes = 0;
        simtime_t currentTime = SIMTIME_ZERO;
        simtime_t sifsTime = SIMTIME_ZERO;
        simtime_t slotTime = SIMTIME_ZERO;
        simtime_t maximumBlockAckTxTime = SIMTIME_ZERO;
    };

    struct ResponseSelection {
        HcfQueueToken queueToken;
        std::vector<Packet *> queuePackets;
        Packet *sourcePacket = nullptr;
        AccessCategory accessCategory = AC_BE;
        Tid tid = 0;
        bool hasReportedTid = false;
    };

    struct SequencePreparation {
        std::unique_ptr<ISequenceNumberAssignment> state;
        bool active = false;
    };

    struct RandomAccessPreparation {
        AccessCategory accessCategory = AC_BE;
        int randomAccessRuCount = 0;
        int originalBackoff = 0;
        int resultingBackoff = 0;
        bool attempt = false;
    };

    enum class RecoveryKind {
        NONE,
        COMPRESSED_BLOCK_ACK_REQUEST,
    };

    struct Exchange {
        Tid tid = 0;
        HcfQueueToken sourceQueueToken;
        std::vector<Packet *> packets;
        std::vector<HcfPacketIdentity> packetIdentities;
        std::vector<int> sequenceNumbers;
        RecoveryKind recoveryKind = RecoveryKind::NONE;
        AccessCategory recoveryAccessCategory = AC_BE;
        Ptr<const Ieee80211CompressedBlockAckReq> blockAckReq;
        physicallayer::Ieee80211HeRu ru;
        bool randomAccess = false;
        MacAddress bssid;
        uint16_t associationId = 0;
        uint64_t associationEpoch = 0;
        simtime_t expectedResponseTime = SIMTIME_ZERO;

        friend std::ostream& operator<<(std::ostream& stream, const Exchange& exchange)
        {
            stream << "tid=" << static_cast<int>(exchange.tid)
                   << " packets=" << exchange.packets.size()
                   << " randomAccess=" << (exchange.randomAccess ? "yes" : "no")
                   << " expectedResponse=" << exchange.expectedResponseTime;
            return stream;
        }
    };

    /** Move-only, exact result of the fallible HE-TB construction phase. */
    struct PreparedTriggeredUlResponse {
        std::unique_ptr<Packet> responsePacket;
        Ptr<const Ieee80211MacHeader> responseHeader;
        Exchange exchange;
        std::map<uint32_t, Exchange>::node_type stagedExchange;
        SequencePreparation originalSequenceState;
        SequencePreparation preparedSequenceState;
        std::vector<Packet *> originalPackets;
        std::vector<std::unique_ptr<Packet>> preparedPacketOwners;
        std::vector<std::unique_ptr<Packet>> rollbackPacketOwners;
        std::vector<Packet *> queueOrder;
        Ptr<Ieee80211CompressedBlockAckReq> preparedBlockAckReq;
        AccessCategory blockAckReqAccessCategory = AC_BE;
        HeTbResponseEvent event;
        uint32_t triggerId = 0;
        bool hasBlockAckRequest = false;
        bool committed = false;
        bool queueCommitted = false;
        std::unique_ptr<ITx::PreparedTransmission> txReservation;

        PreparedTriggeredUlResponse() = default;
        PreparedTriggeredUlResponse(const PreparedTriggeredUlResponse&) = delete;
        PreparedTriggeredUlResponse& operator=(const PreparedTriggeredUlResponse&) = delete;
        PreparedTriggeredUlResponse(PreparedTriggeredUlResponse&&) = default;
        PreparedTriggeredUlResponse& operator=(PreparedTriggeredUlResponse&&) = default;
    };

    class INET_API IActions
    {
      public:
        virtual ~IActions() = default;
        virtual simtime_t getTriggeredUlCurrentTime() const = 0;
        virtual void scheduleTriggeredUlTimer(simtime_t time, cMessage *timer) = 0;
        virtual void cancelTriggeredUlTimer(cMessage *timer) = 0;
        virtual void cancelAndDeleteTriggeredUlTimer(cMessage *timer) = 0;
        virtual MacAddress getTriggeredUlBssid() const = 0;
        virtual MacAddress getTriggeredUlLocalAddress() const = 0;
        virtual uint16_t getTriggeredUlAssociationId() const = 0;
        virtual uint64_t getTriggeredUlAssociationEpoch() const = 0;
        virtual std::unique_ptr<ISequenceNumberAssignment>
                cloneTriggeredUlSequenceState() const = 0;
        virtual void commitTriggeredUlSequenceState(
                const ISequenceNumberAssignment& state) = 0;
        virtual Ptr<Ieee80211CompressedBlockAckReq>
                materializeTriggeredUlBlockAckRequest(
                        const BlockAckRequestSelection& selection) = 0;
        virtual void commitTriggeredUlBlockAckRequest(
                const Ptr<Ieee80211CompressedBlockAckReq>& blockAckReq,
                AccessCategory accessCategory) = 0;
        virtual void validateTriggeredUlPackets(HcfQueueToken queueToken,
                const std::vector<Packet *>& packets) const = 0;
        virtual std::vector<Packet *> commitTriggeredUlPackets(
                HcfQueueToken queueToken, const std::vector<Packet *>& originals,
                const std::vector<Packet *>& prepared) = 0;
        virtual void rollbackTriggeredUlPackets(HcfQueueToken queueToken,
                const std::vector<Packet *>& originals,
                const std::vector<Packet *>& backups,
                const std::vector<Packet *>& queueOrder) = 0;
        virtual void takeTriggeredUlPacket(Packet *packet) = 0;
        /** ITx borrows packet/header and synchronously makes its transmission
         * copy; the caller retains and deletes packet on both return paths. */
        virtual std::unique_ptr<ITx::PreparedTransmission>
                prepareTriggeredUlHandoff(Packet *packet,
                        const Ptr<const Ieee80211MacHeader>& header) = 0;
        virtual void commitTriggeredUlHandoff(
                std::unique_ptr<ITx::PreparedTransmission>) noexcept = 0;
        virtual Ptr<Ieee80211CompressedBlockAck>
                prepareTriggeredUlMuBarBlockAck(
                        const Ieee80211HeTriggerUserInfo& user,
                        const MacAddress& originator) = 0;
        virtual Hz getTriggeredUlCenterFrequency() const = 0;
        virtual uint8_t getTriggeredUlBssColor() const = 0;
        virtual FcsMode getTriggeredUlFcsMode() const = 0;
        virtual simtime_t getTriggeredUlSifsTime() const = 0;
        virtual AccessCategory mapTriggeredUlTidToAccessCategory(Tid tid) const = 0;
        virtual RandomAccessPreparation prepareTriggeredUlRandomAccess(
                AccessCategory accessCategory, int randomAccessRuCount) = 0;
        virtual int commitTriggeredUlRandomAccess(
                const RandomAccessPreparation& preparation) = 0;
        virtual void emitTriggeredUlResponse(HeTbResponseEvent& event) = 0;
        virtual void reportTriggeredUlRandomAccessResult(bool successful) = 0;
        virtual void processTriggeredUlBlockAckRequestFailure(
                const Ptr<const Ieee80211CompressedBlockAckReq>& blockAckReq,
                AccessCategory accessCategory) = 0;
        virtual void processTriggeredUlBlockAckRequestSuccess(
                const Ptr<const Ieee80211CompressedBlockAck>& blockAck,
                AccessCategory accessCategory) = 0;
        virtual void retireTriggeredUlPacket(Packet *packet,
                HcfPacketIdentity identity) = 0;
        virtual void retryTriggeredUlPacket(Packet *packet,
                HcfPacketIdentity identity, HcfQueueToken sourceQueueToken,
                const MacAddress& bssid, uint16_t associationId,
                uint64_t associationEpoch) = 0;
        virtual void startTriggeredUlMuEdcaTimer(AccessCategory accessCategory) = 0;
    };

  private:
    IActions *actions = nullptr;
    IObserver *observer = nullptr;
    cMessage *responseTimer = nullptr;
    std::map<uint32_t, Exchange> exchanges;

    void finishExchange(std::map<uint32_t, Exchange>::iterator exchange,
            bool successful);
    void retryPackets(Exchange& exchange);

  public:
    ~HeTriggeredUlExchangeService();

    void configure(IActions *actions);
    void setObserver(IObserver *observer) { this->observer = observer; }
    void shutdown();
    cMessage *getResponseTimer() const { return responseTimer; }
    size_t getExchangeCount() const { return exchanges.size(); }
    bool hasPendingExchange() const { return !exchanges.empty(); }
    const std::map<uint32_t, Exchange>& getExchanges() const { return exchanges; }

    void commit(uint32_t triggerId, Exchange&& exchange);
    PreparedTriggeredUlResponse prepareResponse(Packet *sourcePacket,
            HcfQueueToken sourceQueueToken, const std::vector<Packet *>& sourcePackets,
            AccessCategory selectedAc, uint8_t selectedTid, uint32_t queueBytes,
            int availableSlots, const Ieee80211HeTriggerUserInfo *selected,
            const Ptr<const Ieee80211TriggerFrame>& trigger, uint32_t triggerId,
            W transmitPower,
            const std::optional<physicallayer::Ieee80211HeTxopDuration>& solicitingTxopDuration,
            Exchange exchange,
            const std::vector<BlockAckCandidateSnapshot>& blockAckCandidates = {});
    void commit(PreparedTriggeredUlResponse&& prepared);
    void precommit(PreparedTriggeredUlResponse& prepared);
    void transferPrecommit(PreparedTriggeredUlResponse& destination,
            PreparedTriggeredUlResponse& source);
    void rollback(PreparedTriggeredUlResponse& prepared);
    void setDeadline(uint32_t triggerId, simtime_t deadline);
    void retryAll();
    void scheduleNextTimeout();
    void handleTimeout();
    void processMultiStaBlockAck(Packet *packet,
            const Ptr<const Ieee80211MultiStaBlockAck>& blockAck);
    static Packet *buildHeTbAmpdu(const std::vector<Packet *>& mpdus);
    static void preparePrimaryDataMpdu(Packet *packet, Tid tid,
            AccessCategory accessCategory, uint32_t queueBytes);
    static void prepareAdditionalDataMpdu(Packet *packet);
    static Packet *buildQosNullMpdu(Ptr<Ieee80211DataHeader> header,
            const MacAddress& bssid, const MacAddress& localAddress, Tid tid,
            AccessCategory accessCategory, uint32_t queueBytes);
    static Packet *buildCompressedBlockAckRequestMpdu(
            Ptr<Ieee80211CompressedBlockAckReq> blockAckReq,
            const MacAddress& localAddress);
    PreparedResponsePacket buildResponsePacket(
            const ResponsePacketSnapshot& snapshot) const;
    ResponseSelection selectResponse(
            const ResponseSelectionSnapshot& snapshot) const;
    static std::optional<BlockAckRequestSelection> selectBlockAckRequest(
            const std::vector<BlockAckCandidateSnapshot>& candidates,
            const MacAddress& receiverAddress);
    SequencePreparation prepareSequenceState() const;
    void assignSequenceNumber(SequencePreparation& preparation,
            const Ptr<Ieee80211DataOrMgmtHeader>& header) const;
    void commitSequenceState(SequencePreparation& preparation) const;
    void rollbackSequenceState(SequencePreparation& preparation) const;
    Packet *prepareAndCommitResponse(Packet *sourcePacket,
            HcfQueueToken sourceQueueToken, const std::vector<Packet *>& sourcePackets,
            AccessCategory selectedAc, uint8_t selectedTid, uint32_t queueBytes,
            int availableSlots, const Ieee80211HeTriggerUserInfo *selected,
            const Ptr<const Ieee80211TriggerFrame>& trigger, uint32_t triggerId,
            W transmitPower,
            const std::optional<physicallayer::Ieee80211HeTxopDuration>& solicitingTxopDuration,
            Exchange& exchange, Ptr<const Ieee80211MacHeader>& responseHeader,
            bool& committed, bool publish = true,
            PreparedTriggeredUlResponse *preparedResult = nullptr,
            const std::vector<BlockAckCandidateSnapshot>& blockAckCandidates = {});
    RandomAccessPreparation prepareRandomAccess(AccessCategory accessCategory,
            int randomAccessRuCount);
    int commitRandomAccess(const RandomAccessPreparation& preparation);
    TriggerSelection parseTrigger(Packet *packet,
            const Ptr<const Ieee80211TriggerFrame>& trigger,
            const TriggerReceptionSnapshot& snapshot) const;
    void processTrigger(TriggerProcessingSnapshot snapshot);
};

} // namespace ieee80211
} // namespace inet

#endif
