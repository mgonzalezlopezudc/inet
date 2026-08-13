//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreementHandler.h"

#include <optional>

#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {
namespace ieee80211 {

void OriginatorBlockAckAgreementHandler::createAgreement(const Ptr<const Ieee80211AddbaRequest>& addbaRequest)
{
    OriginatorBlockAckAgreement *blockAckAgreement = new OriginatorBlockAckAgreement(addbaRequest->getReceiverAddress(), addbaRequest->getTid(), addbaRequest->getStartingSequenceNumber(), addbaRequest->getBufferSize(), addbaRequest->getAMsduSupported(), addbaRequest->getBlockAckPolicy() == 0);
    blockAckAgreement->setAssociationEpoch(++nextAssociationEpoch);
    auto agreementId = std::make_pair(addbaRequest->getReceiverAddress(), addbaRequest->getTid());
    blockAckAgreements[agreementId] = blockAckAgreement;
}

simtime_t OriginatorBlockAckAgreementHandler::computeEarliestExpirationTime()
{
    simtime_t earliestTime = SIMTIME_MAX;
    for (auto id : blockAckAgreements) {
        auto agreement = id.second;
        auto snapshot = agreement->getSnapshot();
        if (!snapshot.expirationHandlingInProgress &&
                (snapshot.isAddbaResponseReceived ||
                 (snapshot.isAddbaRequestInProgress && snapshot.isAddbaRequestSent))) {
            ASSERT(earliestTime >= 0);
            ASSERT(snapshot.expirationTime >= 0);
            earliestTime = std::min(earliestTime, snapshot.expirationTime);
        }
    }
    return earliestTime;
}

void OriginatorBlockAckAgreementHandler::blockAckAgreementExpired(IProcedureCallback *procedureCallback, IBlockAckAgreementHandlerCallback *agreementHandlerCallback)
{
    // When a timeout of BlockAckTimeout is detected, the STA shall send a DELBA frame to the
    // peer STA with the Reason Code field set to TIMEOUT and shall issue a MLME-DELBA.indication
    // primitive with the ReasonCode parameter having a value of TIMEOUT.
    // The procedure is illustrated in Figure 10-14.
    simtime_t now = simTime();
    for (auto id : blockAckAgreements) {
        auto agreement = id.second;
        auto snapshot = agreement->getSnapshot();
        if (snapshot.expirationTime <= now && snapshot.isAddbaResponseReceived &&
                !snapshot.expirationHandlingInProgress) {
            agreement->setExpirationHandlingInProgress(true);
            MacAddress receiverAddr = id.first.first;
            Tid tid = id.first.second;
            const auto& delba = buildDelba(receiverAddr, tid, 39);
            auto delbaPacket = new Packet("Delba", delba);
            procedureCallback->processMgmtFrame(delbaPacket, delba); // 39 - TIMEOUT see: Table 8-36—Reason codes
        }
        // IEEE Std 802.11-2024, 10.25.2: dot11ADDBAResponseTimeout bounds how
        // long an originator waits for the ADDBA Response before retrying.
        else if (snapshot.expirationTime <= now && snapshot.isAddbaRequestInProgress &&
                snapshot.isAddbaRequestSent) {
            EV_INFO << "ADDBA response timeout elapsed for receiver=" << id.first.first
                    << " tid=" << id.first.second << "; waking queued retry\n";
            agreement->setIsAddbaRequestInProgress(false);
        }
    }
    scheduleInactivityTimer(agreementHandlerCallback);
}

const Ptr<Ieee80211AddbaRequest> OriginatorBlockAckAgreementHandler::buildAddbaRequest(MacAddress receiverAddr, Tid tid, SequenceNumberCyclic startingSequenceNumber, IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy)
{
    auto addbaRequest = makeShared<Ieee80211AddbaRequest>();
    addbaRequest->setReceiverAddress(receiverAddr);
    addbaRequest->setTid(tid);
    addbaRequest->setAMsduSupported(blockAckAgreementPolicy->isMsduSupported());
    addbaRequest->setBlockAckTimeoutValue(blockAckAgreementPolicy->getBlockAckTimeoutValue());
    addbaRequest->setBufferSize(blockAckAgreementPolicy->getMaximumAllowedBufferSize());
    // The Block Ack Policy subfield is set to 1 for immediate Block Ack and 0 for delayed Block Ack.
    addbaRequest->setBlockAckPolicy(blockAckAgreementPolicy->isDelayedAckPolicySupported() ? 0 : 1);
    addbaRequest->setStartingSequenceNumber(startingSequenceNumber);
    return addbaRequest;
}

//
// The inactivity timer at the originator is reset when a BlockAck frame
// corresponding to the TID for which the Block Ack policy is set is received.
//
void OriginatorBlockAckAgreementHandler::processReceivedBlockAck(const Ptr<const Ieee80211BlockAck>& blockAck, IBlockAckAgreementHandlerCallback *callback)
{
    if (auto basicBlockAck = dynamicPtrCast<const Ieee80211BasicBlockAck>(blockAck)) {
        auto agreement = getAgreement(basicBlockAck->getTransmitterAddress(), basicBlockAck->getTidInfo());
        if (agreement) {
            agreement->setStartingSequenceNumber(basicBlockAck->getStartingSequenceNumber());
            agreement->calculateExpirationTime();
            scheduleInactivityTimer(callback);
        }
    }
    else if (auto compressedBlockAck = dynamicPtrCast<const Ieee80211CompressedBlockAck>(blockAck)) {
        auto agreement = getAgreement(compressedBlockAck->getTransmitterAddress(), compressedBlockAck->getTidInfo());
        if (agreement) {
            agreement->setStartingSequenceNumber(compressedBlockAck->getStartingSequenceNumber());
            agreement->calculateExpirationTime();
            scheduleInactivityTimer(callback);
        }
    }
    else if (auto multiTidBlockAck = dynamicPtrCast<const Ieee80211MultiTidBlockAck>(blockAck)) {
        unsigned int numRecords = multiTidBlockAck->getRecordsArraySize();
        for (unsigned int i = 0; i < numRecords; ++i) {
            const auto& rec = multiTidBlockAck->getRecords(i);
            auto agreement = getAgreement(multiTidBlockAck->getTransmitterAddress(), rec.tid);
            if (agreement) {
                agreement->setStartingSequenceNumber(SequenceNumberCyclic(rec.startingSequenceNumber));
                agreement->calculateExpirationTime();
            }
        }
        scheduleInactivityTimer(callback);
    }
    else if (auto multiStaBlockAck = dynamicPtrCast<const Ieee80211MultiStaBlockAck>(blockAck)) {
        unsigned int numRecords = multiStaBlockAck->getRecordsArraySize();
        for (unsigned int i = 0; i < numRecords; ++i) {
            const auto& rec = multiStaBlockAck->getRecords(i);
            auto agreement = getAgreement(multiStaBlockAck->getTransmitterAddress(), rec.tid);
            if (agreement) {
                agreement->setStartingSequenceNumber(SequenceNumberCyclic(rec.startingSequenceNumber));
                agreement->calculateExpirationTime();
            }
        }
        scheduleInactivityTimer(callback);
    }
    else
        throw cRuntimeError("Unsupported BlockAck");
}

void OriginatorBlockAckAgreementHandler::scheduleInactivityTimer(IBlockAckAgreementHandlerCallback *callback)
{
    simtime_t earliestExpirationTime = computeEarliestExpirationTime();
    if (earliestExpirationTime != SIMTIME_MAX)
        callback->scheduleInactivityTimer(earliestExpirationTime <= simTime() ?
                SIMTIME_ZERO : earliestExpirationTime - simTime());
}

OriginatorBlockAckAgreement *OriginatorBlockAckAgreementHandler::getAgreement(MacAddress receiverAddr, Tid tid)
{
    auto agreementId = std::make_pair(receiverAddr, tid);
    auto it = blockAckAgreements.find(agreementId);
    return it != blockAckAgreements.end() ? it->second : nullptr;
}

const Ptr<Ieee80211Delba> OriginatorBlockAckAgreementHandler::buildDelba(MacAddress receiverAddr, Tid tid, int reasonCode)
{
    auto delba = makeShared<Ieee80211Delba>();
    delba->setReceiverAddress(receiverAddr);
    delba->setTid(tid);
    delba->setReasonCode(reasonCode);
    // The Initiator subfield indicates if the originator or the recipient of the data is sending this frame.
    delba->setInitiator(true);
    return delba;
}

void OriginatorBlockAckAgreementHandler::terminateAgreement(MacAddress originatorAddr, Tid tid)
{
    auto agreementId = std::make_pair(originatorAddr, tid);
    auto it = blockAckAgreements.find(agreementId);
    if (it != blockAckAgreements.end()) {
        OriginatorBlockAckAgreement *agreement = it->second;
        blockAckAgreements.erase(it);
        delete agreement;
    }
}

bool OriginatorBlockAckAgreementHandler::processQueuedDataFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& dataHeader,
        IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy, IProcedureCallback *callback)
{
    auto reservation = reserveQueuedDataFramePrerequisite(packet, dataHeader,
            blockAckAgreementPolicy);
    if (!reservation)
        return false;
    auto addbaReq = reservation.getPacket()->peekAtFront<Ieee80211AddbaRequest>();
    callback->processMgmtFrame(reservation.releasePacket(), addbaReq);
    reservation.commit();
    return true;
}

IOriginatorBlockAckAgreementHandler::PrerequisiteProbe
OriginatorBlockAckAgreementHandler::probeQueuedDataFramePrerequisite(Packet *packet,
        const Ptr<const Ieee80211DataHeader>& dataHeader,
        IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy)
{
    PrerequisiteProbe probe;
    probe.packetId = packet == nullptr ? -1 : packet->getId();
    probe.receiverAddress = dataHeader->getReceiverAddress();
    probe.tid = dataHeader->getTid();
    if (probe.receiverAddress.isMulticast() || probe.receiverAddress.isBroadcast() ||
            !blockAckAgreementPolicy->isAddbaReqNeeded(packet, dataHeader))
        return probe;

    auto agreement = getAgreement(probe.receiverAddress, probe.tid);
    probe.agreementPresent = agreement != nullptr;
    if (agreement == nullptr) {
        probe.required = true;
        return probe;
    }
    auto snapshot = agreement->getSnapshot();
    probe.agreementGeneration = snapshot.generation;
    if (snapshot.isAddbaResponseReceived)
        return probe;
    if (snapshot.isAddbaRequestInProgress) {
        if (!snapshot.isAddbaRequestSent)
            return probe;
        const simtime_t retryDeadline = snapshot.expirationTime;
        if (retryDeadline < SIMTIME_ZERO || simTime() < retryDeadline)
            return probe;
    }
    probe.required = true;
    return probe;
}

IOriginatorBlockAckAgreementHandler::PrerequisiteReservation
OriginatorBlockAckAgreementHandler::reserveQueuedDataFramePrerequisite(Packet *packet,
        const Ptr<const Ieee80211DataHeader>& dataHeader,
        IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy)
{
    return reserveQueuedDataFramePrerequisite(
            probeQueuedDataFramePrerequisite(packet, dataHeader, blockAckAgreementPolicy),
            packet, dataHeader, blockAckAgreementPolicy);
}

IOriginatorBlockAckAgreementHandler::PrerequisiteReservation
OriginatorBlockAckAgreementHandler::reserveQueuedDataFramePrerequisite(
        const PrerequisiteProbe& probe, Packet *packet,
        const Ptr<const Ieee80211DataHeader>& dataHeader,
        IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy)
{
    auto currentProbe = probeQueuedDataFramePrerequisite(packet, dataHeader, blockAckAgreementPolicy);
    if (!probe.required || !currentProbe.required || probe.packetId != currentProbe.packetId ||
            probe.receiverAddress != currentProbe.receiverAddress || probe.tid != currentProbe.tid ||
            probe.agreementPresent != currentProbe.agreementPresent ||
            probe.agreementGeneration != currentProbe.agreementGeneration)
        return {};

    auto agreement = getAgreement(dataHeader->getReceiverAddress(), dataHeader->getTid());
    const bool agreementCreated = agreement == nullptr;
    std::optional<OriginatorBlockAckAgreementSnapshot> agreementSnapshot;
    if (agreement != nullptr)
        agreementSnapshot.emplace(agreement->getSnapshot());
    if (agreementSnapshot && agreementSnapshot->isAddbaResponseReceived)
        return {};

    if (agreementSnapshot && agreementSnapshot->isAddbaRequestInProgress) {
        // IEEE Std 802.11-2024, 10.25.2 and 11.5.2.2(c)-(e): an ADDBA Response
        // follows a transmitted ADDBA Request, so an unsent request has no response timeout.
        if (!agreementSnapshot->isAddbaRequestSent)
            return {};
        const simtime_t retryDeadline = agreementSnapshot->expirationTime;
        if (retryDeadline < SIMTIME_ZERO || simTime() < retryDeadline)
            return {};
    }

    const auto startingSequenceNumber = dataHeader->getSequenceNumber().isValid() ?
            dataHeader->getSequenceNumber() + 1 : SequenceNumberCyclic(0);
    auto addbaReq = buildAddbaRequest(dataHeader->getReceiverAddress(), dataHeader->getTid(),
            startingSequenceNumber, blockAckAgreementPolicy);
    if (agreement == nullptr) {
        createAgreement(addbaReq);
        agreement = getAgreement(dataHeader->getReceiverAddress(), dataHeader->getTid());
    }
    agreement->setIsAddbaRequestInProgress(true);
    agreement->setIsAddbaRequestSent(false);
    agreement->setBlockAckTimeoutValue(blockAckAgreementPolicy->computeAddbaFailureTimeout());
    auto addbaPacket = new Packet("AddbaReq", addbaReq);
    addbaPacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
    auto key = std::make_pair(dataHeader->getReceiverAddress(), dataHeader->getTid());
    return {addbaPacket, [this, key, agreementCreated, agreementSnapshot] {
        auto agreement = getAgreement(key.first, key.second);
        if (agreementCreated)
            terminateAgreement(key.first, key.second);
        else if (agreement != nullptr && agreementSnapshot)
            agreement->restoreSnapshot(*agreementSnapshot);
    }};
}

void OriginatorBlockAckAgreementHandler::processTransmittedDataFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& dataHeader, IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy, IProcedureCallback *callback)
{
    if (dataHeader->getReceiverAddress().isMulticast() || dataHeader->getReceiverAddress().isBroadcast())
        return;
    auto agreement = getAgreement(dataHeader->getReceiverAddress(), dataHeader->getTid());
    if (processQueuedDataFrame(packet, dataHeader, blockAckAgreementPolicy, callback))
        return;
    if (blockAckAgreementPolicy->isAddbaReqNeeded(packet, dataHeader) && agreement != nullptr) {
        auto agreementSnapshot = agreement->getSnapshot();
        if (agreementSnapshot.isAddbaResponseReceived)
            return;
        const simtime_t retryDeadline = agreementSnapshot.expirationTime;
        const bool hasRetryDeadline = retryDeadline >= SIMTIME_ZERO;

        if (agreementSnapshot.isAddbaRequestInProgress && (!hasRetryDeadline || simTime() < retryDeadline)) {
            EV_DETAIL << "Suppressing AddbaReq while previous request is pending for receiver="
                      << dataHeader->getReceiverAddress() << " tid=" << dataHeader->getTid()
                      << " attempts=" << agreement->getNumSentBaPolicyFrames()
                      << " retryDeadline=" << retryDeadline << "\n";
            return;
        }

        if (hasRetryDeadline && simTime() >= retryDeadline) {
            EV_INFO << "ADDBA failure timeout elapsed; retrying request for receiver="
                    << dataHeader->getReceiverAddress() << " tid=" << dataHeader->getTid()
                    << " attempts=" << agreement->getNumSentBaPolicyFrames() << "\n";
        }

        auto addbaReq = buildAddbaRequest(dataHeader->getReceiverAddress(), dataHeader->getTid(), dataHeader->getSequenceNumber() + 1, blockAckAgreementPolicy);
        agreement->setIsAddbaRequestInProgress(true);
        agreement->setIsAddbaRequestSent(false);
        agreement->setBlockAckTimeoutValue(blockAckAgreementPolicy->computeAddbaFailureTimeout());
        auto addbaPacket = new Packet("AddbaReq", addbaReq);
        callback->processMgmtFrame(addbaPacket, addbaReq);
    }
}

void OriginatorBlockAckAgreementHandler::processReceivedAddbaResp(const Ptr<const Ieee80211AddbaResponse>& addbaResp, IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy, IBlockAckAgreementHandlerCallback *callback)
{
    auto agreement = getAgreement(addbaResp->getTransmitterAddress(), addbaResp->getTid());
    if (blockAckAgreementPolicy->isAddbaReqAccepted(addbaResp, agreement)) {
        updateAgreement(agreement, addbaResp);
        scheduleInactivityTimer(callback);
    }
    else {
        // TODO send a new one?
    }
}

void OriginatorBlockAckAgreementHandler::updateAgreement(OriginatorBlockAckAgreement *agreement, const Ptr<const Ieee80211AddbaResponse>& addbaResp)
{
    agreement->setIsAddbaResponseReceived(true);
    agreement->setIsAddbaRequestInProgress(false);
    agreement->setBufferSize(addbaResp->getBufferSize());
    agreement->setBlockAckTimeoutValue(addbaResp->getBlockAckTimeoutValue());
    agreement->calculateExpirationTime();
}

void OriginatorBlockAckAgreementHandler::processTransmittedAddbaReq(
        const Ptr<const Ieee80211AddbaRequest>& addbaReq,
        IBlockAckAgreementHandlerCallback *callback)
{
    auto agreement = getAgreement(addbaReq->getReceiverAddress(), addbaReq->getTid());
    if (agreement) {
        agreement->setIsAddbaRequestSent(true);
        agreement->setIsAddbaRequestInProgress(true);
        agreement->baPolicyFrameSent();
        if (!agreement->getSnapshot().isAddbaResponseReceived)
            agreement->calculateExpirationTime();
        scheduleInactivityTimer(callback);
    }
    else
        throw cRuntimeError("Block Ack Agreement should have already been added");
}

void OriginatorBlockAckAgreementHandler::processTransmittedDelba(const Ptr<const Ieee80211Delba>& delba)
{
    terminateAgreement(delba->getReceiverAddress(), delba->getTid());
}

void OriginatorBlockAckAgreementHandler::processReceivedDelba(const Ptr<const Ieee80211Delba>& delba, IOriginatorBlockAckAgreementPolicy *blockAckAgreementPolicy)
{
    if (blockAckAgreementPolicy->isDelbaAccepted(delba))
        terminateAgreement(delba->getTransmitterAddress(), delba->getTid());
}

OriginatorBlockAckAgreementHandler::~OriginatorBlockAckAgreementHandler()
{
    for (auto it : blockAckAgreements)
        delete it.second;
}

} // namespace ieee80211
} // namespace inet
