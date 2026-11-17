// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154StandardMac.h"
#include "inet/common/InitStages.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/linklayer/ieee802154/Ieee802154InterfaceData.h"
#include "inet/linklayer/ieee802154/Ieee802154FrameCodec.h"
#include "inet/linklayer/ieee802154/Ieee802154CommandCodec.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/Simsignals.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/common/FcsInd_m.h"
#include <cmath>

namespace inet {

Define_Module(Ieee802154StandardMac);
using PhyStatus = physicallayer::Ieee802154PhyServiceStatus;
using PhyAdmission = physicallayer::Ieee802154PhyServiceAdmission;
using PhyState = physicallayer::Ieee802154PhyState;

namespace {
Ieee802154MacServiceResult macResult(const physicallayer::Ieee802154PhyServiceResult& result)
{
    if (auto abort = std::get_if<physicallayer::Ieee802154PhyServiceAbortReason>(&result)) {
        using Reason = physicallayer::Ieee802154PhyServiceAbortReason;
        return *abort == Reason::CANCELLED ? Ieee802154MacServiceAbortReason::CANCELLED :
                *abort == Reason::RESET ? Ieee802154MacServiceAbortReason::RESET : Ieee802154MacServiceAbortReason::STOPPED;
    }
    auto status = std::get<PhyStatus>(result);
    if (status == PhyStatus::SUCCESS) return Ieee802154MacStatus::SUCCESS;
    if (status == PhyStatus::READ_ONLY) return Ieee802154MacStatus::READ_ONLY;
    if (status == PhyStatus::UNSUPPORTED_ATTRIBUTE) return Ieee802154MacStatus::UNSUPPORTED_ATTRIBUTE;
    return Ieee802154MacStatus::INVALID_PARAMETER;
}

Ieee802154MacServiceAdmission macAdmission(PhyAdmission admission)
{
    switch (admission) {
        case PhyAdmission::ACCEPTED: return Ieee802154MacServiceAdmission::ACCEPTED;
        case PhyAdmission::BUSY: return Ieee802154MacServiceAdmission::BUSY;
        case PhyAdmission::UNAVAILABLE: return Ieee802154MacServiceAdmission::UNAVAILABLE;
        case PhyAdmission::INVALID_REQUEST: return Ieee802154MacServiceAdmission::INVALID_REQUEST;
        case PhyAdmission::UNSUPPORTED_OPERATION: return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    }
    throw cRuntimeError("Unknown PHY admission");
}
}

Ieee802154StandardMac::~Ieee802154StandardMac()
{
    cancelAndDelete(dataTimer);
    cancelAndDelete(ackTimer);
    cancelAndDelete(pendingTimer);
    cancelAndDelete(pollTimer);
    cancelAndDelete(scanTimer);
    cancelAndDelete(associationTimer);
    cancelAndDelete(receiverPrepareTimer);
    cancelAndDelete(receiverEndTimer);
    for (const auto& [token, original] : pendingOriginals)
        delete original.msdu;
    if (transaction)
        delete transaction->msdu;
    if (ackExchange)
        delete ackExchange->msdu;
}

int Ieee802154StandardMac::numInitStages() const { return NUM_INIT_STAGES; }

void Ieee802154StandardMac::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        initialPanCoordinator = panCoordinator = par("isPanCoordinator").boolValue();
        client.reference(this, "clientModule", true);
        managementClient.reference(this, "managementClientModule", false);
        networkInterface.reference(this, "interfaceModule", true);
        access.reference(this, "accessModule", false);
        phy.reference(this, "providerModule", false);
        dataTimer = new cMessage("dataTimer");
        ackTimer = new cMessage("ackTimer");
        ackTimer->setSchedulingPriority(1);
        pendingTimer = new cMessage("pendingTimer");
        pendingTimer->setSchedulingPriority(1); // Receive callbacks enforce expiry equality before matching.
        pollTimer = new cMessage("pollTimer");
        pollTimer->setSchedulingPriority(1);
        scanTimer = new cMessage("scanTimer");
        scanTimer->setSchedulingPriority(1);
        associationTimer = new cMessage("associationTimer");
        associationTimer->setSchedulingPriority(1);
        receiverPrepareTimer = new cMessage("receiverPrepareTimer");
        receiverPrepareTimer->setSchedulingPriority(1);
        receiverEndTimer = new cMessage("receiverEndTimer");
        receiverEndTimer->setSchedulingPriority(1);
        if (par("scanCapacity").intValue() < 1)
            throw cRuntimeError("scanCapacity must be positive");
        auto capacity = par("pendingCapacity").intValue();
        if (capacity < 1 || !std::isfinite(par("pollResponseWait").doubleValue()) || par("pollResponseWait").doubleValue() <= 0)
            throw cRuntimeError("pendingCapacity and pollResponseWait must be positive");
        pending = std::make_unique<Ieee802154PendingTransactions>(capacity);
        if (!std::isfinite(par("maxPropagationDelay").doubleValue()) || par("maxPropagationDelay").doubleValue() < 0)
            throw cRuntimeError("maxPropagationDelay must be a finite nonnegative bound");
        auto shortAddress = par("shortAddress").intValue();
        auto panId = par("panId").intValue();
        if (shortAddress < 0 || shortAddress > 65535 || panId < 0 || panId > 65535)
            throw cRuntimeError("IEEE 802.15.4 shortAddress/panId must fit 16 bits");
        auto ccaDuration = SimTime(par("initialCcaDuration"));
        auto ccaUs = ccaDuration.inUnit(SIMTIME_US);
        if (ccaUs < 1 || ccaUs > 1000000 || ccaDuration != SimTime(ccaUs,SIMTIME_US))
            throw cRuntimeError("initialCcaDuration must be an integer in 1..1000000 us");
        // IEEE 802.15.4-2024, 6.6.1: one random initial DSN per device.
        auto initialDsn = intuniform(0, 255, par("dsnRng"));
        auto initialBsn = par("enableManagement").boolValue() ? intuniform(0, 255, par("dsnRng")) : 0;
        pib = std::make_unique<Ieee802154MacPib>(Ieee802154Address(par("extendedAddress")),
                initialDsn, ccaUs,
                std::vector<Ieee802154MacSetRequest>{{"macShortAddress", uint64_t(shortAddress)}, {"macPanId", uint64_t(panId)}, {"macRxOnWhenIdle", bool(par("rxOnWhenIdle"))}}, initialBsn);
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        auto node = findContainingNode(this);
        auto status = node ? check_and_cast_nullable<NodeStatus *>(node->getSubmodule("status")) : nullptr;
        started = !status || status->getState() == NodeStatus::UP;
        publishIdentity();
    }
    else if (stage == INITSTAGE_LAST)
        restoreIdle();
}

void Ieee802154StandardMac::publishIdentity()
{
    auto extended = std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value);
    auto shortValue = std::get<uint64_t>(*pib->get("macShortAddress").value);
    auto pan = std::get<uint64_t>(*pib->get("macPanId").value);
    networkInterface->addProtocolDataIfAbsent<Ieee802154InterfaceData>()->setIdentity(
            extended, Ieee802154Address::fromShort(shortValue), pan);
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::admit(uint64_t id, bool concurrent, bool duringManagement)
{
    if (id == 0 || id <= lastRequestId)
        return Ieee802154MacServiceAdmission::INVALID_REQUEST;
    lastRequestId = id;
    if (!started || !client)
        return Ieee802154MacServiceAdmission::UNAVAILABLE;
    if (retiringRequests)
        return Ieee802154MacServiceAdmission::BUSY;
    if (resetRequest || ((startRequest || scanRequest || beaconPreparation || associationRequest || orphanResponseRequest) && !duringManagement) || ((transaction || ackExchange || managementRequest || pollRequest) && !concurrent))
        return Ieee802154MacServiceAdmission::BUSY;
    return Ieee802154MacServiceAdmission::ACCEPTED;
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::admitRadio(uint64_t id, bool concurrent, bool duringManagement)
{
    auto admission = admit(id,concurrent,duringManagement);
    return admission == Ieee802154MacServiceAdmission::ACCEPTED && !phyAvailable ?
            Ieee802154MacServiceAdmission::UNAVAILABLE : admission;
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitData(uint64_t id, Packet *packet, const Ieee802154MacDataRequest& request)
{
    Enter_Method("submitData");
    // IEEE Std 802.15.4-2024, Table 8-30: ignore IndirectTx outside the coordinator/destination predicates.
    bool indirect = request.indirectTx && panCoordinator && !request.destinationAddress.isNone();
    auto admission = indirect ? admit(id,true) : admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (pendingPanConflict || (!indirect && std::any_of(pendingPolls.begin(),pendingPolls.end(),[](const auto& poll) { return poll.device.isBroadcast(); })))
        return Ieee802154MacServiceAdmission::BUSY;
    if (!packet)
        return Ieee802154MacServiceAdmission::INVALID_REQUEST;
    if (!phy || !access)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    take(packet);
    auto frame = makeShared<Ieee802154Frame>();
    frame->setFrameVersion(1); // Table 8-30: selected LegacyTx service.
    frame->setDsn(getInteger("macDsn"));
    frame->setDestinationAddress(request.destinationAddress);
    frame->setDestinationPanId(request.destinationPanId);
    frame->setSourcePanId(getInteger("macPanId"));
    frame->setAckRequest(request.ackTx && !request.destinationAddress.isBroadcast() && !request.destinationAddress.isExtendedGroup());
    std::optional<Ieee802154MacStatus> failure;
    if (packet->getDataLength().get<b>() % 8)
        failure = Ieee802154MacStatus::INVALID_PARAMETER;
    else if (indirect && (!par("enableManagement").boolValue() || (request.destinationAddress.isExtendedGroup() && !request.destinationAddress.isBroadcast())))
        failure = Ieee802154MacStatus::UNSUPPORTED_FEATURE;
    else if (request.security.securityLevel > 7)
        failure = Ieee802154MacStatus::INVALID_PARAMETER;
    else if (request.security.securityLevel != 0)
        failure = Ieee802154MacStatus::UNSUPPORTED_SECURITY;
    else if (request.destinationAddress.isShortUnallocated())
        failure = Ieee802154MacStatus::INVALID_ADDRESS;
    else if (request.sourceAddressMode == Ieee802154Address::AddressType::SHORT) {
        auto address = Ieee802154Address::fromShort(getInteger("macShortAddress"));
        if (address.isShortBroadcast() || address.isShortUnallocated())
            failure = Ieee802154MacStatus::INVALID_ADDRESS;
        else
            frame->setSourceAddress(address);
    }
    else if (request.sourceAddressMode == Ieee802154Address::AddressType::EXTENDED)
        frame->setSourceAddress(std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value));
    else if (request.sourceAddressMode != Ieee802154Address::AddressType::NONE)
        failure = Ieee802154MacStatus::INVALID_PARAMETER;
    if (frame->getSourceAddress().isNone() && request.destinationAddress.isNone())
        failure = Ieee802154MacStatus::INVALID_ADDRESS;
    frame->setPanCompression(!frame->getSourceAddress().isNone() && !request.destinationAddress.isNone() && frame->getSourcePanId() == request.destinationPanId);
    if (!failure) {
        try {
            std::vector<uint8_t> bytes;
            if (packet->getDataLength() > b(0)) {
                auto chunk = packet->peekDataAsBytes();
                bytes = chunk->getBytes();
            }
            size_t headerLength = 5 + (request.destinationAddress.isNone() ? 0 : request.destinationAddress.isShort() ? 4 : 10) +
                    (frame->getSourceAddress().isNone() ? 0 : (frame->getPanCompression() ? 0 : 2) + (frame->getSourceAddress().isShort() ? 2 : 8));
            if (headerLength + bytes.size() > 127)
                failure = Ieee802154MacStatus::FRAME_TOO_LONG;
            else {
                frame->setPayloadArraySize(bytes.size());
                for (size_t i = 0; i < bytes.size(); ++i)
                    frame->setPayload(i, bytes[i]);
                Ieee802154FrameCodec::finalize(*frame);
            }
        }
        catch (const cRuntimeError&) {
            failure = Ieee802154MacStatus::INVALID_PARAMETER;
        }
    }
    if (failure) {
        delete packet;
        Ieee802154MacDataConfirm confirm(request.msduHandle, *failure);
        client->handleDataConfirm(id, confirm);
    }
    else if (indirect) {
        // Table 8-36: BO=15 persistence unit is 960 O-QPSK symbols.
        auto expiresAt = simTime() + SimTime(15360, SIMTIME_US) * getInteger("macTransactionPersistenceTime");
        auto queuePan = uint16_t(request.destinationAddress.isBroadcast() ? getInteger("macPanId") : request.destinationPanId);
        if (!pending->insert({id, queuePan, canonicalAddress(queuePan,request.destinationAddress), frame, expiresAt})) {
            delete packet;
            client->handleDataConfirm(id, Ieee802154MacDataConfirm(request.msduHandle, Ieee802154MacStatus::TRANSACTION_OVERFLOW));
        }
        else {
            pendingOriginals.emplace(id, PendingOriginal{packet, request.msduHandle});
            pib->set("macDsn", uint64_t((frame->getDsn() + 1) & 255));
            expirePending();
        }
    }
    else {
        pib->set("macDsn", uint64_t((frame->getDsn() + 1) & 255));
        transaction = Transaction{id, packet, request.msduHandle, frame};
        startData();
    }
    return Ieee802154MacServiceAdmission::ACCEPTED;
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitGet(uint64_t id, const Ieee802154MacGetRequest& request)
{
    Enter_Method("submitGet");
    auto admission = admit(id);
    if (admission == Ieee802154MacServiceAdmission::ACCEPTED) {
        if (phy && request.attribute.rfind("phy",0) == 0) {
            if (phyOperation)
                return Ieee802154MacServiceAdmission::BUSY;
            managementRequest = ManagementRequest{id,request.attribute,{}};
            auto internalId = allocatePhyId();
            phyOperation = PhyOperation{internalId,PhyPurpose::MANAGEMENT_GET};
            admission = macAdmission(phy->submitGet(internalId,{request.attribute}));
            if (admission != Ieee802154MacServiceAdmission::ACCEPTED) {
                phyOperation.reset(); managementRequest.reset();
            }
        }
        else {
            auto confirm = pib->get(request.attribute);
            client->handleGetConfirm(id, confirm);
        }
    }
    return admission;
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitSet(uint64_t id, const Ieee802154MacSetRequest& request)
{
    Enter_Method("submitSet");
    auto admission = admit(id);
    if (admission == Ieee802154MacServiceAdmission::ACCEPTED) {
        if (phy && request.attribute.rfind("phy",0) == 0) {
            if (phyOperation)
                return Ieee802154MacServiceAdmission::BUSY;
            auto value = std::visit([](const auto& item) -> physicallayer::Ieee802154PhyPibValue { return item; },request.value);
            managementRequest = ManagementRequest{id,request.attribute,value};
            auto internalId = allocatePhyId();
            phyOperation = PhyOperation{internalId,PhyPurpose::MANAGEMENT_SET};
            admission = macAdmission(phy->submitSet(internalId,{request.attribute,value}));
            if (admission != Ieee802154MacServiceAdmission::ACCEPTED) {
                phyOperation.reset(); managementRequest.reset();
            }
        }
        else {
            Ieee802154MacSetConfirm confirm(request.attribute, pib->set(request.attribute, request.value));
            publishIdentity();
            client->handleSetConfirm(id, confirm);
            restoreIdle();
        }
    }
    return admission;
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitReset(uint64_t id, const Ieee802154MacResetRequest& request)
{
    Enter_Method("submitReset");
    auto admission = admit(id, true, true);
    if (admission == Ieee802154MacServiceAdmission::ACCEPTED) {
        resetRequest = ResetRequest{id, request.setDefaultPib};
        broadcastReceivePending = false;
        clearReceiverWindow();
        if (rxEnableRequest) {
            auto receiverId = *rxEnableRequest;
            rxEnableRequest.reset();
            if (managementClient)
                managementClient->handleRxEnableConfirm(receiverId, {Ieee802154MacServiceAbortReason::RESET,{}});
        }
        beaconResponses = 0;
        pendingPanConflict.reset();
        discardAck();
        if (associationRequest)
            completeAssociate(Ieee802154MacServiceAbortReason::RESET);
        if (orphanResponseRequest)
            completeOrphanResponse(Ieee802154MacServiceAbortReason::RESET);
        if (beaconPreparation)
            completeBeaconPreparation(Ieee802154MacServiceAbortReason::RESET);
        if (startRequest)
            completeStart(Ieee802154MacServiceAbortReason::RESET);
        if (managementRequest)
            completeManagement(Ieee802154MacServiceAbortReason::RESET);
        if (transaction && transaction->kind != Transaction::Kind::SCAN_REQUEST)
            completeData(Ieee802154MacServiceAbortReason::RESET);
        if (pollRequest)
            completePoll(Ieee802154MacServiceAbortReason::RESET);
        clearPending(Ieee802154MacServiceAbortReason::RESET);
        resetRequest->retiring = false;
        auto deferredResult = resetRequest->deferredResult;
        if (deferredResult)
            finishReset(*deferredResult);
        else if (!started)
            finishReset(Ieee802154MacServiceAbortReason::STOPPED);
        if (!resetRequest)
            return admission;
        if (scanRequest)
            finishScan(Ieee802154MacServiceAbortReason::RESET);
        else
            beginResetPhy();
    }
    return admission;
}

Ieee802154MacServiceCancelResult Ieee802154StandardMac::cancel(uint64_t id)
{
    Enter_Method("cancel");
    if (id == 0)
        return Ieee802154MacServiceCancelResult::NOT_PENDING;
    if (startRequest && startRequest->id == id)
        return Ieee802154MacServiceCancelResult::NOT_CANCELLABLE;
    if (beaconPreparation && *beaconPreparation == id)
        return Ieee802154MacServiceCancelResult::NOT_CANCELLABLE;
    if (orphanResponseRequest && orphanResponseRequest->id == id && phyOperation && phyOperation->purpose == PhyPurpose::ORPHAN_CHANNEL_GET)
        return Ieee802154MacServiceCancelResult::NOT_CANCELLABLE;
    if (resetRequest && resetRequest->id == id)
        return Ieee802154MacServiceCancelResult::NOT_CANCELLABLE;
    if (associationRequest && associationRequest->id == id) {
        if (resetRequest || !started || (phyOperation && phyOperation->purpose == PhyPurpose::ASSOCIATE_CHANNEL))
            return Ieee802154MacServiceCancelResult::NOT_CANCELLABLE;
        completeAssociate(Ieee802154MacServiceAbortReason::CANCELLED);
        return Ieee802154MacServiceCancelResult::CANCELLED;
    }
    if (scanRequest && scanRequest->id == id) {
        if (scanRequest->closing || resetRequest || !started)
            return Ieee802154MacServiceCancelResult::NOT_CANCELLABLE;
        finishScan(Ieee802154MacServiceAbortReason::CANCELLED);
        return Ieee802154MacServiceCancelResult::CANCELLED;
    }
    if ((resetRequest || !started) && (pendingOriginals.count(id) || (pollRequest && pollRequest->id == id) ||
            (transaction && transaction->id == id) || (managementRequest && managementRequest->id == id)))
        return Ieee802154MacServiceCancelResult::NOT_CANCELLABLE;
    if (managementRequest && managementRequest->id == id) {
        phyOperation.reset();
        if (phy) phy->submitReset(allocatePhyId(),false);
        phyState = PhyState::OFF;
        completeManagement(Ieee802154MacServiceAbortReason::CANCELLED);
        return Ieee802154MacServiceCancelResult::CANCELLED;
    }
    if (pollRequest && pollRequest->id == id && !transaction) {
        completePoll(Ieee802154MacServiceAbortReason::CANCELLED);
        return Ieee802154MacServiceCancelResult::CANCELLED;
    }
    if (pendingOriginals.count(id) && (!transaction || transaction->id != id)) {
        completePending(id, Ieee802154MacServiceAbortReason::CANCELLED);
        return Ieee802154MacServiceCancelResult::CANCELLED;
    }
    if (!transaction || transaction->id != id)
        return Ieee802154MacServiceCancelResult::NOT_PENDING;
    retireDataRadio();
    completeData(Ieee802154MacServiceAbortReason::CANCELLED);
    return Ieee802154MacServiceCancelResult::CANCELLED;
}

bool Ieee802154StandardMac::handleOperationStage(LifecycleOperation *operation, IDoneCallback *)
{
    Enter_Method("handleOperationStage");
    auto stage = operation->getCurrentStage();
    if (dynamic_cast<ModuleStartOperation *>(operation)) {
        if (stage == ModuleStartOperation::STAGE_LINK_LAYER)
        {
            started = true;
            restoreIdle();
        }
    }
    else if (dynamic_cast<ModuleStopOperation *>(operation)) {
        if (stage == ModuleStopOperation::STAGE_LOCAL)
            stop();
    }
    else if (dynamic_cast<ModuleCrashOperation *>(operation))
        stop();
    else
        throw cRuntimeError("Unsupported lifecycle operation");
    return true;
}

uint64_t Ieee802154StandardMac::getInteger(const char *name) const { return std::get<uint64_t>(*pib->get(name).value); }
bool Ieee802154StandardMac::getBoolean(const char *name) const { return std::get<bool>(*pib->get(name).value); }

void Ieee802154StandardMac::requestState(PhyState state, PhyPurpose purpose)
{
    auto id = allocatePhyId();
    phyOperation = PhyOperation{id, purpose};
    auto admission = phy->submitState(id, state);
    if (admission != PhyAdmission::ACCEPTED) {
        phyOperation.reset();
        if (purpose == PhyPurpose::ACK_READY) {
            finishAck(false);
            return;
        }
        if (scanRequest) {
            scanRequest->confirm.result = Ieee802154MacServiceAbortReason::STOPPED;
            completeScan();
            return;
        }
        if (transaction)
            completeData(admission == PhyAdmission::UNAVAILABLE ? Ieee802154MacServiceResult(Ieee802154MacServiceAbortReason::STOPPED) :
                    Ieee802154MacServiceResult(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE));
    }
}

void Ieee802154StandardMac::restoreIdle()
{
    if (!started || !phyAvailable || resetRequest || retiringRequests || transaction || ackExchange || managementRequest || startRequest || scanRequest || beaconPreparation || orphanResponseRequest || phyOperation || !phy)
        return;
    if (beaconResponses) {
        --beaconResponses;
        transmitBeacon(0,true);
        return;
    }
    if (pendingPanConflict) {
        if (!panCoordinator && getBoolean("macAssociatedPanCoord") && pendingPanConflict->panId == getInteger("macPanId") &&
                pendingPanConflict->coordinator == std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value)) {
            transmitPanConflict();
            return;
        }
        pendingPanConflict.reset();
    }
    auto target = getBoolean("macRxOnWhenIdle") || getBoolean("macPromiscuousMode") || (pollRequest && pollRequest->waiting) || receiverWantsRx() || broadcastReceivePending ? PhyState::RECEIVE : PhyState::OFF;
    if (target != phyState)
        requestState(target, PhyPurpose::IDLE);
    if (receiverWantsRx() && phyState == PhyState::RECEIVE && !receiverWindow->startsAt) {
        receiverWindow->startsAt = simTime();
        if (receiverWindow->duration != 0xffffff)
            scheduleAt(simTime() + SimTime(receiverWindow->duration * int64_t(16),SIMTIME_US),receiverEndTimer);
    }
}

void Ieee802154StandardMac::startData()
{
    if (!transaction || resetRequest || !started || !phyAvailable || retiringRequests || ackExchange)
        return;
    if (!client) {
        completeData(Ieee802154MacServiceAbortReason::STOPPED);
        return;
    }
    if (simTime() < eligibleAt) {
        transaction->phase = Phase::WAIT_IFS;
        if (!dataTimer->isScheduled())
            scheduleAt(eligibleAt, dataTimer);
        if (!phyOperation && phyState != PhyState::RECEIVE)
            requestState(PhyState::RECEIVE, PhyPurpose::IDLE);
    }
    else {
        transaction->phase = Phase::WAIT_RX;
        if (phyOperation)
            return;
        if (phyState != PhyState::RECEIVE)
            requestState(PhyState::RECEIVE, PhyPurpose::RX_FOR_ACCESS);
        else
            startAccess();
    }
}

void Ieee802154StandardMac::startAccess()
{
    transaction->phase = Phase::ACCESS;
    ccaDeferred = false;
    if (accessId == UINT64_MAX)
        throw cRuntimeError("IEEE 802.15.4 internal access request IDs exhausted");
    transaction->accessId = ++accessId;
    Ieee802154AccessRequest request{uint8_t(getInteger("macMinBe")), uint8_t(getInteger("macMaxBe")),
            uint8_t(getInteger("macMaxCsmaBackoffs")), SimTime(16, SIMTIME_US) * getInteger("macUnitBackoffPeriod")};
    if (access->start(transaction->accessId, request) != Ieee802154MacServiceAdmission::ACCEPTED)
        completeData(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE);
}

void Ieee802154StandardMac::completeData(const Ieee802154MacServiceResult& result)
{
    auto completed = *transaction;
    transaction.reset();
    cancelEvent(dataTimer);
    cancelEvent(ackTimer);
    ccaDeferred = false;
    access->cancel(completed.accessId, Ieee802154MacServiceAbortReason::CANCELLED);
    delete completed.msdu;
    if ((completed.kind == Transaction::Kind::BEACON || completed.kind == Transaction::Kind::AUTO_BEACON) &&
            result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) && completed.frame->getFramePending())
        pendingPolls.push_front({uint16_t(completed.frame->getSourcePanId()),Ieee802154Address::fromShort(0xffff),simTime()});
    if (completed.kind == Transaction::Kind::PAN_CONFLICT) {
        auto conflict = pendingPanConflict;
        pendingPanConflict.reset();
        if (conflict && started && !resetRequest && getBoolean("macAssociatedPanCoord") &&
                conflict->panId == getInteger("macPanId") && conflict->coordinator ==
                std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value) &&
                result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) && managementClient)
            managementClient->handleSyncLossIndication({Ieee802154SyncLossReason::PAN_ID_CONFLICT,
                    conflict->panId,conflict->channel,conflict->coordinator,{},{}});
    }
    else if (completed.kind == Transaction::Kind::ORPHAN_RESPONSE) {
        if (orphanResponseRequest)
            completeOrphanResponse(result);
    }
    else if (completed.kind == Transaction::Kind::DISASSOCIATE) {
        if (disassociationRequest) {
            auto request = *disassociationRequest;
            disassociationRequest.reset();
            completeDisassociate(request.first,request.second,result);
        }
    }
    else if (completed.kind == Transaction::Kind::ASSOCIATE_REQUEST) {
        if (associationRequest) {
            if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS))
                scheduleAt(simTime() + SimTime(15360,SIMTIME_US) * getInteger("macResponseWaitTime"),associationTimer);
            else
                completeAssociate(result);
        }
    }
    else if (completed.kind == Transaction::Kind::BEACON) {
        if (managementClient)
            managementClient->handleBeaconConfirm(completed.id,{result});
    }
    else if (completed.kind == Transaction::Kind::AUTO_BEACON) {
        // No external request owns an automatic response.
    }
    else if (completed.kind == Transaction::Kind::SCAN_REQUEST) {
        if (scanRequest && !scanRequest->closing && !resetRequest && started) {
            if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS)) {
                scanRequest->requestSent = true;
                requestState(PhyState::RECEIVE,PhyPurpose::SCAN_RX);
            }
            else if (std::holds_alternative<Ieee802154MacServiceAbortReason>(result))
                finishScan(result);
            else {
                scanRequest->confirm.unscannedChannels.push_back(scanRequest->parameters.channels[scanRequest->index++]);
                beginScanChannel();
            }
        }
    }
    else if (completed.kind == Transaction::Kind::POLL) {
        if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) && completed.ackFramePending.value_or(false)) {
            pollRequest->waiting = true;
            pollRequest->deadline = simTime() + SimTime(par("pollResponseWait"));
            scheduleAt(pollRequest->deadline, pollTimer);
        }
        else
            completePoll(result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) ?
                    Ieee802154MacServiceResult(Ieee802154MacStatus::NO_DATA) : result);
    }
    else if (completed.kind == Transaction::Kind::INDIRECT) {
        if (completed.pendingDevice && completed.pendingDevice->second.isBroadcast() &&
                result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) &&
                pending->count(completed.pendingDevice->first,completed.pendingDevice->second,simTime()) > 1)
            pendingPolls.push_front({completed.pendingDevice->first,completed.pendingDevice->second,simTime()});
        if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) || std::holds_alternative<Ieee802154MacServiceAbortReason>(result) ||
                result == Ieee802154MacServiceResult(Ieee802154MacStatus::TRANSACTION_EXPIRED))
            completePending(completed.id, result, completed.backoffs, completed.ackFramePending, completed.ackRssi);
        else {
            // 6.6.3.4: a new poll after the failed attempt is required; no direct retry loop.
            pendingPolls.erase(std::remove_if(pendingPolls.begin(), pendingPolls.end(), [&](const auto& poll) {
                return completed.pendingDevice && poll.panId == completed.pendingDevice->first &&
                        poll.device == completed.pendingDevice->second && poll.receivedAt <= simTime();
            }), pendingPolls.end());
        }
    }
    else if (client) {
        Ieee802154MacDataConfirm confirm(completed.handle, result);
        if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS))
            confirm.numBackoffs = completed.backoffs;
        confirm.ackFramePending = completed.ackFramePending;
        confirm.ackRssi = completed.ackRssi;
        client->handleDataConfirm(completed.id, confirm);
    }
    startPendingResponse();
    restoreIdle();
}

void Ieee802154StandardMac::finishReset(const Ieee802154MacServiceResult& result)
{
    if (!resetRequest)
        return;
    if (resetRequest->retiring || pendingCallbacksActive) {
        resetRequest->deferredResult = result;
        return;
    }
    auto request = *resetRequest;
    if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS)) {
        auto defaultDsn = request.defaults ? intuniform(0, 255, par("dsnRng")) : 0;
        auto defaultBsn = request.defaults && par("enableManagement").boolValue() ? intuniform(0, 255, par("dsnRng")) : 0;
        pib->reset(request.defaults, defaultDsn, defaultBsn);
        if (request.defaults)
            panCoordinator = initialPanCoordinator;
        if (request.defaults)
            associatedDevices.clear();
        publishIdentity();
        phyState = PhyState::OFF;
        eligibleAt = simTime();
    }
    resetRequest.reset();
    if (client)
        client->handleResetConfirm(request.id, Ieee802154MacResetConfirm(request.defaults, result));
    restoreIdle();
}

void Ieee802154StandardMac::stop()
{
    started = false;
    broadcastReceivePending = false;
    clearReceiverWindow();
    if (rxEnableRequest) {
        auto receiverId = *rxEnableRequest;
        rxEnableRequest.reset();
        if (managementClient)
            managementClient->handleRxEnableConfirm(receiverId, {Ieee802154MacServiceAbortReason::STOPPED,{}});
    }
    beaconResponses = 0;
    pendingPanConflict.reset();
    discardAck();
    if (associationRequest)
        completeAssociate(Ieee802154MacServiceAbortReason::STOPPED);
    if (orphanResponseRequest)
        completeOrphanResponse(Ieee802154MacServiceAbortReason::STOPPED);
    if (beaconPreparation)
        completeBeaconPreparation(Ieee802154MacServiceAbortReason::STOPPED);
    if (startRequest)
        completeStart(Ieee802154MacServiceAbortReason::STOPPED);
    if (managementRequest)
        completeManagement(Ieee802154MacServiceAbortReason::STOPPED);
    if (transaction && transaction->kind != Transaction::Kind::SCAN_REQUEST)
        completeData(Ieee802154MacServiceAbortReason::STOPPED);
    if (pollRequest)
        completePoll(Ieee802154MacServiceAbortReason::STOPPED);
    clearPending(Ieee802154MacServiceAbortReason::STOPPED);
    if (scanRequest)
        finishScan(Ieee802154MacServiceAbortReason::STOPPED);
    phyOperation.reset();
    if (phy)
        phy->submitReset(allocatePhyId(), false);
    phyState = PhyState::OFF;
    finishReset(Ieee802154MacServiceAbortReason::STOPPED);
}

void Ieee802154StandardMac::handleMessage(cMessage *message)
{
    if (message == receiverPrepareTimer) {
        if (receiverWindow && receiverWindow->duration == 0)
            finishReceiverWindow();
        else
            restoreIdle();
        return;
    }
    if (message == receiverEndTimer) {
        finishReceiverWindow();
        return;
    }
    if (message == associationTimer) {
        if (associationRequest) {
            Ieee802154MacPollRequest request;
            request.coordinatorPanId = associationRequest->parameters.coordinatorPanId;
            request.coordinatorAddress = associationRequest->parameters.coordinatorAddress;
            pollRequest = PollRequest{associationRequest->id,request};
            pollRequest->association = true;
            startPoll();
        }
        return;
    }
    if (message == scanTimer) {
        if (scanRequest) {
            scanRequest->index++;
            beginScanChannel();
        }
        return;
    }
    if (message == pendingTimer) {
        expirePending();
        return;
    }
    if (message == pollTimer) {
        completePoll(Ieee802154MacStatus::NO_DATA);
        return;
    }
    if (message == ackTimer) {
        ackTimeout();
        return;
    }
    if (message != dataTimer)
        throw cRuntimeError("Unexpected standard MAC message");
    if (ccaDeferred)
        performCca();
    else
        startData();
}

void Ieee802154StandardMac::handleCcaRequest(uint64_t id)
{
    Enter_Method("handleCcaRequest");
    if (!transaction || transaction->accessId != id || transaction->phase != Phase::ACCESS)
        return;
    ccaDeferred = true;
    performCca();
}

void Ieee802154StandardMac::handleAvailabilityIndication(const physicallayer::Ieee802154PhyAvailabilityIndication& indication)
{
    Enter_Method("handleAvailabilityIndication");
    phyAvailable = indication.available;
    phyState = indication.state;
    if (!started)
        return;
    if (phyAvailable) {
        restoreIdle();
        return;
    }
    bool outerRetirement = retiringRequests;
    retiringRequests = true;
    phyOperation.reset();
    pendingPolls.clear();
    broadcastReceivePending = false;
    if (ackExchange)
        finishAck(false);
    if (associationRequest)
        completeAssociate(Ieee802154MacServiceAbortReason::STOPPED);
    if (orphanResponseRequest)
        completeOrphanResponse(Ieee802154MacServiceAbortReason::STOPPED);
    if (beaconPreparation)
        completeBeaconPreparation(Ieee802154MacServiceAbortReason::STOPPED);
    if (startRequest)
        completeStart(Ieee802154MacServiceAbortReason::STOPPED);
    if (managementRequest)
        completeManagement(Ieee802154MacServiceAbortReason::STOPPED);
    if (transaction)
        completeData(Ieee802154MacServiceAbortReason::STOPPED);
    if (pollRequest)
        completePoll(Ieee802154MacServiceAbortReason::STOPPED);
    if (scanRequest)
        finishScan(Ieee802154MacServiceAbortReason::STOPPED);
    finishReset(Ieee802154MacServiceAbortReason::STOPPED);
    retiringRequests = outerRetirement;
    restoreIdle();
}

void Ieee802154StandardMac::performCca()
{
    if (!transaction || !ccaDeferred || ackExchange || phyOperation)
        return;
    if (simTime() < eligibleAt) {
        if (!dataTimer->isScheduled())
            scheduleAt(eligibleAt, dataTimer);
        return;
    }
    if (phyState != PhyState::RECEIVE) {
        requestState(PhyState::RECEIVE, PhyPurpose::IDLE);
        return;
    }
    ccaDeferred = false;
    auto internalId = allocatePhyId();
    phyOperation = PhyOperation{internalId, PhyPurpose::CCA};
    auto admission = phy->submitCca(internalId);
    if (admission != PhyAdmission::ACCEPTED) {
        phyOperation.reset();
        completeData(admission == PhyAdmission::UNAVAILABLE ? Ieee802154MacServiceResult(Ieee802154MacServiceAbortReason::STOPPED) :
                Ieee802154MacServiceResult(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE));
    }
}

void Ieee802154StandardMac::handleAccessConfirm(uint64_t id, const Ieee802154MacServiceResult& result, uint8_t count)
{
    Enter_Method("handleAccessConfirm");
    if (!transaction || transaction->accessId != id || transaction->phase != Phase::ACCESS)
        return;
    if (result != Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS))
        completeData(result);
    else {
        transaction->backoffs = std::min<int>(255, transaction->backoffs + count);
        transaction->phase = Phase::WAIT_TX;
        requestState(PhyState::TRANSMIT, PhyPurpose::TX_READY);
    }
}

void Ieee802154StandardMac::handleStateConfirm(uint64_t id, const physicallayer::Ieee802154PhyStateConfirm& confirm)
{
    Enter_Method("handleStateConfirm");
    if (!phyOperation || phyOperation->id != id)
        return;
    auto purpose = phyOperation->purpose;
    phyOperation.reset();
    if (purpose == PhyPurpose::SCAN_REALIGN_OFF && scanRequest && scanRequest->realignment) {
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS))
            finishScan(macResult(confirm.result));
        else {
            phyState = *confirm.state;
            auto internalId = allocatePhyId();
            auto target = *scanRequest->originalChannel;
            target.channel = scanRequest->realignment->first.channelNumber;
            phyOperation = PhyOperation{internalId,PhyPurpose::SCAN_REALIGN_CHANNEL};
            if (phy->submitSet(internalId,{"phyCurrentChannelInfo",target}) != PhyAdmission::ACCEPTED) {
                phyOperation.reset();
                finishScan(Ieee802154MacServiceAbortReason::STOPPED);
            }
        }
        return;
    }
    if ((purpose == PhyPurpose::SCAN_OFF || purpose == PhyPurpose::SCAN_RESTORE_OFF) && scanRequest) {
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
            scanRequest->confirm.result = macResult(confirm.result);
            completeScan();
        }
        else {
            phyState = *confirm.state;
            setScanChannel(purpose == PhyPurpose::SCAN_RESTORE_OFF);
        }
        return;
    }
    if (purpose == PhyPurpose::SCAN_RX && scanRequest) {
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS))
            finishScan(macResult(confirm.result));
        else {
            phyState = *confirm.state;
            scanRequest->listeningSince = simTime();
            scanRequest->deadline = simTime() + (scanRequest->parameters.type == Ieee802154ScanType::ORPHAN ?
                    SimTime(15360,SIMTIME_US) * getInteger("macResponseWaitTime") :
                    SimTime(16,SIMTIME_US) * scanRequest->parameters.duration);
            if (scanRequest->parameters.type == Ieee802154ScanType::ED) {
                scanRequest->energyPeak = 0;
                measureScanEnergy();
            }
            else
                scheduleAt(scanRequest->deadline, scanTimer);
        }
        return;
    }
    if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
        if (purpose == PhyPurpose::ACK_READY) {
            if (ackExchange)
                finishAck(false);
            return;
        }
        if (transaction)
            completeData(std::holds_alternative<physicallayer::Ieee802154PhyServiceAbortReason>(confirm.result) ?
                    macResult(confirm.result) : Ieee802154MacServiceResult(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE));
        return;
    }
    phyState = *confirm.state;
    if (purpose == PhyPurpose::ACK_READY)
        startAck();
    else if (purpose == PhyPurpose::RX_FOR_ACCESS && transaction)
        startAccess();
    else if (purpose == PhyPurpose::TX_READY && transaction) {
        auto packet = new Packet("IEEE802154-DATA");
        packet->insertAtBack(transaction->frame);
        packet->addTag<PacketProtocolTag>()->setProtocol(&Protocol::ieee802154);
        auto internalId = allocatePhyId();
        phyOperation = PhyOperation{internalId, PhyPurpose::TRANSMIT};
        transaction->phase = Phase::TRANSMIT;
        emit(packetSentToLowerSignal, packet);
        if (!phyOperation || phyOperation->id != internalId || !transaction) {
            delete packet;
            return;
        }
        physicallayer::Ieee802154PhyTransmitRequest request;
        request.destination = transaction->frame->getDestinationAddress();
        if (phy->submitTransmit(internalId, packet, request) != PhyAdmission::ACCEPTED) {
            delete packet;
            phyOperation.reset();
            completeData(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE);
        }
    }
    else if (purpose == PhyPurpose::IDLE || purpose == PhyPurpose::ACK_LISTEN)
        continueData();
}

void Ieee802154StandardMac::handleCcaConfirm(uint64_t id, const physicallayer::Ieee802154PhyCcaConfirm& confirm)
{
    Enter_Method("handleCcaConfirm");
    if (!phyOperation || phyOperation->id != id)
        return;
    phyOperation.reset();
    if (!transaction)
        return;
    if (confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS))
        access->handleCcaResult(transaction->accessId, *confirm.outcome == physicallayer::Ieee802154PhyCcaOutcome::IDLE);
    else
        completeData(std::holds_alternative<physicallayer::Ieee802154PhyServiceAbortReason>(confirm.result) ?
                macResult(confirm.result) : Ieee802154MacServiceResult(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE));
}

void Ieee802154StandardMac::handleTransmitStarted(uint64_t, const physicallayer::Ieee802154PhyTransmitStart&) { Enter_Method("handleTransmitStarted"); }

void Ieee802154StandardMac::handleTransmitConfirm(uint64_t id, const physicallayer::Ieee802154PhyTransmitConfirm& confirm)
{
    Enter_Method("handleTransmitConfirm");
    if (!phyOperation || phyOperation->id != id)
        return;
    auto purpose = phyOperation->purpose;
    phyOperation.reset();
    if (purpose == PhyPurpose::ACK_TRANSMIT) {
        finishAck(confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS));
        return;
    }
    if (!transaction)
        return;
    if (confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
        eligibleAt = std::max(eligibleAt, simTime() + interframeSpacing(transaction->frame->getChunkLength()));
        if (transaction->frame->getAckRequest()) {
            transaction->phase = Phase::WAIT_ACK;
            transaction->transmittedEnd = confirm.timing->ppduEnd;
            transaction->ackDeadline = confirm.timing->ppduEnd + 2 * SimTime(par("maxPropagationDelay")) + SimTime(544, SIMTIME_US);
            scheduleAt(transaction->ackDeadline, ackTimer);
            requestState(PhyState::RECEIVE, PhyPurpose::ACK_LISTEN);
        }
        else
            completeData(Ieee802154MacStatus::SUCCESS);
    }
    else
        completeData(std::holds_alternative<physicallayer::Ieee802154PhyServiceAbortReason>(confirm.result) ?
                macResult(confirm.result) : Ieee802154MacServiceResult(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE));
}

void Ieee802154StandardMac::handleResetConfirm(uint64_t id, const physicallayer::Ieee802154PhyResetConfirm& confirm)
{
    Enter_Method("handleResetConfirm");
    if (!phyOperation || phyOperation->id != id || phyOperation->purpose != PhyPurpose::RESET)
        return;
    phyOperation.reset();
    finishReset(confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) ?
            Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) : Ieee802154MacServiceResult(Ieee802154MacServiceAbortReason::STOPPED));
}

void Ieee802154StandardMac::handleReceiveIndication(Packet *packet, const physicallayer::Ieee802154PhyReceiveIndication& quality)
{
    Enter_Method("handleReceiveIndication");
    take(packet);
    bool consumed = consumeReceiverWindow();
    processReceivedPacket(packet,quality);
    if (consumed)
        restoreIdle();
}

void Ieee802154StandardMac::processReceivedPacket(Packet *packet, const physicallayer::Ieee802154PhyReceiveIndication& quality)
{
    emit(packetReceivedFromLowerSignal, packet);
    if (!started || !client || packet->hasBitError()) {
        delete packet;
        return;
    }
    std::vector<uint8_t> bytes;
    try {
        auto raw = packet->peekDataAsBytes(Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
        bytes = raw->getBytes();
    }
    catch (const cRuntimeError&) {
        delete packet;
        return;
    }
    delete packet;
    auto frame = Ieee802154FrameCodec::decode(bytes);
    if (frame->getDecodeStatus() != Ieee802154FrameCodec::OK || !frame->getFcsValid())
        return;
    if (phyOperation && phyOperation->purpose == PhyPurpose::CCA && frame->getFrameType() != 0)
        return; // 6.3.2.1 permits discarding frames during CCA; beacon observation does not preempt the PHY.
    if (par("enableManagement").boolValue()) {
        expirePending(); // Equality at a persistence deadline precedes every managed receive decision.
        if (!started || resetRequest || !client)
            return;
    }
    std::optional<Ieee802154CommandCodec::Command> parsedCommand;
    if (frame->getFrameType() == 3 && par("enableManagement").boolValue() && !frame->getSecurityEnabled()) {
        std::vector<uint8_t> content(frame->getPayloadArraySize());
        for (size_t i = 0; i < content.size(); i++) content[i] = frame->getPayload(i);
        auto decoded = Ieee802154CommandCodec::decode(content);
        if (auto command = std::get_if<Ieee802154CommandCodec::Command>(&decoded)) parsedCommand = *command;
    }
    auto realignment = parsedCommand ? std::get_if<Ieee802154CommandCodec::CoordinatorRealignment>(&*parsedCommand) : nullptr;
    bool validRealignment = realignment && frame->getSourceAddress().isExtended() && !frame->getSourceAddress().isExtendedGroup() &&
            !frame->getPanCompression() && frame->getDestinationPanId() == 0xffff && realignment->channelNumber >= 11 &&
            realignment->channelNumber <= 26 && realignment->coordinatorShortAddress != 0xffff &&
            (frame->getDestinationAddress().isShortBroadcast() || frame->getDestinationAddress() ==
                    std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value));
    std::optional<uint64_t> orphanScanId;
    if (scanRequest) {
        if (scanRequest->parameters.type != Ieee802154ScanType::ORPHAN) {
            receiveScanBeacon(frame, quality);
            return;
        }
        if (!validRealignment || scanRequest->closing || !scanTimer->isScheduled() ||
                quality.timing.ppduStart < scanRequest->listeningSince || simTime() > scanTimer->getArrivalTime() ||
                frame->getDestinationAddress() != std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value) ||
                realignment->shortAddress == 0xffff || !frame->getAckRequest())
            return;
        orphanScanId = scanRequest->id;
        cancelEvent(scanTimer);
    }
    if (par("enableManagement").boolValue() && panCoordinator && frame->getFrameType() == 3 && !frame->getSecurityEnabled() &&
            frame->getPayloadArraySize() == 1 && frame->getPayload(0) == 7 && frame->getSourceAddress().isNone() &&
            frame->getDestinationAddress().isShortBroadcast() && frame->getDestinationPanId() == 0xffff && !frame->getAckRequest()) {
        if (getBoolean("macBeaconAutoRespond")) {
            if (beaconResponses == UINT64_MAX)
                throw cRuntimeError("IEEE 802.15.4 beacon response counter exhausted");
            ++beaconResponses;
            restoreIdle();
        }
        else if (managementClient)
            managementClient->handleBeaconRequestIndication({frame->getSourceAddress(),frame->getDestinationPanId()});
        return;
    }
    if (panCoordinator && parsedCommand && std::holds_alternative<Ieee802154CommandCodec::OrphanNotification>(*parsedCommand) &&
            frame->getSourceAddress().isExtended() && !frame->getSourceAddress().isExtendedGroup() &&
            frame->getDestinationAddress().isShortBroadcast() && frame->getDestinationPanId() == 0xffff && frame->getPanCompression()) {
        if (managementClient)
            managementClient->handleOrphanIndication({frame->getSourceAddress()});
        return;
    }
    bool promiscuous = getBoolean("macPromiscuousMode");
    if (promiscuous) {
        auto monitor = new Packet("MCPS-DATA.promiscuous");
        monitor->insertAtBack(makeShared<BytesChunk>(std::vector<uint8_t>(bytes.begin(), bytes.end() - 2)));
        monitor->addTag<FcsInd>()->setHasFcs(false);
        monitor->addTag<PacketProtocolTag>()->setProtocol(&Protocol::ieee802154);
        Ieee802154MacPromiscuousIndication indication;
        indication.lqi = quality.lqi.value_or(0);
        indication.rssi = receivedRssi(quality);
        client->handlePromiscuousIndication(monitor, indication);
        if (!started || !client)
            return;
        if (orphanScanId && (!scanRequest || scanRequest->id != *orphanScanId))
            return;
    }
    if (frame->getFrameType() == 2) {
        if (transaction && transaction->phase == Phase::WAIT_ACK && frame->getDsn() == transaction->frame->getDsn() &&
                quality.timing.ppduStart >= transaction->transmittedEnd + SimTime(192, SIMTIME_US) && quality.timing.ppduEnd <= transaction->ackDeadline) {
            cancelEvent(ackTimer);
            transaction->ackFramePending = frame->getFramePending();
            if (quality.meanPower)
                transaction->ackRssi = receivedRssi(quality);
            eligibleAt = std::max(eligibleAt, quality.timing.ppduEnd + interframeSpacing(transaction->frame->getChunkLength()));
            completeData(Ieee802154MacStatus::SUCCESS);
        }
        return;
    }
    if (par("enableManagement").boolValue() && receiveManagedBeacon(frame,quality))
        return;
    bool dataRequest = false;
    bool associationResponse = false;
    std::optional<uint8_t> associationCapability;
    std::optional<Ieee802154CommandCodec::AssociationResponse> responseContent;
    std::optional<uint8_t> disassociationReason;
    bool panConflict = false;
    bool acceptRealignment = validRealignment && (orphanScanId || (getBoolean("macAssociatedPanCoord") &&
            frame->getSourceAddress() == std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value)));
    if (parsedCommand && !frame->getSourceAddress().isNone()) {
        auto command = &*parsedCommand;
            panConflict = panCoordinator && std::holds_alternative<Ieee802154CommandCodec::PanConflictNotification>(*command) &&
                    frame->getSourceAddress().isExtended() && frame->getDestinationAddress().isExtended() && frame->getPanCompression() &&
                    frame->getSourcePanId() == getInteger("macPanId") &&
                    associatedDevices.count({uint16_t(getInteger("macPanId")),frame->getSourceAddress()});
            dataRequest = panCoordinator && std::holds_alternative<Ieee802154CommandCodec::DataRequest>(*command);
            associationResponse = std::holds_alternative<Ieee802154CommandCodec::AssociationResponse>(*command) &&
                    frame->getSourceAddress().isExtended() && frame->getDestinationAddress().isExtended() &&
                    frame->getPanCompression() && frame->getAckRequest();
            if (associationResponse)
                responseContent = std::get<Ieee802154CommandCodec::AssociationResponse>(*command);
            if (auto disassociation = std::get_if<Ieee802154CommandCodec::DisassociationNotification>(command)) {
                bool authority = (getBoolean("macAssociatedPanCoord") && frame->getSourceAddress() ==
                        std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value)) ||
                        (panCoordinator && associatedDevices.count({uint16_t(getInteger("macPanId")),frame->getSourceAddress()}));
                if (authority && frame->getSourceAddress().isExtended() && frame->getPanCompression() &&
                        frame->getDestinationPanId() == getInteger("macPanId") && !frame->getDestinationAddress().isNone() &&
                        !frame->getDestinationAddress().isBroadcast() && !frame->getDestinationAddress().isExtendedGroup() &&
                        (disassociation->reason == 1 || disassociation->reason == 2))
                    disassociationReason = disassociation->reason;
            }
            if (panCoordinator && std::holds_alternative<Ieee802154CommandCodec::AssociationRequest>(*command) &&
                    frame->getSourceAddress().isExtended() && frame->getSourcePanId() == 0xffff && frame->getAckRequest()) {
                if (!getBoolean("macAssociationPermit"))
                    return; // IEEE Std 802.15.4-2024, 10.21.2: ignore when association is not permitted.
                associationCapability = std::get<Ieee802154CommandCodec::AssociationRequest>(*command).capabilityInformation;
                if (*associationCapability & 0x10)
                    return; // Fast association requires a separately implemented direct-response procedure.
            }
    }
    bool securedCommand = frame->getFrameType() == 3 && frame->getSecurityEnabled() && par("enableManagement").boolValue();
    if (frame->getFrameType() != 1 && !dataRequest && !securedCommand && !associationResponse && !associationCapability && !disassociationReason && !acceptRealignment && !panConflict)
        return;
    auto pan = uint16_t(getInteger("macPanId"));
    const auto& destination = frame->getDestinationAddress();
    bool implicitBroadcast = destination.isNone() && getBoolean("macImplicitBroadcast");
    bool accepted;
    if (destination.isNone())
        accepted = implicitBroadcast || (!frame->getSourceAddress().isNone() && panCoordinator && frame->getSourcePanId() == pan);
    else {
        bool addressMatches = destination.isBroadcast() || destination == std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value) ||
                (destination.isShort() && !destination.isShortUnallocated() && destination.getShort() == getInteger("macShortAddress")) ||
                (destination.isExtendedGroup() && getBoolean("macGroupRxMode"));
        accepted = addressMatches && (frame->getDestinationPanId() == pan || frame->getDestinationPanId() == 0xffff);
    }
    if (!accepted)
        return;
    if (frame->getFrameType() == 1 && !frame->getSecurityEnabled() && destination.isBroadcast() &&
            isAssociatedCoordinator(frame->getSourcePanId(),frame->getSourceAddress()))
        broadcastReceivePending = frame->getFramePending();
    AckExchange received;
    received.associationCapability = associationCapability;
    received.disassociationReason = disassociationReason;
    if (acceptRealignment)
        received.realignment = *realignment;
    if (panConflict)
        received.panConflictChannel = quality.channel;
    received.receivedLength = B(bytes.size());
    auto& indication = received.indication;
    indication.sourceAddress = frame->getSourceAddress();
    indication.destinationAddress = destination;
    indication.destinationPanId = destination.isNone() ? pan : frame->getDestinationPanId();
    if (!frame->getSourceAddress().isNone()) {
        indication.effectiveSourcePanId = frame->getSourcePanId();
        if (!frame->getPanCompression())
            indication.sourcePanId = frame->getSourcePanId();
    }
    indication.dsn = frame->getDsn();
    indication.framePending = frame->getFramePending();
    indication.frameVersion = frame->getFrameVersion();
    indication.lqi = quality.lqi.value_or(0);
    indication.rssi = receivedRssi(quality);
    if (dataRequest) {
        auto canonical = canonicalAddress(frame->getSourcePanId(),frame->getSourceAddress());
        if (!canonical.isBroadcast() && !canonical.isExtendedGroup() && pending->count(frame->getSourcePanId(), canonical, simTime()))
            received.polledDevice = std::make_pair(frame->getSourcePanId(), canonical);
    }
    else if (pollRequest && pollRequest->waiting && quality.timing.ppduEnd <= pollRequest->deadline &&
            (frame->getSourceAddress().isNone() ? frame->getDestinationPanId() : frame->getSourcePanId()) == pollRequest->request.coordinatorPanId) {
        auto coordinator = pollRequest->request.coordinatorAddress;
        auto source = frame->getSourceAddress();
        bool matches = source.isNone() || source == coordinator;
        if (coordinator.isShort() && coordinator.getShort() == getInteger("macCoordShortAddress"))
            matches |= source == std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value);
        if (associationResponse && pollRequest->association && associationRequest && coordinator.isShort() && source.isExtended())
            matches = true; // 10.21.2: learn the extended coordinator identity from its response.
        if (matches && !frame->getSecurityEnabled()) {
            received.receivedPollResponse = pollRequest->id;
            received.pollDataPresent = frame->getFrameType() == 1 && frame->getPayloadArraySize() != 0;
            if (associationResponse)
                received.associationResponse = responseContent;
            cancelEvent(pollTimer);
        }
    }
    if (frame->getSecurityEnabled())
        received.securityError = frame->getFrameVersion() == 0 ? Ieee802154MacStatus::UNSUPPORTED_LEGACY : Ieee802154MacStatus::UNSUPPORTED_SECURITY;
    else if (!promiscuous && !dataRequest && !associationResponse && !associationCapability && !disassociationReason && !acceptRealignment && !panConflict && !(received.receivedPollResponse && !frame->getPayloadArraySize())) {
        std::vector<uint8_t> payload;
        for (size_t i = 0; i < frame->getPayloadArraySize(); ++i)
            payload.push_back(frame->getPayload(i));
        received.msdu = new Packet("MCPS-DATA.indication");
        if (!payload.empty())
            received.msdu->insertAtBack(makeShared<BytesChunk>(payload));
    }
    bool acknowledge = frame->getAckRequest() && !destination.isBroadcast() && !implicitBroadcast;
    if (acknowledge && !ackExchange && !phyOperation) {
        auto ack = makeShared<Ieee802154Frame>();
        ack->setFrameType(2);
        ack->setFrameVersion(0);
        ack->setDsn(frame->getDsn());
        ack->setFramePending(received.polledDevice.has_value());
        Ieee802154FrameCodec::finalize(*ack);
        received.frame = ack;
        received.target = quality.timing.ppduEnd + SimTime(16, SIMTIME_US) * getInteger("macSifsPeriod");
        ackExchange = received;
        requestState(PhyState::TRANSMIT, PhyPurpose::ACK_READY);
    }
    else {
        eligibleAt = std::max(eligibleAt, quality.timing.ppduEnd + interframeSpacing(received.receivedLength));
        deliverReceived(received, false);
        continueData();
    }
}

simtime_t Ieee802154StandardMac::interframeSpacing(b length) const
{
    return SimTime(16, SIMTIME_US) * getInteger(length <= B(18) ? "macSifsPeriod" : "macLifsPeriod");
}

uint8_t Ieee802154StandardMac::receivedRssi(const physicallayer::Ieee802154PhyReceiveIndication& indication)
{
    if (!indication.meanPower || *indication.meanPower <= W(0))
        return 0;
    return std::lround(std::clamp((10 * std::log10(indication.meanPower->get<W>() * 1000) + 100) / 80, 0.0, 1.0) * 255);
}

void Ieee802154StandardMac::deliverReceived(AckExchange& exchange, bool ackSent)
{
    if (!started || !client) {
        delete exchange.msdu;
        exchange.msdu = nullptr;
        return;
    }
    if (exchange.polledDevice && ackSent) {
        pendingPolls.push_back({exchange.polledDevice->first, exchange.polledDevice->second, simTime()});
    }
    if (exchange.associationCapability && ackSent && managementClient)
        managementClient->handleAssociateIndication({exchange.indication.sourceAddress,*exchange.associationCapability});
    if (exchange.associationResponse && associationRequest && exchange.receivedPollResponse && pollRequest && pollRequest->association) {
        associationRequest->response = exchange.associationResponse;
        associationRequest->coordinatorExtendedAddress = exchange.indication.sourceAddress;
    }
    if (exchange.disassociationReason) {
        auto source = exchange.indication.sourceAddress;
        auto pan = uint16_t(getInteger("macPanId"));
        bool leaving = getBoolean("macAssociatedPanCoord") &&
                source == std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value);
        cleanupAssociation(pan,leaving ? std::optional<Ieee802154Address>() : source,exchange.receivedPollResponse);
        if (started && managementClient)
            managementClient->handleDisassociateIndication({source,*exchange.disassociationReason});
    }
    if (exchange.realignment) {
        if (scanRequest && scanRequest->parameters.type == Ieee802154ScanType::ORPHAN)
            commitOrphanRealignment(*exchange.realignment,exchange.indication.sourceAddress);
        else if (managementClient) {
            auto& content = *exchange.realignment;
            physicallayer::Ieee802154PhyChannel target{physicallayer::Ieee802154PhyChannel::Phy::O_QPSK,
                    physicallayer::Ieee802154PhyChannel::Band::MHZ_2450,content.channelNumber};
            managementClient->handleSyncLossIndication({Ieee802154SyncLossReason::REALIGNMENT,content.panId,target,
                    exchange.indication.sourceAddress,content.shortAddress,content.coordinatorShortAddress});
        }
    }
    if (exchange.panConflictChannel && managementClient)
        managementClient->handleSyncLossIndication({Ieee802154SyncLossReason::PAN_ID_CONFLICT,exchange.indication.destinationPanId,
                *exchange.panConflictChannel,std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value),{},{}});
    bool pollHasData = exchange.pollDataPresent;
    if (exchange.securityError) {
        Ieee802154MacCommStatusIndication status(*exchange.securityError);
        status.panId = exchange.indication.destinationPanId;
        status.sourceAddress = exchange.indication.sourceAddress;
        status.destinationAddress = exchange.indication.destinationAddress;
        client->handleCommStatusIndication(nullptr, status);
    }
    else if (exchange.msdu) {
        auto packet = exchange.msdu;
        exchange.msdu = nullptr;
        exchange.indication.ackSent = ackSent;
        client->handleDataIndication(packet, exchange.indication);
    }
    if (exchange.receivedPollResponse && pollRequest && pollRequest->id == *exchange.receivedPollResponse)
        completePoll(pollHasData ? Ieee802154MacStatus::SUCCESS : Ieee802154MacStatus::NO_DATA);
}

void Ieee802154StandardMac::startAck()
{
    if (!ackExchange)
        return;
    if (simTime() != ackExchange->target) {
        finishAck(false);
        return;
    }
    auto packet = new Packet("IEEE802154-ACK");
    packet->insertAtBack(ackExchange->frame);
    packet->addTag<PacketProtocolTag>()->setProtocol(&Protocol::ieee802154);
    auto internalId = allocatePhyId();
    phyOperation = PhyOperation{internalId, PhyPurpose::ACK_TRANSMIT};
    emit(packetSentToLowerSignal, packet);
    if (!phyOperation || phyOperation->id != internalId || !ackExchange) {
        delete packet;
        return;
    }
    physicallayer::Ieee802154PhyTransmitRequest request;
    request.startAt = ackExchange->target;
    if (!ackExchange->indication.sourceAddress.isNone())
        request.destination = ackExchange->indication.sourceAddress;
    if (phy->submitTransmit(internalId, packet, request) != PhyAdmission::ACCEPTED) {
        delete packet;
        phyOperation.reset();
        finishAck(false);
    }
}

void Ieee802154StandardMac::finishAck(bool sent)
{
    if (!ackExchange)
        return;
    auto completed = *ackExchange;
    ackExchange.reset();
    eligibleAt = std::max(eligibleAt, simTime() + interframeSpacing(completed.receivedLength));
    deliverReceived(completed, sent);
    continueData();
}

void Ieee802154StandardMac::discardAck()
{
    if (ackExchange) {
        delete ackExchange->msdu;
        ackExchange.reset();
    }
}

void Ieee802154StandardMac::continueData()
{
    if (!started || !phyAvailable || retiringRequests || resetRequest || ackExchange || phyOperation)
        return;
    if (!transaction) {
        startPendingResponse();
        if (!transaction)
            restoreIdle();
    }
    else if (transaction->phase == Phase::ACCESS)
        performCca();
    else if (transaction->phase == Phase::WAIT_ACK) {
        if (phyState != PhyState::RECEIVE)
            requestState(PhyState::RECEIVE, PhyPurpose::ACK_LISTEN);
    }
    else if (transaction->phase == Phase::WAIT_IFS || transaction->phase == Phase::WAIT_RX)
        startData();
}

void Ieee802154StandardMac::ackTimeout()
{
    if (!transaction || transaction->phase != Phase::WAIT_ACK)
        return;
    if (transaction->kind == Transaction::Kind::INDIRECT || transaction->retries >= getInteger("macMaxFrameRetries"))
        completeData(Ieee802154MacStatus::NO_ACK);
    else {
        transaction->retries++;
        transaction->phase = Phase::WAIT_IFS;
        if (!ackExchange)
            startData();
    }
}

void Ieee802154StandardMac::handleEdConfirm(uint64_t id, const physicallayer::Ieee802154PhyEdConfirm& confirm)
{
    Enter_Method("handleEdConfirm");
    if (!phyOperation || phyOperation->id != id || phyOperation->purpose != PhyPurpose::SCAN_ED || !scanRequest)
        return;
    phyOperation.reset();
    if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) || !confirm.level) {
        finishScan(macResult(confirm.result));
        return;
    }
    scanRequest->energyPeak = std::max(scanRequest->energyPeak,*confirm.level);
    if (simTime() == scanRequest->deadline) {
        scanRequest->confirm.energyDetectList.push_back(scanRequest->energyPeak);
        scanRequest->index++;
        beginScanChannel();
    }
    else
        measureScanEnergy();
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitPoll(uint64_t id, const Ieee802154MacPollRequest& request)
{
    Enter_Method("submitPoll");
    auto admission = admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!par("enableManagement").boolValue() || !managementClient || !phy || !access)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    pollRequest = PollRequest{id, request};
    if (request.coordinatorPanId == 0xffff) {
        completePoll(Ieee802154MacStatus::INVALID_PARAMETER); // Table 10-112 permits 0x0000..0xfffe.
        return admission;
    }
    if (request.security.securityLevel != 0) {
        completePoll(request.security.securityLevel > 7 ? Ieee802154MacStatus::INVALID_PARAMETER : Ieee802154MacStatus::UNSUPPORTED_SECURITY);
        return admission;
    }
    if (request.coordinatorAddress.isNone() || request.coordinatorAddress.isBroadcast() ||
            request.coordinatorAddress.isExtendedGroup() || request.coordinatorAddress.isShortUnallocated()) {
        completePoll(Ieee802154MacStatus::INVALID_ADDRESS);
        return admission;
    }
    startPoll();
    return admission;
}

void Ieee802154StandardMac::startPoll()
{
    auto request = pollRequest->request;
    auto id = pollRequest->id;
    auto frame = makeShared<Ieee802154Frame>();
    frame->setFrameType(3);
    frame->setFrameVersion(1);
    frame->setDsn(getInteger("macDsn"));
    frame->setDestinationAddress(request.coordinatorAddress);
    frame->setDestinationPanId(request.coordinatorPanId);
    // IEEE Std 802.15.4-2024, 10.22.4.1: use extended identity without an allocated short address.
    auto shortAddress = getInteger("macShortAddress");
    frame->setSourceAddress(shortAddress < 0xfffe ? Ieee802154Address::fromShort(shortAddress) :
            std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value));
    frame->setSourcePanId(request.coordinatorPanId);
    frame->setPanCompression(true); // 10.22.4.1: source PAN is omitted with explicit destination addressing.
    frame->setAckRequest(true);
    frame->setPayloadArraySize(1);
    frame->setPayload(0, 4);
    Ieee802154FrameCodec::finalize(*frame);
    pib->set("macDsn", uint64_t((frame->getDsn() + 1) & 255));
    transaction = Transaction{id, nullptr, 0, frame};
    transaction->kind = Transaction::Kind::POLL;
    startData();
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitPurge(uint64_t id, uint8_t handle)
{
    Enter_Method("submitPurge");
    auto admission = admit(id, true);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!par("enableManagement").boolValue() || !managementClient)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    auto result = Ieee802154MacStatus::INVALID_HANDLE;
    for (const auto& [token, original] : pendingOriginals) {
        // Tables 8-33/34: an MSDU which has left the queue for transmission cannot be purged.
        if (std::holds_alternative<std::monostate>(original.command) && original.handle == handle && (!transaction || transaction->id != token)) {
            completePending(token, Ieee802154MacServiceAbortReason::PURGED);
            result = Ieee802154MacStatus::SUCCESS;
            break;
        }
    }
    if (managementClient)
        managementClient->handlePurgeConfirm(id, {handle, result});
    return admission;
}

void Ieee802154StandardMac::retireDataRadio()
{
    if (!ackExchange) {
        phyOperation.reset();
        if (phy)
            phy->submitReset(allocatePhyId(), false);
        phyState = PhyState::OFF;
    }
}

void Ieee802154StandardMac::completePending(uint64_t token, const Ieee802154MacServiceResult& result,
        std::optional<uint8_t> backoffs, std::optional<bool> ackFramePending, std::optional<uint8_t> ackRssi)
{
    auto found = pendingOriginals.find(token);
    if (found == pendingOriginals.end())
        return;
    auto original = found->second;
    pendingOriginals.erase(found);
    pending->remove(token);
    schedulePendingExpiry();
    delete original.msdu;
    reportPendingResult(token,original,result,backoffs,ackFramePending,ackRssi);
}

void Ieee802154StandardMac::reportPendingResult(uint64_t token, const PendingOriginal& original,
        const Ieee802154MacServiceResult& result, std::optional<uint8_t> backoffs,
        std::optional<bool> ackFramePending, std::optional<uint8_t> ackRssi)
{
    if (auto device = std::get_if<Ieee802154Address>(&original.command)) {
        if (managementClient)
            managementClient->handleAssociateResponseStatus(token,{*device,result});
    }
    else if (auto disassociation = std::get_if<Disassociation>(&original.command))
        completeDisassociate(token,*disassociation,result);
    else if (client) {
        Ieee802154MacDataConfirm confirm(original.handle, result);
        if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS)) {
            ASSERT(backoffs);
            confirm.numBackoffs = backoffs;
        }
        confirm.ackFramePending = ackFramePending;
        confirm.ackRssi = ackRssi;
        client->handleDataConfirm(token, confirm);
    }
}

void Ieee802154StandardMac::schedulePendingExpiry()
{
    cancelEvent(pendingTimer);
    if (auto next = pending->nextExpiry())
        scheduleAt(std::max(simTime(), *next), pendingTimer);
}

void Ieee802154StandardMac::expirePending()
{
    auto expired = pending->expire(simTime());
    for (const auto& entry : expired) {
        if (transaction && transaction->kind == Transaction::Kind::INDIRECT && transaction->id == entry.token) {
            retireDataRadio();
            completeData(Ieee802154MacStatus::TRANSACTION_EXPIRED);
        }
        else
            completePending(entry.token, Ieee802154MacStatus::TRANSACTION_EXPIRED);
    }
    schedulePendingExpiry();
}

void Ieee802154StandardMac::clearPending(const Ieee802154MacServiceResult& result)
{
    if (pendingCallbacksActive)
        return;
    pendingPolls.clear();
    pending->clear();
    cancelEvent(pendingTimer);
    auto originals = std::move(pendingOriginals);
    pendingOriginals.clear();
    // Detach the entire barrier and destroy accepted packets before any reentrant callback.
    for (const auto& [token, original] : originals)
        delete original.msdu;
    pendingCallbacksActive = true;
    for (const auto& [token, original] : originals) {
        auto terminal = started ? result : Ieee802154MacServiceResult(Ieee802154MacServiceAbortReason::STOPPED);
        reportPendingResult(token,original,terminal);
    }
    pendingCallbacksActive = false;
}

void Ieee802154StandardMac::startPendingResponse()
{
    if (!started || !phyAvailable || resetRequest || retiringRequests || transaction || ackExchange || managementRequest || startRequest || scanRequest || beaconPreparation || associationRequest || orphanResponseRequest || pollRequest || phyOperation || !client)
        return;
    while (!pendingPolls.empty()) {
        auto poll = pendingPolls.front();
        pendingPolls.pop_front();
        auto entry = pending->first(poll.panId, poll.device, simTime());
        if (!entry)
            continue;
        auto frame = makeShared<Ieee802154Frame>(*entry->frame);
        frame->setFramePending(frame->getFrameType() == 1 && pending->count(poll.panId, poll.device, simTime()) > 1);
        Ieee802154FrameCodec::finalize(*frame);
        transaction = Transaction{entry->token, nullptr, 0, frame};
        transaction->kind = Transaction::Kind::INDIRECT;
        transaction->pendingDevice = std::make_pair(entry->panId,entry->destination);
        startData();
        break;
    }
}

void Ieee802154StandardMac::completePoll(const Ieee802154MacServiceResult& result)
{
    if (!pollRequest)
        return;
    auto id = pollRequest->id;
    bool association = pollRequest->association;
    pollRequest.reset();
    cancelEvent(pollTimer);
    if (association && associationRequest) {
        if (associationRequest->response)
            completeAssociate(Ieee802154MacStatus::SUCCESS);
        else
            completeAssociate(result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) ?
                    Ieee802154MacServiceResult(Ieee802154MacStatus::NO_DATA) : result);
    }
    else if (!association && managementClient)
        managementClient->handlePollConfirm(id, {result});
    restoreIdle();
}

uint64_t Ieee802154StandardMac::allocatePhyId()
{
    if (phyId == UINT64_MAX)
        throw cRuntimeError("IEEE 802.15.4 internal PHY request IDs exhausted");
    return ++phyId;
}

void Ieee802154StandardMac::completeManagement(const Ieee802154MacServiceResult& result, const std::optional<Ieee802154MacPibValue>& value)
{
    if (!managementRequest)
        return;
    auto request = *managementRequest;
    managementRequest.reset();
    if (client) {
        if (request.value)
            client->handleSetConfirm(request.id,Ieee802154MacSetConfirm(request.attribute,result));
        else {
            Ieee802154MacGetConfirm confirm(request.attribute,result);
            confirm.value = value;
            client->handleGetConfirm(request.id,confirm);
        }
    }
    restoreIdle();
}

void Ieee802154StandardMac::handleGetConfirm(uint64_t id, const physicallayer::Ieee802154PhyGetConfirm& confirm)
{
    Enter_Method("handleGetConfirm");
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::ORPHAN_CHANNEL_GET && orphanResponseRequest) {
        phyOperation.reset();
        if (confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) && confirm.value &&
                std::holds_alternative<physicallayer::Ieee802154PhyChannel>(*confirm.value))
            transmitOrphanResponse(std::get<physicallayer::Ieee802154PhyChannel>(*confirm.value));
        else
            completeOrphanResponse(macResult(confirm.result));
        return;
    }
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::SCAN_CCA_GET && scanRequest) {
        phyOperation.reset();
        if (confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) && confirm.value &&
                std::holds_alternative<simtime_t>(*confirm.value)) {
            scanRequest->originalCcaDuration = std::get<simtime_t>(*confirm.value);
            beginScanChannel();
        }
        else
            finishScan(macResult(confirm.result));
        return;
    }
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::SCAN_GET && scanRequest) {
        phyOperation.reset();
        if (confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) && confirm.value &&
                std::holds_alternative<physicallayer::Ieee802154PhyChannel>(*confirm.value)) {
            scanRequest->originalChannel = std::get<physicallayer::Ieee802154PhyChannel>(*confirm.value);
            if (scanRequest->parameters.type == Ieee802154ScanType::ED) {
                auto internalId = allocatePhyId();
                phyOperation = PhyOperation{internalId, PhyPurpose::SCAN_CCA_GET};
                if (phy->submitGet(internalId,{"phyCcaDuration"}) != PhyAdmission::ACCEPTED) {
                    phyOperation.reset();
                    finishScan(Ieee802154MacServiceAbortReason::STOPPED);
                }
            }
            else
                beginScanChannel();
        }
        else
            finishScan(macResult(confirm.result));
        return;
    }
    if (!phyOperation || phyOperation->id != id || phyOperation->purpose != PhyPurpose::MANAGEMENT_GET || !managementRequest)
        return;
    phyOperation.reset();
    std::optional<Ieee802154MacPibValue> value;
    if (confirm.value)
        std::visit([&](const auto& item) { value = item; },*confirm.value);
    completeManagement(macResult(confirm.result),value);
}

void Ieee802154StandardMac::handleSetConfirm(uint64_t id, const physicallayer::Ieee802154PhySetConfirm& confirm)
{
    Enter_Method("handleSetConfirm");
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::SCAN_REALIGN_CHANNEL && scanRequest && scanRequest->realignment) {
        phyOperation.reset();
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS))
            finishScan(macResult(confirm.result));
        else {
            auto id = scanRequest->id;
            const auto recovered = *scanRequest->realignment;
            pib->set("macPanId",uint64_t(recovered.first.panId));
            pib->set("macShortAddress",uint64_t(recovered.first.shortAddress));
            pib->set("macCoordShortAddress",uint64_t(recovered.first.coordinatorShortAddress));
            pib->set("macCoordExtendedAddress",recovered.second);
            pib->set("macAssociatedPanCoord",true);
            scanRequest->originalChannel.reset(); // The accepted recovered channel is the new operating configuration.
            publishIdentity();
            if (scanRequest && scanRequest->id == id) {
                scanRequest->index++; // The channel which supplied the realignment was scanned successfully.
                finishScan(Ieee802154MacStatus::SUCCESS);
            }
        }
        return;
    }
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::ASSOCIATE_CHANNEL && associationRequest) {
        phyOperation.reset();
        if (confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
            const auto& request = associationRequest->parameters;
            pib->set("macPanId",uint64_t(request.coordinatorPanId));
            pib->set("macCoordShortAddress",uint64_t(request.coordinatorAddress.isShort() ? request.coordinatorAddress.getShort() : 0xffff));
            pib->set("macCoordExtendedAddress",request.coordinatorAddress.isExtended() ? request.coordinatorAddress : Ieee802154Address());
            publishIdentity();
            if (associationRequest && started && !resetRequest)
                transmitAssociationRequest();
        }
        else
            completeAssociate(macResult(confirm.result));
        return;
    }
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::BEACON_CHANNEL && beaconPreparation) {
        phyOperation.reset();
        completeBeaconPreparation(macResult(confirm.result));
        return;
    }
    if (phyOperation && phyOperation->id == id && scanRequest &&
            (phyOperation->purpose == PhyPurpose::SCAN_ED_DURATION || phyOperation->purpose == PhyPurpose::SCAN_CCA_RESTORE)) {
        auto purpose = phyOperation->purpose;
        phyOperation.reset();
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
            if (purpose == PhyPurpose::SCAN_CCA_RESTORE) {
                scanRequest->confirm.result = macResult(confirm.result);
                completeScan();
            }
            else
                finishScan(macResult(confirm.result));
        }
        else if (purpose == PhyPurpose::SCAN_CCA_RESTORE) {
            scanRequest->originalCcaDuration.reset();
            setScanChannel(true);
        }
        else {
            requestState(PhyState::RECEIVE, PhyPurpose::SCAN_RX);
        }
        return;
    }
    if (phyOperation && phyOperation->id == id && scanRequest &&
            (phyOperation->purpose == PhyPurpose::SCAN_CHANNEL || phyOperation->purpose == PhyPurpose::SCAN_RESTORE)) {
        auto purpose = phyOperation->purpose;
        phyOperation.reset();
        if (purpose == PhyPurpose::SCAN_RESTORE) {
            if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) &&
                    std::holds_alternative<Ieee802154MacStatus>(scanRequest->confirm.result))
                scanRequest->confirm.result = macResult(confirm.result);
            completeScan();
        }
        else if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS))
            finishScan(macResult(confirm.result));
        else if (scanRequest->parameters.type == Ieee802154ScanType::ED) {
            auto internalId = allocatePhyId();
            phyOperation = PhyOperation{internalId, PhyPurpose::SCAN_ED_DURATION};
            if (phy->submitSet(internalId,{"phyCcaDuration",phy->getTimingParameters().symbolPeriod}) != PhyAdmission::ACCEPTED) {
                phyOperation.reset();
                finishScan(Ieee802154MacServiceAbortReason::STOPPED);
            }
        }
        else if (scanRequest->parameters.type == Ieee802154ScanType::ACTIVE || scanRequest->parameters.type == Ieee802154ScanType::ORPHAN)
            transmitScanRequest();
        else
            requestState(PhyState::RECEIVE, PhyPurpose::SCAN_RX);
        return;
    }
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::START_CHANNEL && startRequest) {
        phyOperation.reset();
        if (confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
            if (getInteger("macPanId") != startRequest->parameters.panId)
                associatedDevices.clear();
            pib->set("macPanId", uint64_t(startRequest->parameters.panId));
            panCoordinator = true;
            publishIdentity();
        }
        completeStart(macResult(confirm.result));
        return;
    }
    if (!phyOperation || phyOperation->id != id || phyOperation->purpose != PhyPurpose::MANAGEMENT_SET || !managementRequest)
        return;
    phyOperation.reset();
    if (confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) && managementRequest->attribute == "phyCcaDuration")
        pib->updateCcaDuration(std::get<simtime_t>(*managementRequest->value).inUnit(SIMTIME_US));
    completeManagement(macResult(confirm.result));
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitStart(uint64_t id, const Ieee802154MacStartRequest& request)
{
    Enter_Method("submitStart");
    auto admission = admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!par("enableManagement").boolValue() || !managementClient || !phy)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if (!pendingOriginals.empty() || phyOperation)
        return Ieee802154MacServiceAdmission::BUSY;
    startRequest = StartRequest{id, request};
    // IEEE Std 802.15.4-2024, Table 8-21: START does not allocate a short address.
    if (getInteger("macShortAddress") == 0xffff) {
        completeStart(Ieee802154MacStatus::NO_SHORT_ADDRESS);
        return admission;
    }
    auto internalId = allocatePhyId();
    phyOperation = PhyOperation{internalId, PhyPurpose::START_CHANNEL};
    admission = macAdmission(phy->submitSet(internalId, {"phyCurrentChannelInfo", request.channel}));
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED) {
        phyOperation.reset();
        startRequest.reset();
    }
    return admission;
}

void Ieee802154StandardMac::completeStart(const Ieee802154MacServiceResult& result)
{
    if (!startRequest)
        return;
    auto id = startRequest->id;
    startRequest.reset();
    if (managementClient)
        managementClient->handleStartConfirm(id, {result});
    restoreIdle();
}

void Ieee802154StandardMac::beginResetPhy()
{
    if (!resetRequest)
        return;
    phyOperation.reset();
    if (!phy) {
        finishReset(Ieee802154MacStatus::SUCCESS);
        return;
    }
    auto internalId = allocatePhyId();
    phyOperation = PhyOperation{internalId, PhyPurpose::RESET};
    // Table 8-12: MAC RESET retains PHY configuration in this binding.
    if (phy->submitReset(internalId, false) != PhyAdmission::ACCEPTED) {
        phyOperation.reset();
        finishReset(Ieee802154MacServiceAbortReason::STOPPED);
    }
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitScan(uint64_t id, const Ieee802154MacScanRequest& request)
{
    Enter_Method("submitScan");
    auto admission = admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!par("enableManagement").boolValue() || !managementClient || !phy ||
            (request.type != Ieee802154ScanType::PASSIVE && request.type != Ieee802154ScanType::ED && request.type != Ieee802154ScanType::ACTIVE && request.type != Ieee802154ScanType::ORPHAN) ||
            (request.type == Ieee802154ScanType::ORPHAN && panCoordinator))
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if (!pendingOriginals.empty() || phyOperation)
        return Ieee802154MacServiceAdmission::BUSY;
    scanRequest = ScanRequest{id, request, {request.type}, {}, 0, getBoolean("macAutoRequest")};
    auto& channels = scanRequest->parameters.channels;
    bool invalid = channels.empty() || std::any_of(channels.begin(),channels.end(),[](const auto& channel) {
        return channel.phy != physicallayer::Ieee802154PhyChannel::Phy::O_QPSK ||
                channel.band != physicallayer::Ieee802154PhyChannel::Band::MHZ_2450 || channel.channel < 11 || channel.channel > 26;
    });
    if (invalid) {
        finishScan(Ieee802154MacStatus::BAD_CHANNEL);
        return admission;
    }
    // IEEE Std 802.15.4-2024, 6.4.1.1: ascending channel order.
    std::sort(channels.begin(),channels.end(),[](const auto& a, const auto& b) { return a.channel < b.channel; });
    channels.erase(std::unique(channels.begin(),channels.end()),channels.end());
    if (request.type == Ieee802154ScanType::ED && request.duration == 0) {
        finishScan(Ieee802154MacStatus::SUCCESS);
        return admission;
    }
    auto internalId = allocatePhyId();
    phyOperation = PhyOperation{internalId, PhyPurpose::SCAN_GET};
    admission = macAdmission(phy->submitGet(internalId,{"phyCurrentChannelInfo"}));
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED) {
        phyOperation.reset();
        scanRequest.reset();
    }
    return admission;
}

void Ieee802154StandardMac::beginScanChannel()
{
    if (scanRequest->index == scanRequest->parameters.channels.size()) {
        finishScan(scanRequest->parameters.type == Ieee802154ScanType::ORPHAN ||
                (scanRequest->parameters.type == Ieee802154ScanType::ACTIVE && scanRequest->requestSent && !scanRequest->beaconFound) ?
                Ieee802154MacStatus::NO_BEACON : Ieee802154MacStatus::SUCCESS);
        return;
    }
    requestState(PhyState::OFF, PhyPurpose::SCAN_OFF);
}

void Ieee802154StandardMac::finishScan(const Ieee802154MacServiceResult& result)
{
    if (!scanRequest)
        return;
    scanRequest->confirm.result = result;
    if (scanRequest->closing)
        return;
    scanRequest->closing = true;
    cancelEvent(scanTimer);
    if (ackExchange && ackExchange->realignment)
        discardAck();
    if (transaction && transaction->kind == Transaction::Kind::SCAN_REQUEST) {
        retireDataRadio();
        completeData(result);
    }
    auto& channels = scanRequest->parameters.channels;
    scanRequest->confirm.unscannedChannels.insert(scanRequest->confirm.unscannedChannels.end(),channels.begin() + scanRequest->index, channels.end());
    if (phyOperation) {
        auto id = phyOperation->id;
        phyOperation.reset();
        if (phy->cancel(id) == physicallayer::Ieee802154PhyServiceCancelResult::NOT_CANCELLABLE) {
            phy->submitReset(allocatePhyId(),false);
            phyState = PhyState::OFF;
        }
    }
    if (!scanRequest->originalChannel) {
        completeScan();
        return;
    }
    requestState(PhyState::OFF, PhyPurpose::SCAN_RESTORE_OFF);
}

void Ieee802154StandardMac::setScanChannel(bool restore)
{
    if (restore && scanRequest->originalCcaDuration) {
        auto id = allocatePhyId();
        phyOperation = PhyOperation{id, PhyPurpose::SCAN_CCA_RESTORE};
        if (phy->submitSet(id,{"phyCcaDuration",*scanRequest->originalCcaDuration}) != PhyAdmission::ACCEPTED) {
            phyOperation.reset();
            scanRequest->confirm.result = Ieee802154MacServiceAbortReason::STOPPED;
            completeScan();
        }
        return;
    }
    auto channel = restore ? *scanRequest->originalChannel : scanRequest->parameters.channels[scanRequest->index];
    auto id = allocatePhyId();
    phyOperation = PhyOperation{id, restore ? PhyPurpose::SCAN_RESTORE : PhyPurpose::SCAN_CHANNEL};
    if (phy->submitSet(id,{"phyCurrentChannelInfo",channel}) != PhyAdmission::ACCEPTED) {
        phyOperation.reset();
        if (restore) {
            if (std::holds_alternative<Ieee802154MacStatus>(scanRequest->confirm.result))
                scanRequest->confirm.result = Ieee802154MacStatus::BAD_CHANNEL;
            completeScan();
        }
        else
            finishScan(Ieee802154MacStatus::BAD_CHANNEL);
    }
}

void Ieee802154StandardMac::completeScan()
{
    auto completed = std::move(*scanRequest);
    scanRequest.reset();
    if (managementClient)
        managementClient->handleScanConfirm(completed.id, completed.confirm);
    if (resetRequest)
        beginResetPhy();
    else
        restoreIdle();
}

void Ieee802154StandardMac::receiveScanBeacon(const Ptr<Ieee802154Frame>& frame,
        const physicallayer::Ieee802154PhyReceiveIndication& quality)
{
    if (scanRequest->closing || !scanTimer->isScheduled() || simTime() > scanTimer->getArrivalTime() ||
            quality.timing.ppduStart < scanRequest->listeningSince || frame->getFrameType() != 0 ||
            frame->getSourceAddress().isNone())
        return;
    auto description = describeBeacon(frame,quality);
    if (!description)
        return;
    auto descriptor = *description;
    scanRequest->beaconFound = true;
    bool notify = !scanRequest->autoRequest || getBoolean("macNotifyAllBeacons") ||
            (descriptor.contentAvailable && !descriptor.beacon.payload.empty());
    bool limit = false;
    if (scanRequest->autoRequest) {
        auto& list = scanRequest->confirm.panDescriptors;
        auto match = std::find_if(list.begin(),list.end(),[&](const auto& previous) {
            return previous.channel == descriptor.channel && previous.coordinatorPanId == descriptor.coordinatorPanId &&
                    previous.coordinatorAddress == descriptor.coordinatorAddress;
        });
        if (match == list.end())
            list.push_back(descriptor);
        limit = list.size() >= size_t(par("scanCapacity").intValue());
    }
    auto id = scanRequest->id;
    if (notify && managementClient)
        managementClient->handleBeaconNotifyIndication(descriptor);
    if (scanRequest && scanRequest->id == id && !scanRequest->closing && limit)
        finishScan(Ieee802154MacStatus::LIMIT_REACHED);
}

std::optional<Ieee802154PanDescriptor> Ieee802154StandardMac::describeBeacon(const Ptr<Ieee802154Frame>& frame,
        const physicallayer::Ieee802154PhyReceiveIndication& quality)
{
    const auto& source = frame->getSourceAddress();
    if (frame->getFrameType() != 0 || source.isNone() || source.isBroadcast() || source.isExtendedGroup() || source.isShortUnallocated())
        return {};
    std::vector<uint8_t> payload(frame->getPayloadArraySize());
    for (size_t i = 0; i < payload.size(); i++)
        payload[i] = frame->getPayload(i);
    Ieee802154BeaconCodec::Content content;
    if (!frame->getSecurityEnabled()) {
        auto decoded = Ieee802154BeaconCodec::decode(payload);
        if (!std::holds_alternative<Ieee802154BeaconCodec::Content>(decoded))
            return {};
        content = std::get<Ieee802154BeaconCodec::Content>(decoded);
    }
    auto timestamp = uint32_t((quality.timing.ppduStart + SimTime(160,SIMTIME_US)).inUnit(SIMTIME_US) / 16) & 0xffffff;
    Ieee802154PanDescriptor descriptor{frame->getSourcePanId(),frame->getSourceAddress(),
            quality.channel,content,quality.lqi.value_or(0),timestamp};
    descriptor.sequenceNumber = frame->getDsn();
    if (frame->getSecurityEnabled()) {
        descriptor.contentAvailable = false;
        descriptor.protectedBody = payload;
        descriptor.securityStatus = frame->getFrameVersion() == 0 ? Ieee802154MacStatus::UNSUPPORTED_LEGACY : Ieee802154MacStatus::UNSUPPORTED_SECURITY;
    }
    return descriptor;
}

void Ieee802154StandardMac::measureScanEnergy()
{
    auto id = allocatePhyId();
    phyOperation = PhyOperation{id, PhyPurpose::SCAN_ED};
    if (phy->submitEd(id) != PhyAdmission::ACCEPTED) {
        phyOperation.reset();
        finishScan(Ieee802154MacServiceAbortReason::STOPPED);
    }
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitBeacon(uint64_t id, const Ieee802154MacBeaconRequest& request)
{
    Enter_Method("submitBeacon");
    auto admission = admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!par("enableManagement").boolValue() || !managementClient || !phy || !access)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if (phyOperation)
        return Ieee802154MacServiceAdmission::BUSY;
    beaconPreparation = id;
    if (request.enhanced || request.superframeOrder != 15 || request.security.securityLevel > 7) {
        completeBeaconPreparation(Ieee802154MacStatus::INVALID_PARAMETER);
        return admission;
    }
    if (request.security.securityLevel != 0) {
        completeBeaconPreparation(Ieee802154MacStatus::UNSUPPORTED_SECURITY);
        return admission;
    }
    auto internalId = allocatePhyId();
    phyOperation = PhyOperation{internalId,PhyPurpose::BEACON_CHANNEL};
    admission = macAdmission(phy->submitSet(internalId,{"phyCurrentChannelInfo",request.channel}));
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED) {
        phyOperation.reset();
        beaconPreparation.reset();
    }
    return admission;
}

void Ieee802154StandardMac::completeBeaconPreparation(const Ieee802154MacServiceResult& result)
{
    auto id = *beaconPreparation;
    beaconPreparation.reset();
    if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS))
        transmitBeacon(id,false);
    else if (managementClient)
        managementClient->handleBeaconConfirm(id,{result});
}

void Ieee802154StandardMac::transmitBeacon(uint64_t id, bool automatic)
{
    auto frame = makeShared<Ieee802154Frame>();
    frame->setFrameType(0);
    frame->setFrameVersion(0);
    frame->setDsn(getInteger("macBsn"));
    auto shortAddress = getInteger("macShortAddress");
    frame->setSourceAddress(shortAddress < 0xfffe ? Ieee802154Address::fromShort(shortAddress) :
            std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value));
    frame->setSourcePanId(getInteger("macPanId"));
    frame->setFramePending(pending->count(frame->getSourcePanId(),Ieee802154Address::fromShort(0xffff),simTime()) != 0);
    Ieee802154BeaconCodec::Content content;
    content.superframeSpecification = 0x0fff | (panCoordinator ? 0x4000 : 0) | (getBoolean("macAssociationPermit") ? 0x8000 : 0);
    content.payload = std::get<std::vector<uint8_t>>(*pib->get("macBeaconPayload").value);
    content.pendingAddresses = pending->addresses(frame->getSourcePanId(),simTime());
    auto bytes = Ieee802154BeaconCodec::encode(content);
    if (bytes.size() + 7 + (frame->getSourceAddress().isShort() ? 2 : 8) > 127) {
        if (!automatic && managementClient)
            managementClient->handleBeaconConfirm(id,{Ieee802154MacStatus::FRAME_TOO_LONG});
        return;
    }
    frame->setPayloadArraySize(bytes.size());
    for (size_t i = 0; i < bytes.size(); i++) frame->setPayload(i,bytes[i]);
    Ieee802154FrameCodec::finalize(*frame);
    pib->set("macBsn",uint64_t((frame->getDsn() + 1) & 255));
    transaction = Transaction{id,nullptr,0,frame};
    transaction->kind = automatic ? Transaction::Kind::AUTO_BEACON : Transaction::Kind::BEACON;
    startData();
}

void Ieee802154StandardMac::transmitScanRequest()
{
    auto frame = makeShared<Ieee802154Frame>();
    frame->setFrameType(3);
    frame->setFrameVersion(1);
    frame->setDsn(getInteger("macDsn"));
    frame->setDestinationAddress(Ieee802154Address::SHORT_BROADCAST_ADDRESS);
    frame->setDestinationPanId(0xffff);
    bool orphan = scanRequest->parameters.type == Ieee802154ScanType::ORPHAN;
    if (orphan) {
        frame->setSourceAddress(std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value));
        frame->setSourcePanId(0xffff);
        frame->setPanCompression(true);
    }
    frame->setPayloadArraySize(1);
    frame->setPayload(0,orphan ? 6 : 7);
    Ieee802154FrameCodec::finalize(*frame);
    pib->set("macDsn",uint64_t((frame->getDsn() + 1) & 255));
    transaction = Transaction{scanRequest->id,nullptr,0,frame};
    transaction->kind = Transaction::Kind::SCAN_REQUEST;
    startData();
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitAssociateResponse(uint64_t id,
        const Ieee802154MacAssociateResponse& response)
{
    Enter_Method("submitAssociateResponse");
    auto admission = admit(id,true);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!par("enableManagement").boolValue() || !managementClient || !panCoordinator || !phy || !access)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    std::optional<Ieee802154MacStatus> failure;
    if (!response.deviceAddress.isExtended() || response.deviceAddress.isExtendedGroup())
        failure = Ieee802154MacStatus::INVALID_ADDRESS;
    else if (response.associationStatus > 2 || (response.associationStatus == 0 ? response.shortAddress == 0xffff : response.shortAddress != 0xffff) ||
            response.security.securityLevel > 7)
        failure = Ieee802154MacStatus::INVALID_PARAMETER;
    else if (response.security.securityLevel != 0)
        failure = Ieee802154MacStatus::UNSUPPORTED_SECURITY;
    auto pan = uint16_t(getInteger("macPanId"));
    if (!failure && response.associationStatus == 0 && response.shortAddress < 0xfffe) {
        for (const auto& [peer,shortAddress] : associatedDevices)
            if (peer.first == pan && shortAddress == response.shortAddress && peer.second != response.deviceAddress)
                failure = Ieee802154MacStatus::INVALID_ADDRESS;
    }
    if (failure) {
        managementClient->handleAssociateResponseStatus(id,{response.deviceAddress,*failure});
        return admission;
    }
    auto frame = makeShared<Ieee802154Frame>();
    frame->setFrameType(3);
    frame->setFrameVersion(1);
    frame->setDsn(getInteger("macDsn"));
    frame->setSourceAddress(std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value));
    frame->setDestinationAddress(response.deviceAddress);
    frame->setSourcePanId(getInteger("macPanId"));
    frame->setDestinationPanId(getInteger("macPanId"));
    frame->setPanCompression(true);
    frame->setAckRequest(true);
    auto content = Ieee802154CommandCodec::encode(Ieee802154CommandCodec::AssociationResponse{response.shortAddress,response.associationStatus});
    frame->setPayloadArraySize(content.size());
    for (size_t i = 0; i < content.size(); ++i) frame->setPayload(i,content[i]);
    Ieee802154FrameCodec::finalize(*frame);
    auto expiresAt = simTime() + SimTime(15360,SIMTIME_US) * getInteger("macTransactionPersistenceTime");
    if (!pending->insert({id,uint16_t(getInteger("macPanId")),response.deviceAddress,frame,expiresAt})) {
        managementClient->handleAssociateResponseStatus(id,{response.deviceAddress,Ieee802154MacStatus::TRANSACTION_OVERFLOW});
        return admission;
    }
    pendingOriginals.emplace(id,PendingOriginal{nullptr,0,response.deviceAddress});
    if (response.associationStatus == 0)
        associatedDevices[{pan,response.deviceAddress}] = response.shortAddress;
    else
        associatedDevices.erase({pan,response.deviceAddress});
    pib->set("macDsn",uint64_t((frame->getDsn() + 1) & 255));
    expirePending();
    return admission;
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitAssociate(uint64_t id,
        const Ieee802154MacAssociateRequest& request)
{
    Enter_Method("submitAssociate");
    auto admission = admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!par("enableManagement").boolValue() || !managementClient || !phy || !access || panCoordinator)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if (phyOperation || !pendingOriginals.empty())
        return Ieee802154MacServiceAdmission::BUSY;
    std::optional<Ieee802154MacStatus> failure;
    if (request.coordinatorAddress.isNone() || request.coordinatorAddress.isBroadcast() ||
            request.coordinatorAddress.isExtendedGroup() || request.coordinatorAddress.isShortUnallocated())
        failure = Ieee802154MacStatus::INVALID_ADDRESS;
    else if (getBoolean("macAssociatedPanCoord") || (request.capabilityInformation & ~0x8e) || request.security.securityLevel > 7)
        failure = Ieee802154MacStatus::INVALID_PARAMETER;
    else if (request.security.securityLevel != 0)
        failure = Ieee802154MacStatus::UNSUPPORTED_SECURITY;
    if (failure) {
        managementClient->handleAssociateConfirm(id,{0xffff,{},*failure});
        return admission;
    }
    associationRequest = AssociationRequest{id,request};
    auto internalId = allocatePhyId();
    phyOperation = PhyOperation{internalId,PhyPurpose::ASSOCIATE_CHANNEL};
    admission = macAdmission(phy->submitSet(internalId,{"phyCurrentChannelInfo",request.channel}));
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED) {
        phyOperation.reset();
        associationRequest.reset();
    }
    return admission;
}

void Ieee802154StandardMac::transmitAssociationRequest()
{
    const auto& request = associationRequest->parameters;
    auto frame = makeShared<Ieee802154Frame>();
    frame->setFrameType(3);
    frame->setFrameVersion(1);
    frame->setDsn(getInteger("macDsn"));
    frame->setSourceAddress(std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value));
    frame->setSourcePanId(0xffff);
    frame->setDestinationAddress(request.coordinatorAddress);
    frame->setDestinationPanId(request.coordinatorPanId);
    frame->setAckRequest(true);
    auto content = Ieee802154CommandCodec::encode(Ieee802154CommandCodec::AssociationRequest{request.capabilityInformation});
    frame->setPayloadArraySize(content.size());
    for (size_t i = 0; i < content.size(); ++i) frame->setPayload(i,content[i]);
    Ieee802154FrameCodec::finalize(*frame);
    pib->set("macDsn",uint64_t((frame->getDsn() + 1) & 255));
    transaction = Transaction{associationRequest->id,nullptr,0,frame};
    transaction->kind = Transaction::Kind::ASSOCIATE_REQUEST;
    startData();
}

void Ieee802154StandardMac::completeAssociate(const Ieee802154MacServiceResult& result)
{
    if (!associationRequest)
        return;
    auto completed = *associationRequest;
    associationRequest.reset();
    cancelEvent(associationTimer);
    if (pollRequest && pollRequest->association) {
        pollRequest.reset();
        cancelEvent(pollTimer);
    }
    if (transaction && transaction->id == completed.id) {
        retireDataRadio();
        completeData(result);
    }
    else if (phyOperation && phyOperation->purpose == PhyPurpose::ASSOCIATE_CHANNEL) {
        auto id = phyOperation->id;
        phyOperation.reset();
        phy->cancel(id);
    }
    Ieee802154MacAssociateConfirm confirm;
    confirm.result = result;
    if (completed.response)
        confirm.associationStatus = completed.response->status;
    bool success = result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) && completed.response &&
            completed.response->status == 0 && completed.response->shortAddress != 0xffff;
    if (success) {
        confirm.shortAddress = completed.response->shortAddress;
        pib->set("macShortAddress",uint64_t(confirm.shortAddress));
        pib->set("macCoordExtendedAddress",completed.coordinatorExtendedAddress);
        pib->set("macAssociatedPanCoord",true);
    }
    else {
        // IEEE Std 802.15.4-2024, Table 10-101: failed association restores the default PAN.
        pib->set("macPanId",uint64_t(0xffff));
        pib->set("macAssociatedPanCoord",false);
        if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) && (!completed.response || completed.response->status > 2 ||
                (completed.response->status == 0 && completed.response->shortAddress == 0xffff) ||
                (completed.response->status != 0 && completed.response->shortAddress != 0xffff)))
            confirm.result = Ieee802154MacStatus::NO_DATA;
    }
    publishIdentity();
    if (!started)
        confirm.result = Ieee802154MacServiceAbortReason::STOPPED;
    if (managementClient)
        managementClient->handleAssociateConfirm(completed.id,confirm);
    restoreIdle();
}

Ieee802154Address Ieee802154StandardMac::canonicalAddress(uint16_t panId, const Ieee802154Address& address) const
{
    if (address.isBroadcast())
        return Ieee802154Address::fromShort(0xffff);
    if (address.isShort() && address.getShort() < 0xfffe) {
        for (const auto& [peer,shortAddress] : associatedDevices)
            if (peer.first == panId && shortAddress == address.getShort())
                return peer.second;
    }
    return address;
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitDisassociate(uint64_t id,
        const Ieee802154MacDisassociateRequest& request)
{
    Enter_Method("submitDisassociate");
    auto coordinatorExtended = std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value);
    auto coordinatorShort = uint16_t(getInteger("macCoordShortAddress"));
    bool leaving = getBoolean("macAssociatedPanCoord") &&
            (request.deviceAddress == coordinatorExtended || (request.deviceAddress.isShort() &&
                    coordinatorShort < 0xfffe && request.deviceAddress.getShort() == coordinatorShort));
    bool indirect = request.indirectTx && panCoordinator && !leaving;
    auto admission = indirect ? admit(id,true) : admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!par("enableManagement").boolValue() || !managementClient || !phy || !access)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    std::optional<Ieee802154MacStatus> failure;
    auto peer = canonicalAddress(request.devicePanId,request.deviceAddress);
    if (request.deviceAddress.isNone() || request.deviceAddress.isBroadcast() || request.deviceAddress.isExtendedGroup() ||
            request.deviceAddress.isShortUnallocated())
        failure = Ieee802154MacStatus::INVALID_ADDRESS;
    else if (request.devicePanId != getInteger("macPanId") || request.reason != (leaving ? 2 : 1) ||
            (!leaving && (!panCoordinator || !associatedDevices.count({request.devicePanId,peer}))) || request.security.securityLevel > 7)
        failure = Ieee802154MacStatus::INVALID_PARAMETER;
    else if (request.security.securityLevel != 0)
        failure = Ieee802154MacStatus::UNSUPPORTED_SECURITY;
    if (failure) {
        managementClient->handleDisassociateConfirm(id,{request.devicePanId,request.deviceAddress,*failure});
        return admission;
    }
    auto frame = makeShared<Ieee802154Frame>();
    frame->setFrameType(3);
    frame->setFrameVersion(1);
    frame->setDsn(getInteger("macDsn"));
    frame->setSourceAddress(std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value));
    frame->setDestinationAddress(request.deviceAddress);
    frame->setSourcePanId(request.devicePanId);
    frame->setDestinationPanId(request.devicePanId);
    frame->setPanCompression(true);
    frame->setAckRequest(true);
    auto content = Ieee802154CommandCodec::encode(Ieee802154CommandCodec::DisassociationNotification{request.reason});
    frame->setPayloadArraySize(content.size());
    for (size_t i = 0; i < content.size(); ++i) frame->setPayload(i,content[i]);
    Ieee802154FrameCodec::finalize(*frame);
    Disassociation context{request,leaving ? coordinatorExtended : peer,leaving};
    if (indirect) {
        auto expiresAt = simTime() + SimTime(15360,SIMTIME_US) * getInteger("macTransactionPersistenceTime");
        if (!pending->insert({id,request.devicePanId,peer,frame,expiresAt})) {
            completeDisassociate(id,context,Ieee802154MacStatus::TRANSACTION_OVERFLOW);
            return admission;
        }
        pendingOriginals.emplace(id,PendingOriginal{nullptr,0,context});
        pib->set("macDsn",uint64_t((frame->getDsn() + 1) & 255));
        expirePending();
    }
    else {
        disassociationRequest = std::make_pair(id,context);
        pib->set("macDsn",uint64_t((frame->getDsn() + 1) & 255));
        transaction = Transaction{id,nullptr,0,frame};
        transaction->kind = Transaction::Kind::DISASSOCIATE;
        startData();
    }
    return admission;
}

void Ieee802154StandardMac::completeDisassociate(uint64_t id, const Disassociation& request,
        const Ieee802154MacServiceResult& result)
{
    if (std::holds_alternative<Ieee802154MacStatus>(result))
        cleanupAssociation(request.parameters.devicePanId,request.leavingCoordinator ?
                std::optional<Ieee802154Address>() : request.canonicalPeer);
    auto terminal = started ? result : Ieee802154MacServiceResult(Ieee802154MacServiceAbortReason::STOPPED);
    if (managementClient)
        managementClient->handleDisassociateConfirm(id,{request.parameters.devicePanId,request.parameters.deviceAddress,terminal});
    startPendingResponse();
    restoreIdle();
}

void Ieee802154StandardMac::cleanupAssociation(uint16_t panId, const std::optional<Ieee802154Address>& peer,
        const std::optional<uint64_t>& receivedPoll)
{
    bool outerRetirement = retiringRequests;
    retiringRequests = true;
    bool active = transaction && (!peer || (transaction->pendingDevice ?
            *transaction->pendingDevice == std::make_pair(panId,*peer) :
            transaction->frame->getDestinationPanId() == panId && canonicalAddress(panId,transaction->frame->getDestinationAddress()) == *peer));
    auto entries = peer ? pending->removeFor(panId,*peer) : pending->clear();
    std::map<uint64_t,PendingOriginal> originals;
    for (const auto& entry : entries) {
        auto node = pendingOriginals.extract(entry.token);
        if (!node.empty()) originals.insert(std::move(node));
    }
    pendingPolls.erase(std::remove_if(pendingPolls.begin(),pendingPolls.end(),[&](const auto& poll) {
        return !peer || (poll.panId == panId && poll.device == *peer);
    }),pendingPolls.end());
    schedulePendingExpiry();
    for (const auto& [token,original] : originals)
        delete original.msdu;
    if (peer)
        associatedDevices.erase({panId,*peer});
    else {
        pendingPanConflict.reset();
        broadcastReceivePending = false;
        pib->clearAssociation();
        publishIdentity();
    }
    if (active && transaction) {
        retireDataRadio();
        completeData(Ieee802154MacServiceAbortReason::DISASSOCIATED);
    }
    if (!peer && pollRequest) {
        if (receivedPoll && pollRequest->id == *receivedPoll)
            completePoll(Ieee802154MacStatus::NO_DATA);
        else
            completePoll(Ieee802154MacServiceAbortReason::DISASSOCIATED);
    }
    for (const auto& [token,original] : originals)
        reportPendingResult(token,original,started ? Ieee802154MacServiceResult(Ieee802154MacServiceAbortReason::DISASSOCIATED) :
                Ieee802154MacServiceResult(Ieee802154MacServiceAbortReason::STOPPED));
    retiringRequests = outerRetirement;
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitOrphanResponse(uint64_t id,
        const Ieee802154MacOrphanResponse& response)
{
    Enter_Method("submitOrphanResponse");
    auto admission = response.associatedMember ? admitRadio(id) : admit(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!par("enableManagement").boolValue() || !managementClient || !phy || !access || !panCoordinator)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if (phyOperation)
        return Ieee802154MacServiceAdmission::BUSY;
    std::optional<Ieee802154MacServiceResult> failure;
    if (!response.orphanAddress.isExtended() || response.orphanAddress.isExtendedGroup())
        failure = Ieee802154MacStatus::INVALID_ADDRESS;
    else if (!response.associatedMember)
        failure = Ieee802154MacServiceAbortReason::IGNORED;
    else if (response.shortAddress == 0xffff || getInteger("macShortAddress") == 0xffff || response.security.securityLevel > 7)
        failure = Ieee802154MacStatus::INVALID_PARAMETER;
    else if (response.security.securityLevel != 0)
        failure = Ieee802154MacStatus::UNSUPPORTED_SECURITY;
    if (!failure && response.shortAddress < 0xfffe)
        for (const auto& [peer,shortAddress] : associatedDevices)
            if (peer.first == getInteger("macPanId") && shortAddress == response.shortAddress && peer.second != response.orphanAddress)
                failure = Ieee802154MacStatus::INVALID_ADDRESS;
    if (failure) {
        managementClient->handleOrphanResponseStatus(id,{response.orphanAddress,*failure});
        return admission;
    }
    orphanResponseRequest = OrphanResponseRequest{id,response};
    auto internalId = allocatePhyId();
    phyOperation = PhyOperation{internalId,PhyPurpose::ORPHAN_CHANNEL_GET};
    admission = macAdmission(phy->submitGet(internalId,{"phyCurrentChannelInfo"}));
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED) {
        phyOperation.reset(); orphanResponseRequest.reset();
    }
    return admission;
}

void Ieee802154StandardMac::transmitOrphanResponse(const physicallayer::Ieee802154PhyChannel& channel)
{
    const auto& request = orphanResponseRequest->parameters;
    auto pan = uint16_t(getInteger("macPanId"));
    auto frame = makeShared<Ieee802154Frame>();
    frame->setFrameType(3);
    frame->setFrameVersion(1);
    frame->setDsn(getInteger("macDsn"));
    frame->setSourceAddress(std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value));
    frame->setDestinationAddress(request.orphanAddress);
    frame->setSourcePanId(pan);
    frame->setDestinationPanId(0xffff);
    frame->setAckRequest(true);
    auto content = Ieee802154CommandCodec::encode(Ieee802154CommandCodec::CoordinatorRealignment{
            pan,uint16_t(getInteger("macShortAddress")),uint8_t(channel.channel),request.shortAddress});
    frame->setPayloadArraySize(content.size());
    for (size_t i = 0; i < content.size(); ++i) frame->setPayload(i,content[i]);
    Ieee802154FrameCodec::finalize(*frame);
    associatedDevices[{pan,request.orphanAddress}] = request.shortAddress;
    pib->set("macDsn",uint64_t((frame->getDsn() + 1) & 255));
    transaction = Transaction{orphanResponseRequest->id,nullptr,0,frame};
    transaction->kind = Transaction::Kind::ORPHAN_RESPONSE;
    startData();
}

void Ieee802154StandardMac::completeOrphanResponse(const Ieee802154MacServiceResult& result)
{
    if (!orphanResponseRequest)
        return;
    auto completed = *orphanResponseRequest;
    orphanResponseRequest.reset();
    if (transaction && transaction->id == completed.id) {
        retireDataRadio();
        completeData(result);
    }
    else if (phyOperation && phyOperation->purpose == PhyPurpose::ORPHAN_CHANNEL_GET) {
        auto id = phyOperation->id; phyOperation.reset(); phy->cancel(id);
    }
    if (managementClient)
        managementClient->handleOrphanResponseStatus(completed.id,{completed.parameters.orphanAddress,
                started ? result : Ieee802154MacServiceResult(Ieee802154MacServiceAbortReason::STOPPED)});
    restoreIdle();
}

void Ieee802154StandardMac::commitOrphanRealignment(const Ieee802154CommandCodec::CoordinatorRealignment& realignment,
        const Ieee802154Address& coordinator)
{
    scanRequest->realignment = std::make_pair(realignment,coordinator);
    requestState(PhyState::OFF,PhyPurpose::SCAN_REALIGN_OFF);
}

bool Ieee802154StandardMac::receiveManagedBeacon(const Ptr<Ieee802154Frame>& frame,
        const physicallayer::Ieee802154PhyReceiveIndication& quality)
{
    if (frame->getFrameType() != 0)
        return false;
    auto descriptor = describeBeacon(frame,quality);
    if (!descriptor)
        return true;
    if (descriptor->contentAvailable && isAssociatedCoordinator(descriptor->coordinatorPanId,descriptor->coordinatorAddress)) {
        broadcastReceivePending = frame->getFramePending();
        restoreIdle();
    }
    bool conflict = descriptor->contentAvailable && (descriptor->beacon.superframeSpecification & 0x4000) &&
            descriptor->coordinatorPanId == getInteger("macPanId");
    auto coordinator = std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value);
    if (conflict && panCoordinator && managementClient)
        managementClient->handleSyncLossIndication({Ieee802154SyncLossReason::PAN_ID_CONFLICT,descriptor->coordinatorPanId,
                descriptor->channel,std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value),{},{}});
    else if (conflict && !panCoordinator && getBoolean("macAssociatedPanCoord") && coordinator.isExtended() &&
            descriptor->coordinatorAddress != coordinator && descriptor->coordinatorAddress !=
            Ieee802154Address::fromShort(getInteger("macCoordShortAddress")) && !pendingPanConflict) {
        pendingPanConflict = PanConflict{descriptor->coordinatorPanId,coordinator,descriptor->channel};
        restoreIdle();
    }
    if (started && managementClient && (!getBoolean("macAutoRequest") || getBoolean("macNotifyAllBeacons") ||
            (descriptor->contentAvailable && !descriptor->beacon.payload.empty())))
        managementClient->handleBeaconNotifyIndication(*descriptor);
    return true;
}

void Ieee802154StandardMac::transmitPanConflict()
{
    const auto& conflict = *pendingPanConflict;
    auto frame = makeShared<Ieee802154Frame>();
    frame->setFrameType(3);
    frame->setFrameVersion(1);
    frame->setDsn(getInteger("macDsn"));
    frame->setSourceAddress(std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value));
    frame->setDestinationAddress(conflict.coordinator);
    frame->setSourcePanId(conflict.panId);
    frame->setDestinationPanId(conflict.panId);
    frame->setPanCompression(true);
    frame->setAckRequest(true);
    frame->setPayloadArraySize(1);
    frame->setPayload(0,5);
    Ieee802154FrameCodec::finalize(*frame);
    pib->set("macDsn",uint64_t((frame->getDsn() + 1) & 255));
    transaction = Transaction{0,nullptr,0,frame};
    transaction->kind = Transaction::Kind::PAN_CONFLICT;
    startData();
}

bool Ieee802154StandardMac::isAssociatedCoordinator(uint16_t panId, const Ieee802154Address& address) const
{
    return getBoolean("macAssociatedPanCoord") && panId == getInteger("macPanId") &&
            ((address.isExtended() && !address.isExtendedGroup() && address == std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value)) ||
            (address.isShort() && address.getShort() < 0xfffe && address.getShort() == getInteger("macCoordShortAddress")));
}

bool Ieee802154StandardMac::receiverWantsRx() const
{
    return receiverWindow && receiverWindow->duration && simTime() >= receiverWindow->prepareAt &&
            (!receiverWindow->startsAt || receiverWindow->duration == 0xffffff ||
            simTime() < *receiverWindow->startsAt + SimTime(int64_t(receiverWindow->duration) * 16,SIMTIME_US));
}

void Ieee802154StandardMac::clearReceiverWindow()
{
    cancelEvent(receiverPrepareTimer);
    cancelEvent(receiverEndTimer);
    receiverWindow.reset();
}

void Ieee802154StandardMac::finishReceiverWindow()
{
    if (!receiverWindow || !receiverWindow->startsAt)
        return;
    auto id = receiverWindow->id;
    auto deadline = *receiverWindow->startsAt + SimTime(int64_t(receiverWindow->duration) * 16,SIMTIME_US);
    restoreIdle();
    if (!receiverWindow || receiverWindow->id != id)
        return;
    clearReceiverWindow();
    if (managementClient)
        managementClient->handleRxEnableIndication({uint32_t(deadline.inUnit(SIMTIME_US) / 16) & 0xffffff});
}

bool Ieee802154StandardMac::consumeReceiverWindow()
{
    if (!receiverWindow || !receiverWindow->autoOff || !receiverWindow->startsAt || simTime() < *receiverWindow->startsAt)
        return false;
    if (receiverWindow->duration != 0xffffff &&
            simTime() > *receiverWindow->startsAt + SimTime(int64_t(receiverWindow->duration) * 16,SIMTIME_US))
        return false;
    clearReceiverWindow();
    return true;
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitRxEnable(uint64_t id, const Ieee802154MacRxEnableRequest& request)
{
    Enter_Method("submitRxEnable");
    auto admission = admitRadio(id,true,true);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!par("enableManagement").boolValue() || !managementClient)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if (rxEnableRequest)
        return Ieee802154MacServiceAdmission::BUSY;
    Ieee802154MacStatus status = Ieee802154MacStatus::SUCCESS;
    std::optional<simtime_t> start;
    if (request.instances.size() > 1)
        status = Ieee802154MacStatus::INVALID_PARAMETER;
    else if (request.ranging)
        status = Ieee802154MacStatus::RANGING_NOT_SUPPORTED;
    else if (!request.instances.empty()) {
        const auto& instance = request.instances.front();
        if (instance.onTime > 0xffffff || instance.duration > 0xffffff)
            status = Ieee802154MacStatus::INVALID_PARAMETER;
        else if (instance.onTime) {
            int64_t current = simTime().inUnit(SIMTIME_US) / 16;
            uint32_t distance = (instance.onTime - uint32_t(current)) & 0xffffff;
            if (distance > 0x800000 && !request.deferPermit)
                status = Ieee802154MacStatus::PAST_TIME;
            else {
                start = SimTime((current + distance) * 16,SIMTIME_US);
                if (instance.duration && *start - simTime() < SimTime(192,SIMTIME_US) &&
                        !transaction && !ackExchange && !scanRequest && !phyOperation && phyState != PhyState::RECEIVE)
                    return Ieee802154MacServiceAdmission::BUSY;
            }
        }
        else if (!instance.duration)
            start = simTime();
    }
    if (status != Ieee802154MacStatus::SUCCESS) {
        managementClient->handleRxEnableConfirm(id,{status,std::vector<Ieee802154MacStatus>(request.instances.size(),status)});
        return admission;
    }
    rxEnableRequest = id;
    clearReceiverWindow();
    if (!request.instances.empty()) {
        const auto& instance = request.instances.front();
        auto prepareAt = start && instance.duration ? std::max(simTime(),*start - SimTime(192,SIMTIME_US)) : start.value_or(simTime());
        receiverWindow = ReceiverWindow{id,start,prepareAt,instance.duration,instance.autoOff};
        if (prepareAt > simTime())
            scheduleAt(prepareAt,receiverPrepareTimer);
        if (start && instance.duration && instance.duration != 0xffffff)
            scheduleAt(*start + SimTime(int64_t(instance.duration) * 16,SIMTIME_US),receiverEndTimer);
    }
    restoreIdle();
    if (rxEnableRequest != id)
        return admission;
    rxEnableRequest.reset();
    managementClient->handleRxEnableConfirm(id,{Ieee802154MacStatus::SUCCESS,
            std::vector<Ieee802154MacStatus>(request.instances.size(),Ieee802154MacStatus::SUCCESS)});
    if (receiverWindow && receiverWindow->id == id && !receiverWindow->duration && receiverWindow->startsAt <= simTime())
        finishReceiverWindow();
    return admission;
}

} // namespace inet
