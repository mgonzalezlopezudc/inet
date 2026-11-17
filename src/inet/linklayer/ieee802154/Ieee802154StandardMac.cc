// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154StandardMac.h"
#include "inet/common/InitStages.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/linklayer/ieee802154/Ieee802154InterfaceData.h"
#include "inet/linklayer/ieee802154/Ieee802154FrameCodec.h"
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
        networkInterface.reference(this, "interfaceModule", true);
        access.reference(this, "accessModule", false);
        phy.reference(this, "providerModule", false);
        dataTimer = new cMessage("dataTimer");
        ackTimer = new cMessage("ackTimer");
        ackTimer->setSchedulingPriority(1);
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
        pib = std::make_unique<Ieee802154MacPib>(Ieee802154Address(par("extendedAddress")),
                initialDsn, ccaUs,
                std::vector<Ieee802154MacSetRequest>{{"macShortAddress", uint64_t(shortAddress)}, {"macPanId", uint64_t(panId)}, {"macRxOnWhenIdle", bool(par("rxOnWhenIdle"))}}, 0);
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
    if (resetRequest || ((transaction || ackExchange || managementRequest) && !concurrent))
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
    auto admission = admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
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
    else if (request.indirectTx && panCoordinator && !request.destinationAddress.isNone())
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
        discardAck();
        if (managementRequest)
            completeManagement(Ieee802154MacServiceAbortReason::RESET);
        if (transaction)
            completeData(Ieee802154MacServiceAbortReason::RESET);
        resetRequest->retiring = false;
        auto deferredResult = resetRequest->deferredResult;
        if (deferredResult)
            finishReset(*deferredResult);
        else if (!started)
            finishReset(Ieee802154MacServiceAbortReason::STOPPED);
        if (resetRequest)
            beginResetPhy();
    }
    return admission;
}

Ieee802154MacServiceCancelResult Ieee802154StandardMac::cancel(uint64_t id)
{
    Enter_Method("cancel");
    if (id == 0)
        return Ieee802154MacServiceCancelResult::NOT_PENDING;
    if (resetRequest && resetRequest->id == id)
        return Ieee802154MacServiceCancelResult::NOT_CANCELLABLE;
    if ((resetRequest || !started) && ((transaction && transaction->id == id) || (managementRequest && managementRequest->id == id)))
        return Ieee802154MacServiceCancelResult::NOT_CANCELLABLE;
    if (managementRequest && managementRequest->id == id) {
        phyOperation.reset();
        if (phy) phy->submitReset(allocatePhyId(),false);
        phyState = PhyState::OFF;
        completeManagement(Ieee802154MacServiceAbortReason::CANCELLED);
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
        if (transaction)
            completeData(admission == PhyAdmission::UNAVAILABLE ? Ieee802154MacServiceResult(Ieee802154MacServiceAbortReason::STOPPED) :
                    Ieee802154MacServiceResult(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE));
    }
}

void Ieee802154StandardMac::restoreIdle()
{
    if (!started || !phyAvailable || resetRequest || retiringRequests || transaction || ackExchange || managementRequest || phyOperation || !phy)
        return;
    auto target = getBoolean("macRxOnWhenIdle") || getBoolean("macPromiscuousMode") ? PhyState::RECEIVE : PhyState::OFF;
    if (target != phyState)
        requestState(target, PhyPurpose::IDLE);
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
    if (client) {
        Ieee802154MacDataConfirm confirm(completed.handle, result);
        if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS))
            confirm.numBackoffs = completed.backoffs;
        confirm.ackFramePending = completed.ackFramePending;
        confirm.ackRssi = completed.ackRssi;
        client->handleDataConfirm(completed.id, confirm);
    }
    restoreIdle();
}

void Ieee802154StandardMac::finishReset(const Ieee802154MacServiceResult& result)
{
    if (!resetRequest)
        return;
    if (resetRequest->retiring) {
        resetRequest->deferredResult = result;
        return;
    }
    auto request = *resetRequest;
    if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS)) {
        auto defaultDsn = request.defaults ? intuniform(0, 255, par("dsnRng")) : 0;
        pib->reset(request.defaults, defaultDsn, 0);
        if (request.defaults)
            panCoordinator = initialPanCoordinator;
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
    if (managementRequest)
        completeManagement(Ieee802154MacServiceAbortReason::STOPPED);
    discardAck();
    if (transaction)
        completeData(Ieee802154MacServiceAbortReason::STOPPED);
    phyOperation.reset();
    if (phy)
        phy->submitReset(allocatePhyId(), false);
    phyState = PhyState::OFF;
    finishReset(Ieee802154MacServiceAbortReason::STOPPED);
}

void Ieee802154StandardMac::handleMessage(cMessage *message)
{
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
    if (ackExchange)
        finishAck(false);
    if (managementRequest)
        completeManagement(Ieee802154MacServiceAbortReason::STOPPED);
    if (transaction)
        completeData(Ieee802154MacServiceAbortReason::STOPPED);
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
    processReceivedPacket(packet, quality);
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
    if (frame->getFrameType() != 1)
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
    AckExchange received;
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
    if (frame->getSecurityEnabled())
        received.securityError = frame->getFrameVersion() == 0 ? Ieee802154MacStatus::UNSUPPORTED_LEGACY : Ieee802154MacStatus::UNSUPPORTED_SECURITY;
    else if (!promiscuous) {
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
        ack->setFramePending(false);
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
    if (transaction->retries >= getInteger("macMaxFrameRetries"))
        completeData(Ieee802154MacStatus::NO_ACK);
    else {
        transaction->retries++;
        transaction->phase = Phase::WAIT_IFS;
        if (!ackExchange)
            startData();
    }
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
    if (!phyOperation || phyOperation->id != id || phyOperation->purpose != PhyPurpose::MANAGEMENT_SET || !managementRequest)
        return;
    phyOperation.reset();
    if (confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) && managementRequest->attribute == "phyCcaDuration")
        pib->updateCcaDuration(std::get<simtime_t>(*managementRequest->value).inUnit(SIMTIME_US));
    completeManagement(macResult(confirm.result));
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

uint64_t Ieee802154StandardMac::allocatePhyId()
{
    if (phyId == UINT64_MAX)
        throw cRuntimeError("IEEE 802.15.4 internal PHY request IDs exhausted");
    return ++phyId;
}

void Ieee802154StandardMac::handleEdConfirm(uint64_t, const physicallayer::Ieee802154PhyEdConfirm&) { Enter_Method("handleEdConfirm"); }

} // namespace inet
