//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTriggeredUlExchangeService.h"

#include <algorithm>
#include <memory>

#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/linklayer/ethernet/common/Ethernet.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/IIeee80211HeUlTriggerPolicy.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeUlScheduler.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

W computeIeee80211HeTbTransmitPower(W maximumPower, int targetReceivePowerDbm,
        double pathLossDb, bool useMaximumTransmitPower);

Packet *HeTriggeredUlExchangeService::buildHeTbAmpdu(
        const std::vector<Packet *>& mpdus)
{
    if (mpdus.empty())
        throw cRuntimeError("Cannot build an empty HE-TB A-MPDU");
    auto ampdu = new Packet("HE-TB-A-MPDU");
    // IEEE Std 802.11-2024 9.7.1 and 26.5.2.4: each HE-TB MPDU is
    // delimiter-prefixed and all non-final subframes are four-octet aligned.
    for (size_t i = 0; i < mpdus.size(); ++i) {
        auto delimiter = makeShared<Ieee80211MpduSubframeHeader>();
        delimiter->setLength(mpdus[i]->getByteLength());
        delimiter->setEof(i + 1 == mpdus.size());
        ampdu->insertAtBack(delimiter);
        ampdu->insertAtBack(mpdus[i]->peekData());
        int padding = (4 - (B(4) + B(mpdus[i]->getByteLength())).get<B>() % 4) % 4;
        if (i + 1 != mpdus.size() && padding != 0)
            ampdu->insertAtBack(makeShared<ByteCountChunk>(B(padding)));
    }
    return ampdu;
}

void HeTriggeredUlExchangeService::preparePrimaryDataMpdu(Packet *packet,
        Tid tid, AccessCategory accessCategory, uint32_t queueBytes)
{
    if (packet == nullptr)
        throw cRuntimeError("Cannot prepare an empty HE-TB data MPDU");
    auto header = packet->removeAtFront<Ieee80211DataHeader>();
    if (!header->getBufferStatusPresent())
        header->setChunkLength(header->getChunkLength() + B(4));
    header->setOrder(true);
    header->setAckPolicy(NORMAL_ACK);
    header->setBufferStatusPresent(true);
    header->setBufferStatusTid(tid);
    header->setBufferStatusAc(accessCategory);
    header->setBufferStatusQueueSize(queueBytes);
    packet->insertAtFront(header);
}

void HeTriggeredUlExchangeService::prepareAdditionalDataMpdu(Packet *packet)
{
    if (packet == nullptr)
        throw cRuntimeError("Cannot prepare an empty additional HE-TB MPDU");
    auto header = packet->removeAtFront<Ieee80211DataHeader>();
    header->setOrder(true);
    header->setAckPolicy(NORMAL_ACK);
    packet->insertAtFront(header);
}

Packet *HeTriggeredUlExchangeService::buildQosNullMpdu(
        Ptr<Ieee80211DataHeader> header, const MacAddress& bssid,
        const MacAddress& localAddress, Tid tid,
        AccessCategory accessCategory, uint32_t queueBytes)
{
    if (header == nullptr)
        throw cRuntimeError("Cannot construct HE-TB QoS Null without a prepared header");
    header->setType(ST_QOS_NULL);
    header->setReceiverAddress(bssid);
    header->setTransmitterAddress(localAddress);
    header->setAddress3(bssid);
    header->setToDS(true);
    header->setTid(tid);
    header->setAckPolicy(NORMAL_ACK);
    header->setOrder(true);
    header->setBufferStatusPresent(true);
    header->setBufferStatusTid(tid);
    header->setBufferStatusAc(accessCategory);
    header->setBufferStatusQueueSize(queueBytes);
    header->setChunkLength(B(30));
    auto packet = new Packet("HE-TB-QoS-Null", header);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    return packet;
}

Packet *HeTriggeredUlExchangeService::buildCompressedBlockAckRequestMpdu(
        Ptr<Ieee80211CompressedBlockAckReq> blockAckReq,
        const MacAddress& localAddress)
{
    if (blockAckReq == nullptr)
        throw cRuntimeError("Cannot construct HE-TB compressed BAR without a header");
    blockAckReq->setTransmitterAddress(localAddress);
    blockAckReq->setBarAckPolicy(false);
    blockAckReq->setReserved(0);
    blockAckReq->setFragmentNumber(0);
    blockAckReq->setDurationField(SIMTIME_ZERO);
    auto packet = new Packet("HE-TB-Compressed-BAR", blockAckReq);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    return packet;
}

HeTriggeredUlExchangeService::PreparedResponsePacket
HeTriggeredUlExchangeService::buildResponsePacket(
        const ResponsePacketSnapshot& snapshot) const
{
    if (snapshot.preparedMpdus.empty() || snapshot.trigger == nullptr ||
            snapshot.triggerId == 0 || snapshot.associationId == 0)
        throw cRuntimeError("Incomplete HE-TB response construction snapshot");

    auto finalizeMpdu = [&] (Packet *mpdu, simtime_t durationField) {
        auto header = mpdu->removeAtFront<Ieee80211MacHeader>();
        header->setDurationField(durationField);
        mpdu->insertAtFront(header);
        auto trailer = mpdu->removeAtBack<Ieee80211MacTrailer>(B(4));
        trailer->setFcsMode(snapshot.fcsMode);
        if (snapshot.fcsMode == FCS_COMPUTED)
            trailer->setFcs(computeEthernetFcs(mpdu, snapshot.fcsMode));
        mpdu->insertAtBack(trailer);
    };
    for (auto mpdu : snapshot.preparedMpdus)
        finalizeMpdu(mpdu, SIMTIME_ZERO);
    std::unique_ptr<Packet> packet(buildHeTbAmpdu(snapshot.preparedMpdus));
    auto protection = attachHeTbTxVectorFromTrigger(packet.get(),
            *snapshot.trigger, snapshot.selectedUser, snapshot.associationId,
            snapshot.centerFrequency, snapshot.transmitPower,
            B((packet->getDataLength().get<b>() + 7) / 8), snapshot.bssColor,
            snapshot.triggerId, false, 0, 0, 0,
            snapshot.solicitingTxopDuration, snapshot.sifsTime);

    for (auto mpdu : snapshot.preparedMpdus)
        finalizeMpdu(mpdu, protection.macDurationField);
    packet.reset(buildHeTbAmpdu(snapshot.preparedMpdus));
    auto attachedProtection = attachHeTbTxVectorFromTrigger(packet.get(),
            *snapshot.trigger, snapshot.selectedUser, snapshot.associationId,
            snapshot.centerFrequency, snapshot.transmitPower,
            B((packet->getDataLength().get<b>() + 7) / 8), snapshot.bssColor,
            snapshot.triggerId, false, 0, 0, 0,
            snapshot.solicitingTxopDuration, snapshot.sifsTime);
    if (attachedProtection.macDurationField != protection.macDurationField ||
            !(attachedProtection.txopDuration == protection.txopDuration))
        throw cRuntimeError("HE-TB response protection changed while rebuilding the unchanged-length PSDU");

    PreparedResponsePacket result;
    result.packet = packet.release();
    result.header = snapshot.preparedMpdus.front()->peekAtFront<Ieee80211MacHeader>();
    result.macDurationField = protection.macDurationField;
    result.txopDuration = protection.txopDuration;
    return result;
}

HeTriggeredUlExchangeService::ResponseSelection
HeTriggeredUlExchangeService::selectResponse(
        const ResponseSelectionSnapshot& snapshot) const
{
    if (!snapshot.hasSelectedUser)
        throw cRuntimeError("Cannot select an HE-TB response without a selected Trigger user");
    ResponseSelection result;
    AccessCategory preferredAccessCategory;
    switch (snapshot.selectedUser.preferredAc) {
        case 0: preferredAccessCategory = AC_BE; break;
        case 1: preferredAccessCategory = AC_BK; break;
        case 2: preferredAccessCategory = AC_VI; break;
        case 3: preferredAccessCategory = AC_VO; break;
        default: throw cRuntimeError("Invalid Preferred AC in Basic Trigger");
    }
    auto selectQueue = [&] (const AccessQueueSnapshot& queue) {
        result.queueToken = queue.queueToken;
        result.queuePackets = queue.packets;
        result.accessCategory = queue.accessCategory;
    };
    auto findQueue = [&] (AccessCategory accessCategory) -> const AccessQueueSnapshot * {
        for (const auto& queue : snapshot.queues)
            if (queue.accessCategory == accessCategory)
                return &queue;
        return nullptr;
    };
    auto findFirstData = [&] (const AccessQueueSnapshot& queue) -> Packet * {
        for (auto packet : queue.packets) {
            auto header = dynamicPtrCast<const Ieee80211DataHeader>(
                    packet->peekAtFront<Ieee80211MacHeader>());
            if (header != nullptr && header->getType() == ST_DATA_WITH_QOS)
                return packet;
        }
        return nullptr;
    };
    auto selectInProgress = [&] (AccessCategory lowestAccessCategory) {
        for (int ac = AC_VO; ac >= lowestAccessCategory; --ac) {
            auto queue = findQueue(static_cast<AccessCategory>(ac));
            if (queue != nullptr && queue->inProgressTid >= 0) {
                selectQueue(*queue);
                result.tid = queue->inProgressTid;
                result.hasReportedTid = true;
                return true;
            }
        }
        return false;
    };

    if (snapshot.randomAccess) {
        if (snapshot.unassociated) {
            for (const auto& queue : snapshot.queues) {
                for (auto packet : queue.packets) {
                    auto header = dynamicPtrCast<const Ieee80211MgmtHeader>(
                            packet->peekAtFront<Ieee80211MacHeader>());
                    if (header == nullptr || header->getReceiverAddress() !=
                            snapshot.responsePeer)
                        continue;
                    selectQueue(queue);
                    result.sourcePacket = packet;
                    result.tid = 15;
                    result.hasReportedTid = true;
                    return result;
                }
            }
        }
        for (int ac = AC_VO; ac >= AC_BK; --ac) {
            auto queue = findQueue(static_cast<AccessCategory>(ac));
            if (queue == nullptr)
                continue;
            auto packet = findFirstData(*queue);
            if (packet == nullptr)
                continue;
            selectQueue(*queue);
            auto header = packet->peekAtFront<Ieee80211DataHeader>();
            result.tid = header->getTid();
            result.hasReportedTid = true;
            if (snapshot.triggerType != IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER)
                result.sourcePacket = packet;
            return result;
        }
    }
    else if (snapshot.triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER) {
        for (int ac = AC_VO; ac >= AC_BK; --ac) {
            auto queue = findQueue(static_cast<AccessCategory>(ac));
            if (queue == nullptr)
                continue;
            auto packet = findFirstData(*queue);
            if (packet == nullptr)
                continue;
            selectQueue(*queue);
            result.tid = packet->peekAtFront<Ieee80211DataHeader>()->getTid();
            result.hasReportedTid = true;
            return result;
        }
        selectInProgress(AC_BK);
    }
    else if (snapshot.triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        for (int ac = AC_VO; ac >= preferredAccessCategory; --ac) {
            auto queue = findQueue(static_cast<AccessCategory>(ac));
            if (queue == nullptr)
                continue;
            auto packet = findFirstData(*queue);
            if (packet == nullptr)
                continue;
            selectQueue(*queue);
            result.sourcePacket = packet;
            result.tid = packet->peekAtFront<Ieee80211DataHeader>()->getTid();
            result.hasReportedTid = true;
            return result;
        }
        if (!selectInProgress(preferredAccessCategory)) {
            auto queue = findQueue(preferredAccessCategory);
            if (queue != nullptr)
                selectQueue(*queue);
        }
    }
    if (!result.queueToken.isValid()) {
        auto queue = findQueue(preferredAccessCategory);
        if (queue != nullptr)
            selectQueue(*queue);
    }
    return result;
}

std::optional<HeTriggeredUlExchangeService::BlockAckRequestSelection>
HeTriggeredUlExchangeService::selectBlockAckRequest(
        const std::vector<BlockAckCandidateSnapshot>& candidates,
        const MacAddress& receiverAddress)
{
    for (int ac = AC_VO; ac >= AC_BK; --ac) {
        const auto accessCategory = static_cast<AccessCategory>(ac);
        for (const auto& candidate : candidates) {
            if (candidate.accessCategory != accessCategory ||
                    !candidate.hasDataHeader || !candidate.qosData ||
                    candidate.receiverAddress != receiverAddress ||
                    candidate.fragmentNumber != 0 ||
                    !candidate.hasAgreement || !candidate.addbaResponseReceived)
                continue;
            bool completeWindow = true;
            for (const auto& outstanding : candidates) {
                if (outstanding.accessCategory != accessCategory ||
                        !outstanding.hasDataHeader ||
                        outstanding.tid != candidate.tid)
                    continue;
                int offset = (outstanding.sequenceNumber.get() -
                        candidate.agreementStartingSequenceNumber.get() + 4096) % 4096;
                if (outstanding.receiverAddress != receiverAddress ||
                        outstanding.fragmentNumber != 0 || offset >= 64) {
                    completeWindow = false;
                    break;
                }
            }
            if (completeWindow)
                return BlockAckRequestSelection{accessCategory, receiverAddress,
                        candidate.tid,
                        candidate.agreementStartingSequenceNumber};
        }
    }
    return std::nullopt;
}

HeTriggeredUlExchangeService::SequencePreparation
HeTriggeredUlExchangeService::prepareSequenceState() const
{
    SequencePreparation preparation;
    preparation.state = actions->cloneTriggeredUlSequenceState();
    if (preparation.state == nullptr)
        throw cRuntimeError("Cannot clone HE-TB sequence-number state");
    preparation.active = true;
    return preparation;
}

void HeTriggeredUlExchangeService::assignSequenceNumber(
        SequencePreparation& preparation,
        const Ptr<Ieee80211DataOrMgmtHeader>& header) const
{
    if (!preparation.active || preparation.state == nullptr || header == nullptr)
        throw cRuntimeError("Cannot assign an HE-TB sequence number outside preparation");
    preparation.state->assignSequenceNumber(header);
}

void HeTriggeredUlExchangeService::commitSequenceState(
        SequencePreparation& preparation) const
{
    if (!preparation.active || preparation.state == nullptr)
        throw cRuntimeError("Cannot commit inactive HE-TB sequence-number state");
    actions->commitTriggeredUlSequenceState(*preparation.state);
    preparation.active = false;
}

void HeTriggeredUlExchangeService::rollbackSequenceState(
        SequencePreparation& preparation) const
{
    preparation.state.reset();
    preparation.active = false;
}

HeTriggeredUlExchangeService::RandomAccessPreparation
HeTriggeredUlExchangeService::prepareRandomAccess(AccessCategory accessCategory,
        int randomAccessRuCount)
{
    return actions->prepareTriggeredUlRandomAccess(accessCategory,
            randomAccessRuCount);
}

int HeTriggeredUlExchangeService::commitRandomAccess(
        const RandomAccessPreparation& preparation)
{
    if (observer != nullptr)
        observer->beforeRandomAccessCommit();
    return actions->commitTriggeredUlRandomAccess(preparation);
}

HeTriggeredUlExchangeService::~HeTriggeredUlExchangeService()
{
    ASSERT(responseTimer == nullptr);
    ASSERT(exchanges.empty());
}

void HeTriggeredUlExchangeService::configure(IActions *actions)
{
    if (actions == nullptr)
        throw cRuntimeError("HE triggered UL exchange service requires owner actions");
    this->actions = actions;
    if (responseTimer == nullptr)
        responseTimer = new cMessage("triggeredUlResponseTimer");
}

void HeTriggeredUlExchangeService::shutdown()
{
    if (actions == nullptr)
        return;
    for (auto& entry : exchanges)
        for (auto packet : entry.second.packets)
            delete packet;
    exchanges.clear();
    actions->cancelAndDeleteTriggeredUlTimer(responseTimer);
    responseTimer = nullptr;
    actions = nullptr;
}

void HeTriggeredUlExchangeService::commit(uint32_t triggerId, Exchange&& exchange)
{
    if (triggerId == 0 || actions == nullptr)
        throw cRuntimeError("Cannot commit an invalid HE triggered UL exchange");
    if (exchange.bssid.isUnspecified()) {
        exchange.bssid = actions->getTriggeredUlBssid();
        exchange.associationId = actions->getTriggeredUlAssociationId();
        exchange.associationEpoch = actions->getTriggeredUlAssociationEpoch();
    }
    if (exchange.packetIdentities.empty())
        for (auto packet : exchange.packets)
            exchange.packetIdentities.emplace_back(packet->getId());
    if (exchange.packetIdentities.size() != exchange.packets.size() ||
            exchange.sequenceNumbers.size() != exchange.packets.size())
        throw cRuntimeError("Incomplete packet identity in HE triggered UL exchange");
    auto inserted = exchanges.emplace(triggerId, std::move(exchange));
    if (!inserted.second)
        throw cRuntimeError("Duplicate HE-TB Trigger ID reached the committed exchange ledger");
    scheduleNextTimeout();
}

void HeTriggeredUlExchangeService::setDeadline(uint32_t triggerId, simtime_t deadline)
{
    auto it = exchanges.find(triggerId);
    if (it == exchanges.end())
        throw cRuntimeError("Unknown HE-TB Trigger ID %u", triggerId);
    it->second.expectedResponseTime = deadline;
    scheduleNextTimeout();
}

void HeTriggeredUlExchangeService::scheduleNextTimeout()
{
    actions->cancelTriggeredUlTimer(responseTimer);
    if (exchanges.empty())
        return;
    auto earliest = std::min_element(exchanges.begin(), exchanges.end(),
            [] (const auto& left, const auto& right) {
                return left.second.expectedResponseTime < right.second.expectedResponseTime;
            });
    actions->scheduleTriggeredUlTimer(
            std::max(actions->getTriggeredUlCurrentTime(),
                    earliest->second.expectedResponseTime), responseTimer);
}

void HeTriggeredUlExchangeService::retryPackets(Exchange& exchange)
{
    for (size_t i = 0; i < exchange.packets.size(); ++i)
        actions->retryTriggeredUlPacket(exchange.packets[i],
                exchange.packetIdentities[i], exchange.sourceQueueToken,
                exchange.bssid, exchange.associationId, exchange.associationEpoch);
    exchange.packets.clear();
    exchange.packetIdentities.clear();
    exchange.sequenceNumbers.clear();
}

void HeTriggeredUlExchangeService::finishExchange(
        std::map<uint32_t, Exchange>::iterator exchange, bool successful)
{
    if (exchange->second.randomAccess)
        actions->reportTriggeredUlRandomAccessResult(successful);
    exchanges.erase(exchange);
}

void HeTriggeredUlExchangeService::retryAll()
{
    if (actions == nullptr)
        return;
    for (auto& entry : exchanges) {
        auto& exchange = entry.second;
        if (exchange.recoveryKind == RecoveryKind::COMPRESSED_BLOCK_ACK_REQUEST)
            actions->processTriggeredUlBlockAckRequestFailure(
                    exchange.blockAckReq, exchange.recoveryAccessCategory);
        retryPackets(exchange);
        if (exchange.randomAccess)
            actions->reportTriggeredUlRandomAccessResult(false);
    }
    exchanges.clear();
    actions->cancelTriggeredUlTimer(responseTimer);
}

void HeTriggeredUlExchangeService::handleTimeout()
{
    const auto now = actions->getTriggeredUlCurrentTime();
    for (auto it = exchanges.begin(); it != exchanges.end(); ) {
        if (it->second.expectedResponseTime > now) {
            ++it;
            continue;
        }
        auto current = it++;
        if (current->second.recoveryKind == RecoveryKind::COMPRESSED_BLOCK_ACK_REQUEST)
            actions->processTriggeredUlBlockAckRequestFailure(
                    current->second.blockAckReq,
                    current->second.recoveryAccessCategory);
        retryPackets(current->second);
        finishExchange(current, false);
    }
    scheduleNextTimeout();
}

void HeTriggeredUlExchangeService::processMultiStaBlockAck(Packet *packet,
        const Ptr<const Ieee80211MultiStaBlockAck>& blockAck)
{
    // This model currently supports only the fixed 64-bit compressed Block Ack
    // bitmap. Negotiated variable bitmap lengths are a separate feature.
    constexpr int MODELED_BLOCK_ACK_BITMAP_BITS = 64;
    auto correlation = packet->findTag<physicallayer::Ieee80211HeTriggerCorrelationTag>();
    if (correlation == nullptr) {
        delete packet;
        return;
    }
    auto it = exchanges.find(correlation->getTriggerId());
    if (it == exchanges.end() ||
            actions->getTriggeredUlCurrentTime() > it->second.expectedResponseTime ||
            (it->second.associationId != 2045 &&
             (it->second.bssid != actions->getTriggeredUlBssid() ||
              it->second.associationId != actions->getTriggeredUlAssociationId() ||
              it->second.associationEpoch != actions->getTriggeredUlAssociationEpoch()))) {
        delete packet;
        return;
    }
    auto& exchange = it->second;
    if (blockAck->getTransmitterAddress() != exchange.bssid ||
            (exchange.associationId != 2045 &&
             blockAck->getTransmitterAddress() != actions->getTriggeredUlBssid())) {
        delete packet;
        return;
    }
    const auto aid = exchange.associationId;
    if (exchange.recoveryKind == RecoveryKind::COMPRESSED_BLOCK_ACK_REQUEST) {
        const auto requestedTid = exchange.blockAckReq->getTidInfo();
        const auto requestedStartingSequenceNumber =
                exchange.blockAckReq->getStartingSequenceNumber().get();
        uint64_t combinedBitmap = 0;
        bool valid = false;
        // IEEE Std 802.11-2024 26.4.2 requires the originator to examine each
        // Per-AID/TID field; multiple fields for one block-ack session are valid.
        for (unsigned int i = 0; i < blockAck->getRecordsArraySize(); ++i) {
            const auto& record = blockAck->getRecords(i);
            if (record.aid == aid && record.tid == requestedTid &&
                    record.startingSequenceNumber == requestedStartingSequenceNumber &&
                    record.responseReceived) {
                combinedBitmap |= record.bitmap;
                valid = true;
            }
        }
        if (valid) {
            auto compressed = makeShared<Ieee80211CompressedBlockAck>();
            compressed->setReceiverAddress(actions->getTriggeredUlLocalAddress());
            compressed->setTransmitterAddress(exchange.bssid);
            compressed->setTidInfo(requestedTid);
            compressed->setStartingSequenceNumber(
                    SequenceNumberCyclic(requestedStartingSequenceNumber));
            std::vector<uint8_t> bytes(8, 0);
            BitVector bitmap(bytes);
            for (int bit = 0; bit < MODELED_BLOCK_ACK_BITMAP_BITS; ++bit)
                bitmap.setBit(bit,
                        (combinedBitmap & (UINT64_C(1) << bit)) != 0);
            compressed->setBlockAckBitmap(bitmap);
            actions->processTriggeredUlBlockAckRequestSuccess(
                    compressed, exchange.recoveryAccessCategory);
        }
        else
            actions->processTriggeredUlBlockAckRequestFailure(
                    exchange.blockAckReq, exchange.recoveryAccessCategory);
        finishExchange(it, valid);
        scheduleNextTimeout();
        delete packet;
        return;
    }

    std::vector<const Ieee80211MultiStaBlockAckRecord *> matchingRecords;
    for (unsigned int i = 0; i < blockAck->getRecordsArraySize(); ++i)
        if (blockAck->getRecords(i).aid == aid &&
                blockAck->getRecords(i).tid == exchange.tid)
            matchingRecords.push_back(&blockAck->getRecords(i));
    bool successful = exchange.packets.empty() &&
            std::any_of(matchingRecords.begin(), matchingRecords.end(),
                    [] (const auto record) { return record->responseReceived; });
    bool acknowledgedData = false;
    for (size_t i = 0; i < exchange.packets.size(); ++i) {
        bool acknowledged = false;
        for (const auto record : matchingRecords) {
            if (!record->responseReceived)
                continue;
            const int offset = (exchange.sequenceNumbers[i] -
                    record->startingSequenceNumber + 4096) % 4096;
            acknowledged = offset < MODELED_BLOCK_ACK_BITMAP_BITS &&
                    (record->bitmap & (UINT64_C(1) << offset)) != 0;
            if (exchange.preassociationManagement && record->responseReceived &&
                    record->receiverAddress == actions->getTriggeredUlLocalAddress())
                acknowledged = true;
            if (acknowledged)
                break;
        }
        if (acknowledged) {
            actions->retireTriggeredUlPacket(exchange.packets[i],
                    exchange.packetIdentities[i]);
            successful = true;
            acknowledgedData = true;
        }
        else
            actions->retryTriggeredUlPacket(exchange.packets[i],
                    exchange.packetIdentities[i], exchange.sourceQueueToken,
                    exchange.bssid, exchange.associationId,
                    exchange.associationEpoch);
    }
    exchange.packets.clear();
    exchange.packetIdentities.clear();
    exchange.sequenceNumbers.clear();
    if (acknowledgedData)
        actions->startTriggeredUlMuEdcaTimer(
                actions->mapTriggeredUlTidToAccessCategory(exchange.tid));
    EV_INFO << "Applied correlated Multi-STA Block Ack: trigger="
            << correlation->getTriggerId() << ", AID=" << aid
            << ", TID=" << static_cast<int>(exchange.tid)
            << ", packets=" << exchange.packets.size()
            << ", success=" << successful << "\n";
    finishExchange(it, successful);
    scheduleNextTimeout();
    delete packet;
}

static physicallayer::Ieee80211HeTxVectorValidationResult createHeTbTxVector(
        const Ieee80211TriggerFrame& trigger, const Ieee80211HeTriggerUserInfo& selected,
        Hz centerFrequency, uint16_t staId, B psduLength,
        uint8_t bssColor = 0,
        bool ndpFeedbackReport = false, uint8_t ndpFeedbackStatus = 0,
        uint8_t ndpRuToneSetIndex = 0, uint8_t ndpStartingStsNumber = 0,
        physicallayer::Ieee80211HeTxopDuration txopDuration = {})
{
    physicallayer::Ieee80211HeTxVectorRequest request;
    request.centerFrequency = centerFrequency;
    request.channelBandwidth = Hz(trigger.getChannelBandwidthMhz() * 1e6);
    request.ppduFormat = physicallayer::HE_TRIGGER_BASED_UPLINK;
    // Puncturing is not carried by the 802.11ax Trigger Common Info field.
    // The supported HE-TB response therefore uses the unpunctured bandwidth
    // described by UL BW and the selected wire RU allocation.
    request.puncturedSubchannelMask = 0;
    request.lSigLength = trigger.getUlLength();
    request.noSignalExtension = false;
    request.requestedTxTime = trigger.getCommonDuration();
    // UL Length reconstructs a 4 us response-time envelope, not the original
    // transmitter-local exact TXTIME.
    request.requestedTxTimeExact = false;
    request.triggerMethod = physicallayer::Ieee80211HeTriggerMethod::TRIGGER_FRAME;
    request.bssColor = bssColor;
    request.txopDuration = txopDuration;
    request.preFecPaddingFactor = trigger.getPreFecPaddingFactor();
    request.ldpcExtraSymbolSegment = trigger.getLdpcExtraSymbolSegment();
    request.peDisambiguity = trigger.getPeDisambiguity();
    for (size_t i = 0; i < request.spatialReuse.size(); ++i)
        request.spatialReuse[i] = (trigger.getUlSpatialReuse() >> (4 * i)) & 0xF;
    request.doppler = trigger.getDoppler();
    request.numberOfHeLtfSymbols = trigger.getNumberOfHeLtfSymbols();
    request.guardInterval =
            static_cast<physicallayer::Ieee80211HeGuardInterval>(trigger.getGuardInterval());
    request.ltfType =
            static_cast<physicallayer::Ieee80211HeLtfType>(trigger.getLtfType());
    // The nominal PE is reconstructed from the wire pre-FEC padding factor
    // and PE disambiguity fields by the HE-TB calculator.
    request.packetExtensionDurationUs = 0;
    request.ndp = ndpFeedbackReport;

    auto appendUser = [&] (const Ieee80211HeTriggerUserInfo& triggerUser,
            uint16_t userStaId, B userPsduLength) {
        physicallayer::Ieee80211HeUserTxVectorRequest user;
        user.ru.index = triggerUser.ruIndex;
        user.ru.toneSize = triggerUser.ruToneSize;
        user.ru.toneOffset = triggerUser.ruToneOffset;
        user.staId = userStaId;
        user.mcs = triggerUser.mcs;
        user.numberOfSpatialStreams = triggerUser.numberOfSpatialStreams;
        user.streamStartIndex = triggerUser.streamStartIndex;
        user.coding =
                static_cast<physicallayer::Ieee80211HeCoding>(triggerUser.coding);
        user.psduLength = userPsduLength;
        if (&triggerUser == &selected) {
            user.ndpFeedbackReport = ndpFeedbackReport;
            user.ndpFeedbackStatus = ndpFeedbackStatus;
            user.ndpRuToneSetIndex = ndpRuToneSetIndex;
            user.ndpStartingStsNumber = ndpStartingStsNumber;
        }
        request.users.push_back(user);
    };

    appendUser(selected, staId, psduLength);
    if (selected.muMimo) {
        for (unsigned int i = 0; i < trigger.getUsersArraySize(); ++i) {
            const auto& peer = trigger.getUsers(i);
            if (&peer == &selected || !peer.muMimo ||
                    peer.ruToneSize != selected.ruToneSize ||
                    peer.ruToneOffset != selected.ruToneOffset)
                continue;
            appendUser(peer, peer.aid, B(0));
        }
    }
    return physicallayer::Ieee80211HeTxVectorFactory::create(request);
}

HeTbResponseProtection deriveIeee80211HeTbResponseProtection(
        const std::optional<physicallayer::Ieee80211HeTxopDuration>& solicitingTxopDuration,
        simtime_t triggerDuration, simtime_t sifsTime, simtime_t responseTxTime)
{
    if (triggerDuration < SIMTIME_ZERO || sifsTime < SIMTIME_ZERO ||
            responseTxTime < SIMTIME_ZERO)
        throw cRuntimeError("Cannot derive HE-TB response protection from negative timing");
    auto remaining = std::max(SIMTIME_ZERO,
            triggerDuration - sifsTime - responseTxTime);
    int64_t remainingUs = remaining.inUnit(SIMTIME_US);
    if (SimTime(remainingUs, SIMTIME_US) < remaining)
        remainingUs++;
    HeTbResponseProtection result;
    result.macDurationField = SimTime(remainingUs, SIMTIME_US);
    if (solicitingTxopDuration.has_value() && solicitingTxopDuration->unspecified)
        result.txopDuration = {};
    else
        result.txopDuration = {false, static_cast<uint16_t>(
                std::min<int64_t>(8448, remainingUs))};
    return result;
}

HeTbResponseProtection attachHeTbTxVectorFromTrigger(Packet *packet,
        const Ieee80211TriggerFrame& trigger, const Ieee80211HeTriggerUserInfo& user,
        uint16_t staId, Hz centerFrequency, W transmitPower, B psduLength,
        uint8_t bssColor, uint32_t triggerId,
        bool ndpFeedbackReport, uint8_t ndpFeedbackStatus,
        uint8_t ndpRuToneSetIndex, uint8_t ndpStartingStsNumber,
        const std::optional<physicallayer::Ieee80211HeTxopDuration>& solicitingTxopDuration,
        simtime_t sifsTime)
{
    if (packet == nullptr)
        throw cRuntimeError("Cannot attach an HE-TB TXVECTOR to an empty packet");
    auto preliminary = createHeTbTxVector(trigger, user, centerFrequency, staId, psduLength,
            bssColor,
            ndpFeedbackReport, ndpFeedbackStatus, ndpRuToneSetIndex,
            ndpStartingStsNumber);
    if (!preliminary)
        throw cRuntimeError("Cannot construct preliminary Trigger-derived HE-TB TXVECTOR: %s (%s)",
                preliminary.getContext().fieldName.c_str(),
                preliminary.getContext().detail.c_str());
    auto protection = deriveIeee80211HeTbResponseProtection(
            solicitingTxopDuration, trigger.getDurationField(), sifsTime,
            preliminary.getPpduLayout()->getDuration());
    auto result = createHeTbTxVector(trigger, user, centerFrequency, staId, psduLength,
            bssColor,
            ndpFeedbackReport, ndpFeedbackStatus, ndpRuToneSetIndex,
            ndpStartingStsNumber, protection.txopDuration);
    if (!result)
        throw cRuntimeError("Cannot construct Trigger-derived HE-TB TXVECTOR: %s (%s)",
                result.getContext().fieldName.c_str(), result.getContext().detail.c_str());
    packet->addTag<physicallayer::Ieee80211HeTxVectorReq>()->setCanonicalPair(
            result.getTxVector(), result.getPpduLayout());
    packet->addTagIfAbsent<physicallayer::Ieee80211HeTriggerCorrelationTag>()->
            setTriggerId(triggerId);
    if (!std::isnan(transmitPower.get()))
        packet->addTagIfAbsent<SignalPowerReq>()->setPower(transmitPower);
    return protection;
}

void HeTriggeredUlExchangeService::commit(PreparedTriggeredUlResponse&& prepared)
{
    if (prepared.responsePacket == nullptr || prepared.triggerId == 0)
        throw cRuntimeError("Cannot commit an incomplete prepared HE-TB response");

    if (prepared.stagedExchange.empty())
        precommit(prepared);
    auto& exchange = prepared.stagedExchange.empty() ?
            prepared.exchange : prepared.stagedExchange.mapped();

    actions->commitTriggeredUlHandoff(std::move(prepared.txReservation));

    if (prepared.hasBlockAckRequest) {
        exchange.tid = prepared.preparedBlockAckReq->getTidInfo();
        exchange.recoveryKind = RecoveryKind::COMPRESSED_BLOCK_ACK_REQUEST;
        exchange.recoveryAccessCategory =
                prepared.blockAckReqAccessCategory;
        exchange.blockAckReq = prepared.preparedBlockAckReq;
        actions->commitTriggeredUlBlockAckRequest(
                prepared.preparedBlockAckReq,
                prepared.blockAckReqAccessCategory);
    }
    else
        commitSequenceState(prepared.preparedSequenceState);
    rollbackSequenceState(prepared.originalSequenceState);
    auto inserted = exchanges.insert(std::move(prepared.stagedExchange));
    if (!inserted.inserted)
        std::terminate();
    scheduleNextTimeout();
    actions->emitTriggeredUlResponse(prepared.event);
}

void HeTriggeredUlExchangeService::precommit(
        PreparedTriggeredUlResponse& prepared)
{
    if (prepared.queueCommitted || !prepared.stagedExchange.empty())
        throw cRuntimeError("HE-TB response was precommitted more than once");
    std::vector<Packet *> preparedPackets;
    for (const auto& owner : prepared.preparedPacketOwners)
        preparedPackets.push_back(owner.get());
    if (!prepared.originalPackets.empty()) {
        if (observer != nullptr)
            observer->beforeQueueCommit();
        auto committedPackets = actions->commitTriggeredUlPackets(
                prepared.exchange.sourceQueueToken, prepared.originalPackets,
                preparedPackets);
        prepared.queueCommitted = true;
        for (size_t i = 0; i < committedPackets.size(); ++i) {
            actions->takeTriggeredUlPacket(committedPackets[i]);
            prepared.exchange.packets[i] = committedPackets[i];
        }
    }
    if (prepared.exchange.bssid.isUnspecified()) {
        prepared.exchange.bssid = actions->getTriggeredUlBssid();
        prepared.exchange.associationId = actions->getTriggeredUlAssociationId();
        prepared.exchange.associationEpoch = actions->getTriggeredUlAssociationEpoch();
    }
    if (prepared.exchange.packetIdentities.empty())
        for (auto packet : prepared.exchange.packets)
            prepared.exchange.packetIdentities.emplace_back(packet->getId());
    if (prepared.exchange.packetIdentities.size() !=
            prepared.exchange.packets.size() ||
            prepared.exchange.sequenceNumbers.size() !=
                    prepared.exchange.packets.size()) {
        rollback(prepared);
        throw cRuntimeError("Incomplete packet identity in prepared HE-TB exchange");
    }
    std::map<uint32_t, Exchange> staging;
    staging.emplace(prepared.triggerId, std::move(prepared.exchange));
    prepared.stagedExchange = staging.extract(prepared.triggerId);
}

void HeTriggeredUlExchangeService::transferPrecommit(
        PreparedTriggeredUlResponse& destination,
        PreparedTriggeredUlResponse& source)
{
    if (destination.queueCommitted || source.stagedExchange.empty())
        throw cRuntimeError("Invalid HE-TB queue precommit transfer");
    auto& sourceExchange = source.stagedExchange.mapped();
    for (size_t i = 0; i < destination.exchange.packets.size(); ++i)
        destination.exchange.packets[i] = sourceExchange.packets[i];
    destination.exchange.packetIdentities = sourceExchange.packetIdentities;
    destination.exchange.bssid = sourceExchange.bssid;
    destination.exchange.associationId = sourceExchange.associationId;
    destination.exchange.associationEpoch = sourceExchange.associationEpoch;
    destination.originalPackets = std::move(source.originalPackets);
    destination.rollbackPacketOwners = std::move(source.rollbackPacketOwners);
    destination.queueOrder = std::move(source.queueOrder);
    destination.queueCommitted = source.queueCommitted;
    source.queueCommitted = false;
    std::map<uint32_t, Exchange> staging;
    staging.emplace(destination.triggerId, std::move(destination.exchange));
    destination.stagedExchange = staging.extract(destination.triggerId);
}

void HeTriggeredUlExchangeService::rollback(
        PreparedTriggeredUlResponse& prepared)
{
    if (!prepared.queueCommitted)
        return;
    auto& exchange = prepared.stagedExchange.empty() ?
            prepared.exchange : prepared.stagedExchange.mapped();
    std::vector<Packet *> backups;
    for (const auto& owner : prepared.rollbackPacketOwners)
        backups.push_back(owner.get());
    actions->rollbackTriggeredUlPackets(exchange.sourceQueueToken,
            prepared.originalPackets, backups, prepared.queueOrder);
    prepared.queueCommitted = false;
}

HeTriggeredUlExchangeService::PreparedTriggeredUlResponse
HeTriggeredUlExchangeService::prepareResponse(Packet *sourcePacket,
        HcfQueueToken sourceQueueToken, const std::vector<Packet *>& sourcePackets,
        AccessCategory selectedAc, uint8_t selectedTid, uint32_t queueBytes, int availableSlots,
        const Ieee80211HeTriggerUserInfo *selected, const Ptr<const Ieee80211TriggerFrame>& trigger,
        uint32_t triggerId, W transmitPower,
        const std::optional<physicallayer::Ieee80211HeTxopDuration>& solicitingTxopDuration,
        Exchange exchange,
        const std::vector<BlockAckCandidateSnapshot>& blockAckCandidates)
{
    if (!sourceQueueToken.isValid() || selected == nullptr || trigger == nullptr)
        throw cRuntimeError("Cannot prepare an HE-TB response without queue-token and Trigger context");
    const auto centerFrequency = actions->getTriggeredUlCenterFrequency();
    auto originalSequenceNumberState = prepareSequenceState();
    auto preparedSequenceNumberState = prepareSequenceState();
    std::vector<Packet *> originalPackets;
    std::vector<std::unique_ptr<Packet>> preparedPacketOwners;
    std::unique_ptr<Packet> nullMpdu;
    std::unique_ptr<Packet> blockAckReqMpdu;
    Ptr<Ieee80211CompressedBlockAckReq> preparedBlockAckReq;
    AccessCategory blockAckReqAc = AC_BE;
    std::unique_ptr<Packet> responsePacket;
    const bool hadPendingPayload = sourcePacket != nullptr;
    const bool preassociationManagement = sourcePacket != nullptr &&
            dynamicPtrCast<const Ieee80211MgmtHeader>(
                    sourcePacket->peekAtFront<Ieee80211MacHeader>()) != nullptr;
    if (sourcePacket != nullptr) {
        // 26.5.2.4 requires a QoS Null response when the allocation cannot
        // contain pending data. Check the first MPDU too; the aggregation loop
        // below performs the same check for every additional MPDU.
        auto sourceHeader = sourcePacket->peekAtFront<Ieee80211MacHeader>();
        auto dataSourceHeader = dynamicPtrCast<const Ieee80211DataHeader>(sourceHeader);
        if (!preassociationManagement && dataSourceHeader == nullptr)
            throw cRuntimeError("HE-TB data response source is not a data frame");
        B psduLength = B(sourcePacket->getByteLength() +
                (preassociationManagement ? 0 :
                 IIeee80211HeUlScheduler::getHeTbQueuedPacketOverheadBytes(
                         dataSourceHeader->getBufferStatusPresent())));
        auto prospective = createHeTbTxVector(*trigger, *selected,
                centerFrequency,
                preassociationManagement ? 2045 :
                        actions->getTriggeredUlAssociationId(), psduLength);
        if (!prospective && preassociationManagement)
            throw cRuntimeError("Preassociation management MPDU does not fit the HE-TB allocation");
        if (!prospective)
            sourcePacket = nullptr;
    }
    // IEEE Std 802.11-2024, 26.5.2.4 and 26.4.1: when the selected
    // single-TID BA window is full, a Basic Trigger with nonzero TAL may carry
    // a BAR S-MPDU for any AC whose TID has an active agreement. The agreement
    // SSN is used unchanged; an outstanding candidate outside its 64-position
    // compressed bitmap invalidates that BAR choice instead of being skipped.
    if (!preassociationManagement && sourcePacket == nullptr && availableSlots == 0 &&
            trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER &&
            selected->tidAggregationLimit > 0) {
        auto selection = selectBlockAckRequest(blockAckCandidates,
                actions->getTriggeredUlBssid());
        if (selection.has_value()) {
            blockAckReqAc = selection->accessCategory;
            preparedBlockAckReq = actions->materializeTriggeredUlBlockAckRequest(
                    *selection);
        }
        if (preparedBlockAckReq != nullptr)
            blockAckReqMpdu.reset(buildCompressedBlockAckRequestMpdu(
                    preparedBlockAckReq, actions->getTriggeredUlLocalAddress()));
    }
    if (blockAckReqMpdu != nullptr) {
        std::unique_ptr<Packet> prospectiveAmpdu(
                buildHeTbAmpdu({blockAckReqMpdu.get()}));
        auto prospective = createHeTbTxVector(*trigger, *selected,
                centerFrequency,
                actions->getTriggeredUlAssociationId(),
                B((prospectiveAmpdu->getDataLength().get<b>() + 7) / 8));
        if (!prospective) {
            blockAckReqMpdu.reset();
            preparedBlockAckReq = nullptr;
        }
    }
    if (sourcePacket != nullptr) {
        auto originalSourcePacket = sourcePacket;
        preparedPacketOwners.emplace_back(sourcePacket->dup());
        sourcePacket = preparedPacketOwners.back().get();
        originalPackets.push_back(originalSourcePacket);
        // IEEE Std 802.11-2024 Table 9-13, 10.3.2.13.3, and 26.4.4.5:
        // Ack Policy wire bits 00 are context-dependent in an A-MPDU. They
        // denote Implicit BAR on preceding untagged MPDUs and Normal Ack on
        // the tagged final MPDU that solicits the immediate Multi-STA Block Ack.
        auto writableHeader = preassociationManagement ?
                Ptr<Ieee80211DataOrMgmtHeader>(
                        sourcePacket->removeAtFront<Ieee80211MgmtHeader>()) :
                Ptr<Ieee80211DataOrMgmtHeader>(
                        sourcePacket->removeAtFront<Ieee80211DataHeader>());
        if (!writableHeader->getRetry())
            assignSequenceNumber(
                    preparedSequenceNumberState, writableHeader);
        sourcePacket->insertAtFront(writableHeader);
        if (!preassociationManagement)
            preparePrimaryDataMpdu(sourcePacket,
                    selectedTid, selectedAc, queueBytes);
        responsePacket.reset(sourcePacket->dup());
    }
    else if (blockAckReqMpdu == nullptr) {
        if (preassociationManagement)
            throw cRuntimeError("Preassociation management response lost its source MPDU");
        // 26.5.2.4 allows a triggered STA with no data fitting the allocation
        // to carry a QoS Null-style response; we still include BSR so the AP's
        // scheduler state is refreshed by the HE TB exchange.
        auto nullHeader = makeShared<Ieee80211DataHeader>();
        // IEEE Std 802.11-2024 Table 9-13, 10.3.2.13.3, and 26.4.4.5:
        // this single, tagged final QoS Null MPDU uses wire bits 00 as Normal
        // Ack to solicit the immediate Multi-STA Block Ack.
        assignSequenceNumber(
                preparedSequenceNumberState, nullHeader);
        nullMpdu.reset(HeTriggeredUlExchangeService::buildQosNullMpdu(
                nullHeader, actions->getTriggeredUlBssid(),
                actions->getTriggeredUlLocalAddress(),
                selectedTid, selectedAc, queueBytes));
    }

    if (sourcePacket != nullptr) {
        exchange.packets.push_back(sourcePacket);
        exchange.sequenceNumbers.push_back(sourcePacket->peekAtFront<
                Ieee80211DataOrMgmtHeader>()->getSequenceNumber().get());

        // 26.6.3 permits multi-TID HE TB A-MPDUs only within the negotiated
        // Trigger TID Aggregation Limit.  This model deliberately restricts
        // Basic Trigger UL aggregation to one TID; retained packets are removed
        // from the EDCA queue only after the HE TB PSDU is built and are retried
        // individually from the returned Multi-STA BA bitmap.
        int maximumMpduCount = preassociationManagement ? 1 :
                std::min(64, availableSlots);
        for (int i = 0; availableSlots > 0 && (int)exchange.packets.size() < maximumMpduCount &&
                i < (int)sourcePackets.size(); ++i) {
            auto candidate = sourcePackets[i];
            if (candidate == originalPackets.front())
                continue;
            auto candidateHeader = dynamicPtrCast<const Ieee80211DataHeader>(candidate->peekAtFront<Ieee80211MacHeader>());
            if (preassociationManagement)
                break;
            if (candidateHeader == nullptr || candidateHeader->getType() != ST_DATA_WITH_QOS ||
                    candidateHeader->getTid() != selectedTid ||
                    candidateHeader->getReceiverAddress() != actions->getTriggeredUlBssid())
                continue;
            B psduLength(0);
            for (auto packet : exchange.packets)
                psduLength += B(4 + packet->getByteLength());
            psduLength += B(4 + candidate->getByteLength());
            auto prospective = createHeTbTxVector(*trigger, *selected,
                    centerFrequency,
                    actions->getTriggeredUlAssociationId(), psduLength);
            if (!prospective)
                break;
            auto originalCandidate = candidate;
            preparedPacketOwners.emplace_back(candidate->dup());
            candidate = preparedPacketOwners.back().get();
            auto writableCandidateHeader = candidate->removeAtFront<Ieee80211DataHeader>();
            if (!writableCandidateHeader->getRetry())
                assignSequenceNumber(
                        preparedSequenceNumberState, writableCandidateHeader);
            candidate->insertAtFront(writableCandidateHeader);
            HeTriggeredUlExchangeService::prepareAdditionalDataMpdu(candidate);
            originalPackets.push_back(originalCandidate);
            exchange.packets.push_back(candidate);
            exchange.sequenceNumbers.push_back(writableCandidateHeader->getSequenceNumber().get());
        }
    }

    HeTriggeredUlExchangeService::ResponsePacketSnapshot responseSnapshot;
    responseSnapshot.preparedMpdus = sourcePacket != nullptr ? exchange.packets :
            blockAckReqMpdu != nullptr ? std::vector<Packet *>{blockAckReqMpdu.get()} :
            std::vector<Packet *>{nullMpdu.get()};
    responseSnapshot.trigger = trigger;
    responseSnapshot.selectedUser = *selected;
    responseSnapshot.associationId = preassociationManagement ? 2045 :
            actions->getTriggeredUlAssociationId();
    responseSnapshot.centerFrequency = centerFrequency;
    responseSnapshot.transmitPower = transmitPower;
    responseSnapshot.bssColor = actions->getTriggeredUlBssColor();
    responseSnapshot.triggerId = triggerId;
    responseSnapshot.fcsMode = actions->getTriggeredUlFcsMode();
    responseSnapshot.solicitingTxopDuration = solicitingTxopDuration;
    responseSnapshot.sifsTime = actions->getTriggeredUlSifsTime();
    auto preparedResponse = buildResponsePacket(
            responseSnapshot);
    responsePacket.reset(preparedResponse.packet);

    std::vector<std::unique_ptr<Packet>> rollbackPacketOwners;
    if (!originalPackets.empty()) {
        actions->validateTriggeredUlPackets(sourceQueueToken, originalPackets);
        for (auto original : originalPackets)
            rollbackPacketOwners.emplace_back(original->dup());
        try {
            for (size_t i = 0; i < originalPackets.size(); ++i)
                if (observer != nullptr)
                    observer->beforePacketCommit(i);
        }
        catch (...) {
            rollbackSequenceState(preparedSequenceNumberState);
            throw;
        }
    }

    // The PSDU starts with an A-MPDU delimiter, so its first MAC header is
    // deliberately retained from the inner MPDU instead of being re-peeked
    // through the aggregate representation by the Tx handoff.
    auto responseHeader = sourcePacket != nullptr ?
            exchange.packets.front()->peekAtFront<Ieee80211MacHeader>() :
            blockAckReqMpdu != nullptr ?
                    blockAckReqMpdu->peekAtFront<Ieee80211MacHeader>() :
                    nullMpdu->peekAtFront<Ieee80211MacHeader>();
    PreparedTriggeredUlResponse prepared;
    prepared.responsePacket = std::move(responsePacket);
    prepared.responseHeader = responseHeader;
    prepared.exchange = std::move(exchange);
    prepared.originalSequenceState = std::move(originalSequenceNumberState);
    prepared.preparedSequenceState = std::move(preparedSequenceNumberState);
    prepared.originalPackets = std::move(originalPackets);
    prepared.preparedPacketOwners = std::move(preparedPacketOwners);
    prepared.rollbackPacketOwners = std::move(rollbackPacketOwners);
    prepared.queueOrder = sourcePackets;
    prepared.preparedBlockAckReq = preparedBlockAckReq;
    prepared.blockAckReqAccessCategory = blockAckReqAc;
    prepared.hasBlockAckRequest = blockAckReqMpdu != nullptr;
    prepared.triggerId = triggerId;
    prepared.event.triggerId = triggerId;
    prepared.event.triggerType = static_cast<IIeee80211HeUlTriggerPolicy::TriggerType>(
            trigger->getTriggerType());
    prepared.event.reason = sourcePacket != nullptr ? HeTbResponseEvent::DATA_SELECTED :
            blockAckReqMpdu != nullptr ? HeTbResponseEvent::BLOCK_ACK_REQUESTED :
            trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ?
                    HeTbResponseEvent::BUFFER_STATUS_REPORTED :
            hadPendingPayload || queueBytes > 0 ? HeTbResponseEvent::NO_FITTING_PAYLOAD :
                    HeTbResponseEvent::NO_PENDING_DATA;
    prepared.event.associationId = actions->getTriggeredUlAssociationId();
    prepared.event.tid = blockAckReqMpdu != nullptr ?
            preparedBlockAckReq->getTidInfo() : selectedTid;
    prepared.event.accessCategory = blockAckReqMpdu != nullptr ?
            blockAckReqAc : selectedAc;
    prepared.event.ruIndex = selected->ruIndex;
    prepared.event.ruToneSize = selected->ruToneSize;
    prepared.event.ruToneOffset = selected->ruToneOffset;
    prepared.event.hadPendingPayload = hadPendingPayload;
    prepared.event.pendingBytes = queueBytes;
    for (auto selectedPacket : prepared.exchange.packets)
        prepared.event.selectedBytes += selectedPacket->getByteLength();
    prepared.event.reportedBytes = blockAckReqMpdu != nullptr ?
            0 : queueBytes;
    auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(responseHeader);
    prepared.event.ackPolicy = dataHeader == nullptr ? -1 : dataHeader->getAckPolicy();
    if (observer != nullptr) {
        observer->preparedResponse(prepared.responsePacket.get());
        observer->beforeHandoff(prepared.responsePacket.get());
    }
    prepared.txReservation = actions->prepareTriggeredUlHandoff(
            prepared.responsePacket.get(), prepared.responseHeader);
    if (prepared.txReservation == nullptr)
        throw cRuntimeError("HE-TB Tx preparation returned no reservation");
    return prepared;
}

HeTriggeredUlExchangeService::TriggerSelection
HeTriggeredUlExchangeService::parseTrigger(Packet *packet,
        const Ptr<const Ieee80211TriggerFrame>& trigger,
        const TriggerReceptionSnapshot& snapshot) const
{
    TriggerSelection result;
    if (packet == nullptr || trigger == nullptr) {
        result.diagnostic = "missing Trigger packet or header";
        return result;
    }
    const bool sameAssociatedBss = snapshot.associated &&
            trigger->getTransmitterAddress() == snapshot.bssid;
    bool hasUnassociatedRa = false;
    for (unsigned int i = 0; i < trigger->getUsersArraySize(); ++i) {
        const auto& user = trigger->getUsers(i);
        if (user.randomAccess && user.aid == 2045) {
            hasUnassociatedRa = true;
            break;
        }
    }
    const bool foreignUnassociated = !snapshot.associated &&
            trigger->getTransmitterAddress() != snapshot.bssid && hasUnassociatedRa;
    if (!sameAssociatedBss && !foreignUnassociated) {
        result.disposition = TriggerDisposition::FOREIGN_BSS;
        return result;
    }
    auto correlation = packet->findTag<physicallayer::Ieee80211HeTriggerCorrelationTag>();
    if (correlation == nullptr || correlation->getTriggerId() == 0) {
        result.disposition = TriggerDisposition::MISSING_CORRELATION;
        return result;
    }
    result.triggerId = correlation->getTriggerId();
    const auto triggerType = trigger->getTriggerType();
    if (triggerType != IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER &&
            triggerType != 2 &&
            triggerType != IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER &&
            triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        result.diagnostic = "unsupported Trigger type";
        return result;
    }
    if (Hz(trigger->getChannelBandwidthMhz() * 1e6) != snapshot.channelBandwidth) {
        result.disposition = TriggerDisposition::LINK_BANDWIDTH_MISMATCH;
        return result;
    }
    const auto durationEnvelope =
            physicallayer::getIeee80211HeTriggerTxTimeUpperBound(trigger->getUlLength());
    if (!durationEnvelope || durationEnvelope.txTime <= SIMTIME_ZERO ||
            trigger->getCommonDuration() != durationEnvelope.txTime) {
        result.diagnostic = "unrepresentable HE-TB response duration";
        return result;
    }
    // IEEE 802.11-2024 Table 9-47 type 2 is a recipient-side MU-BAR
    // response path. It is not gated by the STA's UL scheduling service.
    if (triggerType == 2 && snapshot.associated) {
        result.disposition = TriggerDisposition::ACCEPT;
        return result;
    }
    if (!snapshot.ulEnabled || snapshot.accessPoint ||
            (!snapshot.associated && !foreignUnassociated) ||
            (snapshot.associated && snapshot.associationId == 0)) {
        result.disposition = TriggerDisposition::INELIGIBLE_STATION;
        return result;
    }
    if (foreignUnassociated &&
            (!snapshot.receivedInHePpdu ||
             (triggerType != IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER &&
              triggerType != IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER) ||
             !snapshot.hasPendingManagement)) {
        result.disposition = TriggerDisposition::INELIGIBLE_STATION;
        return result;
    }
    if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER &&
            !snapshot.ndpFeedbackEnabled) {
        result.disposition = TriggerDisposition::UNSUPPORTED_NDP_FEEDBACK;
        return result;
    }
    if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        const int scheduledStaCount =
                IIeee80211HeUlScheduler::getNfrpScheduledStaCount(
                        snapshot.channelBandwidth,
                        trigger->getNfrpMultiplexingFlag());
        if (trigger->getNfrpFeedbackType() != 0 ||
                trigger->getNfrpStartingAid() + scheduledStaCount > 4096) {
            result.diagnostic = "invalid NFRP response resource range";
            return result;
        }
    }
    if (snapshot.twtSleeping) {
        result.disposition = TriggerDisposition::TWT_SLEEPING;
        return result;
    }
    if (!exchanges.empty()) {
        result.disposition = TriggerDisposition::EXCHANGE_PENDING;
        return result;
    }
    result.disposition = TriggerDisposition::ACCEPT;
    return result;
}

void HeTriggeredUlExchangeService::processTrigger(
        TriggerProcessingSnapshot snapshot)
{
    std::unique_ptr<Packet> receivedPacket(snapshot.packet);
    auto trigger = snapshot.trigger;
    auto parsed = parseTrigger(receivedPacket.get(), trigger, snapshot.reception);
    if (!parsed) {
        EV_WARN << "Ignoring HE UL Trigger: disposition="
                << static_cast<int>(parsed.disposition)
                << (parsed.diagnostic.empty() ? "" : ", ")
                << parsed.diagnostic << "\n";
        return;
    }
    const auto triggerId = parsed.triggerId;
    const auto associationId = snapshot.reception.associationId;
    const bool unassociated = !snapshot.reception.associated;

    auto supportsUser = [&] (const Ieee80211HeTriggerUserInfo& user) {
        const auto bandwidth = Hz(trigger->getChannelBandwidthMhz() * 1e6);
        const int nssIndex = user.numberOfSpatialStreams - 1;
        const auto& negotiated = unassociated ? snapshot.localCapabilities :
                snapshot.negotiatedCapabilities;
        return nssIndex >= 0 && nssIndex < 8 && negotiated &&
                negotiated->localTxPeerRx.valid &&
                negotiated->localTxPeerRx.ofdma &&
                negotiated->localTxPeerRx.supportedChannelWidths.count(bandwidth) != 0 &&
                negotiated->localTxPeerRx.supportedRuToneSizes.count(user.ruToneSize) != 0 &&
                negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[nssIndex] >= user.mcs &&
                (user.coding != physicallayer::HE_CODING_LDPC || negotiated->mutual.ldpc) &&
                (!user.muMimo || negotiated->localTxPeerRx.fullBandwidthUlMuMimo) &&
                !snapshot.ulMuDisabled;
    };

    // IEEE 802.11-2024 9.3.1.22.4 and 26.4.5: an addressed MU-BAR
    // Trigger solicits a Compressed Block Ack in an HE-TB PPDU.
    if (trigger->getTriggerType() == 2) {
        const Ieee80211HeTriggerUserInfo *selected = nullptr;
        for (unsigned int i = 0; i < trigger->getUsersArraySize(); ++i)
            if (trigger->getUsers(i).aid == associationId) {
                selected = &trigger->getUsers(i);
                break;
            }
        if (selected == nullptr) {
            EV_WARN << "Ignoring MU-BAR Trigger because it has no User Info for local AID "
                    << associationId << "\n";
            return;
        }
        if (!selected->muBarCompressedBitmap || selected->muBarMultiTid)
            throw cRuntimeError("Unsupported MU-BAR BlockAckReq variant");
        auto blockAck = actions->prepareTriggeredUlMuBarBlockAck(
                *selected, trigger->getTransmitterAddress());
        if (blockAck == nullptr) {
            EV_WARN << "Ignoring MU-BAR Trigger for AID " << associationId
                    << " because no recipient Block Ack agreement exists for TID "
                    << static_cast<int>(selected->muBarTidInfo) << "\n";
            return;
        }
        std::unique_ptr<Packet> response(new Packet("HE-TB-BlockAck", blockAck));
        response->insertAtBack(makeShared<Ieee80211MacTrailer>());
        auto protection = attachHeTbTxVectorFromTrigger(response.get(), *trigger,
                *selected, associationId, snapshot.reception.centerFrequency,
                snapshot.maximumTransmitPower,
                B((response->getDataLength().get<b>() + 7) / 8),
                snapshot.bssColor, triggerId, false, 0, 0, 0,
                snapshot.solicitingTxopDuration, snapshot.sifsTime);
        auto writableBlockAck = response->removeAtFront<Ieee80211CompressedBlockAck>();
        writableBlockAck->setDurationField(protection.macDurationField);
        response->insertAtFront(writableBlockAck);
        auto trailer = response->removeAtBack<Ieee80211MacTrailer>(B(4));
        trailer->setFcsMode(snapshot.fcsMode);
        if (snapshot.fcsMode == FCS_COMPUTED)
            trailer->setFcs(computeEthernetFcs(response.get(), snapshot.fcsMode));
        response->insertAtBack(trailer);
        auto reservation = actions->prepareTriggeredUlHandoff(response.get(),
                response->peekAtFront<Ieee80211CompressedBlockAck>());
        if (reservation == nullptr)
            throw cRuntimeError("MU-BAR HE-TB Tx preparation returned no reservation");
        actions->commitTriggeredUlHandoff(std::move(reservation));
        return;
    }

    Ieee80211HeTriggerUserInfo nfrpUser;
    uint8_t nfrpToneSetIndex = 0;
    uint8_t nfrpStartingStsNumber = 0;
    const Ieee80211HeTriggerUserInfo *selected = nullptr;
    if (trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        if (trigger->getNfrpFeedbackType() != 0)
            return;
        auto resource = IIeee80211HeUlScheduler::getNfrpResponseResource(
                trigger->getNfrpStartingAid(), associationId,
                Hz(trigger->getChannelBandwidthMhz() * 1e6),
                trigger->getNfrpMultiplexingFlag());
        if (!resource.scheduled) {
            EV_INFO << "Ignoring NFRP Trigger " << triggerId << ": AID "
                    << associationId << " is outside the scheduled range\n";
            return;
        }
        auto maximumRu = physicallayer::getHeEqualRuLayout(Hz(0),
                Hz(trigger->getChannelBandwidthMhz() * 1e6), 1).front();
        nfrpUser.aid = associationId;
        nfrpUser.ruIndex = maximumRu.index;
        nfrpUser.ruToneSize = maximumRu.toneSize;
        nfrpUser.ruToneOffset = maximumRu.toneOffset;
        nfrpUser.mcs = 0;
        nfrpUser.coding = physicallayer::HE_CODING_BCC;
        nfrpUser.numberOfSpatialStreams = 1;
        nfrpUser.streamStartIndex = 0;
        nfrpUser.targetRssiDbm = trigger->getNfrpTargetRssiDbm();
        nfrpUser.useMaximumTransmitPower = trigger->getNfrpUseMaximumTransmitPower();
        nfrpToneSetIndex = resource.toneSetIndex;
        nfrpStartingStsNumber = resource.startingStsNumber;
        selected = &nfrpUser;
    }

    std::vector<const Ieee80211HeTriggerUserInfo *> randomAccessUsers;
    for (unsigned int i = 0; selected == nullptr && i < trigger->getUsersArraySize(); ++i) {
        const auto& user = trigger->getUsers(i);
        const bool eligibleRandomAccess = user.randomAccess &&
                ((unassociated && user.aid == 2045) ||
                 (!unassociated && user.aid == 0));
        if (eligibleRandomAccess && supportsUser(user))
            randomAccessUsers.push_back(&user);
        else if (!unassociated && user.aid == associationId)
            selected = &user;
    }

    auto selectionSnapshot = snapshot.responseSelection;
    selectionSnapshot.unassociated = unassociated;
    selectionSnapshot.responsePeer = snapshot.reception.triggerTransmitter;
    bool randomAccess = false;
    bool randomAccessCommitted = false;
    std::optional<PreparedTriggeredUlResponse> preparedResponse;
    ResponseSelection responseSelection;

    auto trafficFor = [&] (AccessCategory accessCategory, Tid tid) {
        TidTrafficSnapshot empty;
        empty.accessCategory = accessCategory;
        empty.tid = tid;
        for (const auto& traffic : snapshot.traffic)
            if (traffic.accessCategory == accessCategory && traffic.tid == tid)
                return traffic;
        return empty;
    };
    auto makeExchange = [&] (const ResponseSelection& selection,
            Tid tid, const Ieee80211HeTriggerUserInfo& user, bool isRandomAccess) {
        Exchange exchange;
        exchange.tid = tid;
        exchange.sourceQueueToken = selection.queueToken;
        exchange.randomAccess = isRandomAccess;
        exchange.preassociationManagement = unassociated && user.aid == 2045;
        exchange.bssid = snapshot.reception.triggerTransmitter;
        exchange.associationId = exchange.preassociationManagement ? 2045 : associationId;
        exchange.associationEpoch = exchange.preassociationManagement ? 0 :
                snapshot.reception.associationEpoch;
        exchange.ru.index = user.ruIndex;
        exchange.ru.toneSize = user.ruToneSize;
        exchange.ru.toneOffset = user.ruToneOffset;
        // IEEE 802.11-2024 10.3.2.11 and 10.23.2.2 permit RXSTART
        // within SIFS plus one slot; carry that through RXEND.
        exchange.expectedResponseTime = snapshot.currentTime + snapshot.sifsTime +
                trigger->getCommonDuration() + snapshot.sifsTime +
                snapshot.maximumBlockAckTxTime + snapshot.slotTime;
        return exchange;
    };
    auto transmitPowerFor = [&] (const Ieee80211HeTriggerUserInfo& user) {
        return snapshot.triggerPathLossDb.has_value() ?
                computeIeee80211HeTbTransmitPower(snapshot.maximumTransmitPower,
                        user.targetRssiDbm, *snapshot.triggerPathLossDb,
                        user.useMaximumTransmitPower) :
                snapshot.maximumTransmitPower;
    };

    if (selected == nullptr && !randomAccessUsers.empty()) {
        // IEEE 802.11-2024 Table 9-52 and 26.5.4: construct every exact
        // candidate before the single UORA draw, then move the selected one.
        actions->setTriggeredUlRandomAccessPeer(unassociated ?
                snapshot.reception.triggerTransmitter : snapshot.reception.bssid);
        selectionSnapshot.selectedUser = *randomAccessUsers.front();
        selectionSnapshot.hasSelectedUser = true;
        selectionSnapshot.randomAccess = true;
        auto pending = selectResponse(selectionSnapshot);
        if (pending.queueToken.isValid() && pending.hasReportedTid) {
            const auto traffic = trafficFor(pending.accessCategory, pending.tid);
            int availableSlots = traffic.hasBlockAckAgreement ?
                    traffic.availableBlockAckSlots : 1;
            std::vector<PreparedTriggeredUlResponse> candidates;
            try {
                for (auto user : randomAccessUsers)
                    candidates.push_back(prepareResponse(pending.sourcePacket,
                            pending.queueToken, pending.queuePackets,
                            pending.accessCategory, pending.tid,
                            traffic.bufferedBytes, availableSlots, user, trigger,
                            triggerId, transmitPowerFor(*user),
                            snapshot.solicitingTxopDuration,
                            makeExchange(pending, pending.tid, *user, true),
                            snapshot.blockAckCandidates));
            }
            catch (const std::exception& error) {
                EV_WARN << "HE-TB random-access construction aborted before UORA commit: "
                        << error.what() << "\n";
                return;
            }
            auto uora = prepareRandomAccess(pending.accessCategory,
                    randomAccessUsers.size());
            int selectedIndex = -1;
            try {
                if (uora.attempt)
                    precommit(candidates.front());
                selectedIndex = commitRandomAccess(uora);
            }
            catch (const std::exception& error) {
                if (!candidates.empty())
                    rollback(candidates.front());
                EV_WARN << "HE-TB random-access response aborted before UORA commit: "
                        << error.what() << "\n";
                return;
            }
            if (selectedIndex >= 0) {
                selected = randomAccessUsers.at(selectedIndex);
                randomAccess = true;
                randomAccessCommitted = true;
                if (selectedIndex != 0)
                    transferPrecommit(candidates.at(selectedIndex), candidates.front());
                preparedResponse.emplace(std::move(candidates.at(selectedIndex)));
            }
            else if (uora.attempt)
                rollback(candidates.front());
        }
    }
    if (selected == nullptr) {
        EV_INFO << "Ignoring HE UL Trigger " << triggerId
                << ": this STA has no scheduled or selected random-access RU\n";
        return;
    }
    if (!supportsUser(*selected)) {
        EV_WARN << "Ignoring HE UL Trigger allocation that exceeds negotiated local-TX/peer-RX capabilities\n";
        return;
    }

    selectionSnapshot.selectedUser = *selected;
    selectionSnapshot.hasSelectedUser = true;
    selectionSnapshot.randomAccess = randomAccess;
    responseSelection = selectResponse(selectionSnapshot);
    Tid selectedTid = responseSelection.hasReportedTid ? responseSelection.tid : 0;
    if (responseSelection.sourcePacket != nullptr)
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                    responseSelection.sourcePacket->peekAtFront<Ieee80211MacHeader>()))
            selectedTid = dataHeader->getTid();
    auto traffic = trafficFor(responseSelection.accessCategory, selectedTid);
    int availableSlots = traffic.hasBlockAckAgreement ?
            traffic.availableBlockAckSlots : 0;
    if (responseSelection.sourcePacket != nullptr && !traffic.hasBlockAckAgreement)
        availableSlots = 1;
    Packet *sourcePacket = (unassociated || availableSlots != 0) ?
            responseSelection.sourcePacket : nullptr;
    uint32_t queueBytes = trigger->getTriggerType() ==
            IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER ?
            snapshot.totalBufferedBytes : traffic.bufferedBytes;
    const auto transmitPower = transmitPowerFor(*selected);

    if (trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        std::unique_ptr<Packet> response(new Packet("HE-TB-NDP-Feedback-Report"));
        auto header = makeShared<Ieee80211DataHeader>();
        header->setType(ST_QOS_NULL);
        header->setReceiverAddress(snapshot.reception.bssid);
        header->setTransmitterAddress(snapshot.localAddress);
        header->setAddress3(snapshot.reception.bssid);
        header->setToDS(true);
        header->setChunkLength(B(30));
        attachHeTbTxVectorFromTrigger(response.get(), *trigger, *selected,
                associationId, snapshot.reception.centerFrequency,
                transmitPower, B(0), snapshot.bssColor, triggerId, true,
                queueBytes > 256 ? 1 : 0, nfrpToneSetIndex,
                nfrpStartingStsNumber, snapshot.solicitingTxopDuration,
                snapshot.sifsTime);
        auto reservation = actions->prepareTriggeredUlHandoff(
                response.get(), header);
        if (reservation == nullptr)
            throw cRuntimeError("NFRP HE-TB Tx preparation returned no reservation");
        HeTbResponseEvent event;
        event.triggerId = triggerId;
        event.triggerType = IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER;
        event.reason = HeTbResponseEvent::NDP_FEEDBACK_REPORTED;
        event.associationId = associationId;
        event.tid = selectedTid;
        event.accessCategory = responseSelection.accessCategory;
        event.ruIndex = selected->ruIndex;
        event.ruToneSize = selected->ruToneSize;
        event.ruToneOffset = selected->ruToneOffset;
        event.reportedBytes = queueBytes;
        EV_INFO << "Sending HE-TB response: trigger=" << triggerId
                << ", AID=" << associationId << ", scheduled RU="
                << selected->ruIndex << ", packets=0\n";
        actions->commitTriggeredUlHandoff(std::move(reservation));
        actions->emitTriggeredUlResponse(event);
        return;
    }

    try {
        if (!preparedResponse.has_value())
            preparedResponse.emplace(prepareResponse(sourcePacket,
                    responseSelection.queueToken,
                    responseSelection.queuePackets,
                    responseSelection.accessCategory, selectedTid, queueBytes,
                    availableSlots, selected, trigger, triggerId, transmitPower,
                    snapshot.solicitingTxopDuration,
                    makeExchange(responseSelection, selectedTid, *selected,
                            randomAccess), snapshot.blockAckCandidates));
    }
    catch (const std::exception& error) {
        if (randomAccessCommitted)
            throw;
        EV_WARN << "HE-TB response preparation aborted before commit: "
                << error.what() << "\n";
        return;
    }
    auto& exact = *preparedResponse;
    auto& exchange = exact.stagedExchange.empty() ? exact.exchange :
            exact.stagedExchange.mapped();
    const auto packetCount = exchange.packets.empty() ? 1 : exchange.packets.size();
    const auto deadline = exchange.expectedResponseTime;
    EV_INFO << "Sending HE-TB response: trigger=" << triggerId
            << ", AID=" << associationId << ", "
            << (randomAccess ? "random-access" : "scheduled") << " RU="
            << selected->ruIndex << ", packets=" << packetCount << "\n";
    commit(std::move(exact));
    EV_INFO << "Committed HE-TB exchange ledger: trigger=" << triggerId
            << ", packets=" << packetCount << ", deadline=" << deadline << "\n";
}

} // namespace ieee80211
} // namespace inet
