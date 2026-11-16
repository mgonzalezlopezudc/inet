// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154StandardMac.h"
#include "inet/common/InitStages.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/linklayer/ieee802154/Ieee802154InterfaceData.h"
#include "inet/linklayer/ieee802154/Ieee802154FrameCodec.h"
#include "inet/linklayer/ieee802154/Ieee802154CommandCodec.h"
#include "inet/linklayer/ieee802154/Ieee802154SuperframeTiming.h"
#include "inet/linklayer/ieee802154/Ieee802154ExchangeTiming.h"
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
    cancelAndDelete(gtsTimer);
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
        periodicBeaconStartSignal = registerSignal("periodicBeaconStart");
        capWindowInstalledSignal = registerSignal("capWindowInstalled");
        cfpWindowInstalledSignal = registerSignal("cfpWindowInstalled");
        gtsAllocatedSignal = registerSignal("gtsAllocated");
        gtsDeallocatedSignal = registerSignal("gtsDeallocated");
        gtsActivitySignal = registerSignal("gtsActivity");
        outgoingCapActiveSignal = registerSignal("outgoingCapActive");
        syncMissedSignal = registerSignal("syncMissed");
        syncLostSignal = registerSignal("syncLost");
        initialPanCoordinator = panCoordinator = coordinator = par("isPanCoordinator").boolValue();
        client.reference(this, "clientModule", true);
        managementClient.reference(this, "managementClientModule", false);
        networkInterface.reference(this, "interfaceModule", true);
        access.reference(this, "accessModule", false);
        slottedAccess.reference(this, "slottedAccessModule", false);
        phy.reference(this, "providerModule", false);
        superframe.reference(this, "superframeModule", false);
        if (superframe && (!phy || !par("enableManagement").boolValue()))
            throw cRuntimeError("Superframe preparation requires the PHY and management services");
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
        gtsTimer = new cMessage("gtsTimer");
        gtsTimer->setSchedulingPriority(1);
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
                std::vector<Ieee802154MacSetRequest>{{"macShortAddress", uint64_t(shortAddress)}, {"macPanId", uint64_t(panId)}, {"macRxOnWhenIdle", bool(par("rxOnWhenIdle"))}}, initialBsn, bool(superframe));
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
    if (resetRequest || ((startRequest || superframeStartRequest || (synchronization && !synchronization->installed) || scanRequest || beaconPreparation || associationRequest || orphanResponseRequest) && !duringManagement) || ((transaction || ackExchange || managementRequest || pollRequest) && !concurrent))
        return Ieee802154MacServiceAdmission::BUSY;
    return Ieee802154MacServiceAdmission::ACCEPTED;
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::admitRadio(uint64_t id, bool concurrent, bool duringManagement)
{
    auto admission = admit(id,concurrent,duringManagement);
    if (admission == Ieee802154MacServiceAdmission::ACCEPTED && phyAvailable && (interruptedSuperframeChannel || interruptedScanCcaDuration))
        return Ieee802154MacServiceAdmission::BUSY;
    return admission == Ieee802154MacServiceAdmission::ACCEPTED && !phyAvailable ?
            Ieee802154MacServiceAdmission::UNAVAILABLE : admission;
}

simtime_t Ieee802154StandardMac::transactionPersistence() const
{
    auto order = getInteger("macBeaconOrder");
    auto multiplier = order < 15 ? int64_t(1) << order : 1;
    auto duration = SimTime(15360,SIMTIME_US) * multiplier * getInteger("macTransactionPersistenceTime");
    if (order < 15 && superframe && duration > SIMTIME_ZERO)
        return superframe->getFutureSimulationTime(superframe->getCurrentClockTime() + SIMTIME_AS_CLOCKTIME(duration)) - simTime();
    return duration;
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitData(uint64_t id, Packet *packet, const Ieee802154MacDataRequest& request)
{
    Enter_Method("submitData");
    // IEEE Std 802.15.4-2024, Table 8-30: ignore IndirectTx outside the coordinator/destination predicates.
    bool indirect = request.indirectTx && coordinator && !request.destinationAddress.isNone();
    auto admission = indirect ? admit(id,true) : admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (beaconScheduleActive() && !indirect && !slottedAccess)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
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
    std::optional<Ieee802154GtsManager::Key> gtsKey;
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
    if (!failure && request.gtsTx) {
        if (request.indirectTx)
            failure = Ieee802154MacStatus::INVALID_PARAMETER;
        else if (!superframe || !request.destinationAddress.isShort() || request.destinationAddress.isShortBroadcast() ||
                request.sourceAddressMode != Ieee802154Address::AddressType::SHORT || request.destinationPanId != getInteger("macPanId"))
            failure = Ieee802154MacStatus::INVALID_GTS;
        else {
            auto direction = panCoordinator ? Ieee802154GtsDirection::DEVICE_RECEIVE : Ieee802154GtsDirection::DEVICE_TRANSMIT;
            auto shortAddress = panCoordinator ? request.destinationAddress.getShort() : uint16_t(getInteger("macShortAddress"));
            bool context = panCoordinator ? periodicBeaconing && outgoingEpoch.has_value() :
                    parentAssociated && synchronization && synchronization->installed && synchronization->request.trackBeacon &&
                    request.destinationAddress.getShort() == getInteger("macCoordShortAddress");
            if (context)
                for (const auto& grant : gtsManager.getGrants())
                    if (grant.key.panId == request.destinationPanId && grant.key.shortAddress == shortAddress && grant.key.direction == direction) {
                        gtsKey = grant.key;
                        break;
                    }
            if (!gtsKey)
                failure = Ieee802154MacStatus::INVALID_GTS;
        }
    }
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
        auto expiresAt = simTime() + transactionPersistence();
        auto queuePan = uint16_t(request.destinationAddress.isBroadcast() ? getInteger("macPanId") : request.destinationPanId);
        if (!pending->insert({id, queuePan, canonicalAddress(queuePan,request.destinationAddress), frame, expiresAt})) {
            delete packet;
            client->handleDataConfirm(id, Ieee802154MacDataConfirm(request.msduHandle, Ieee802154MacStatus::TRANSACTION_OVERFLOW));
        }
        else {
            pendingOriginals.emplace(id, PendingOriginal{packet, request.msduHandle});
            pib->set("macDsn", uint64_t((frame->getDsn() + 1) & 255));
            expirePending();
            if (panCoordinator && periodicBeaconing && outgoingEpoch && !constructedGtsCandidate && started && !resetRequest)
                maintainGtsCapacity(outgoingEpoch->superframeOrder);
        }
    }
    else {
        pib->set("macDsn", uint64_t((frame->getDsn() + 1) & 255));
        transaction = Transaction{id, packet, request.msduHandle, frame};
        transaction->gtsKey = gtsKey;
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
        if (periodicBeaconing && request.attribute == "phyCurrentChannelInfo") {
            client->handleSetConfirm(id,{request.attribute,Ieee802154MacStatus::INVALID_PARAMETER});
        }
        else if (phy && request.attribute.rfind("phy",0) == 0) {
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
            bool parentReference = request.attribute == "macPanId" || request.attribute == "macCoordShortAddress" ||
                    request.attribute == "macCoordExtendedAddress";
            auto previousReference = parentReference && parentAssociated ? pib->get(request.attribute).value : std::nullopt;
            auto previousLocalIdentity = request.attribute == "macPanId" || request.attribute == "macShortAddress" ?
                    pib->get(request.attribute).value : std::nullopt;
            auto payload = std::get_if<std::vector<uint8_t>>(&request.value);
            bool oversizedBeacon = request.attribute == "macBeaconPayload" && payload && !fitsBeaconPayload(*payload);
            auto status = Ieee802154MacStatus::SUCCESS;
            if (request.attribute == "macGtsPermit" && superframe && std::get_if<bool>(&request.value) &&
                    std::get<bool>(request.value) && gtsManager.reservedCount() >= 7)
                status = Ieee802154MacStatus::INVALID_PARAMETER;
            else if (request.attribute == "macBeaconOrder" && superframe) {
                auto unsignedValue = std::get_if<uint64_t>(&request.value);
                auto signedValue = std::get_if<int64_t>(&request.value);
                std::optional<uint8_t> value;
                if (unsignedValue && *unsignedValue <= 15)
                    value = *unsignedValue;
                else if (signedValue && *signedValue >= 0 && *signedValue <= 15)
                    value = *signedValue;
                if (!value || (*value < 15 && periodicBeaconing && incomingSuperframe && parentAssociated &&
                        (*value != incomingSuperframe->beaconOrder || getInteger("macSuperframeOrder") != incomingSuperframe->superframeOrder)))
                    status = Ieee802154MacStatus::INVALID_PARAMETER;
                else {
                    status = pib->setBeaconOrder(*value);
                    if (status == Ieee802154MacStatus::SUCCESS && *value == 15 && periodicBeaconing)
                        retirePeriodicBeaconing(Ieee802154MacServiceAbortReason::CANCELLED);
                }
            }
            else
                status = oversizedBeacon || (periodicBeaconing && (request.attribute == "macBattLifeExtPeriods" || request.attribute == "macPanId" || request.attribute == "macShortAddress")) ?
                        Ieee802154MacStatus::INVALID_PARAMETER : pib->set(request.attribute, request.value);
            if (status == Ieee802154MacStatus::SUCCESS && previousLocalIdentity && !(pib->get(request.attribute).value == previousLocalIdentity)) {
                auto outerRetirement = retiringRequests;
                retiringRequests = true;
                clearCfp();
                gtsManager.clear();
                if (gtsRequest)
                    completeGts(Ieee802154MacServiceAbortReason::DISASSOCIATED);
                retiringRequests = outerRetirement;
            }
            if (status == Ieee802154MacStatus::SUCCESS && previousReference && !(pib->get(request.attribute).value == previousReference)) {
                parentAssociated = false;
                pib->set("macAssociatedPanCoord",false);
                autoPollPending.reset();
                auto outerRetirement = retiringRequests;
                retiringRequests = true;
                if (childPhase)
                    retirePeriodicBeaconing(Ieee802154MacServiceAbortReason::DISASSOCIATED);
                retireParentContext(false);
                retiringRequests = outerRetirement;
            }
            Ieee802154MacSetConfirm confirm(request.attribute, status);
            publishIdentity();
            client->handleSetConfirm(id, confirm);
            if (status == Ieee802154MacStatus::SUCCESS && request.attribute == "macBeaconPayload" &&
                    panCoordinator && periodicBeaconing && outgoingEpoch && !constructedGtsCandidate && started && !resetRequest)
                maintainGtsCapacity(outgoingEpoch->superframeOrder);
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
        clearCfp();
        constructedGtsCandidate.reset();
        retireReceives();
        incomingSuperframe.reset();
        incomingCapEnd.reset();
        if (superframe) {
            superframe->clearAccessWindow();
            superframe->cancelAccessSearchDeadline();
        }
        retirePeriodicBeaconing(Ieee802154MacServiceAbortReason::RESET);
        retireSynchronization(Ieee802154MacServiceAbortReason::RESET);
        autoPollPending.reset();
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
        if (gtsRequest)
            completeGts(Ieee802154MacServiceAbortReason::RESET);
        gtsManager.clear();
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
    if (synchronization && !synchronization->installed && synchronization->id == id) {
        if (retiringRequests || resetRequest || !started)
            return Ieee802154MacServiceCancelResult::NOT_CANCELLABLE;
        auto operationId = phyOperation && (phyOperation->purpose == PhyPurpose::SYNC_CHANNEL || phyOperation->purpose == PhyPurpose::SYNC_READY) ? std::optional<uint64_t>(phyOperation->id) : std::nullopt;
        if (operationId) phyOperation.reset();
        auto cancelledId = synchronization->id;
        synchronization.reset();
        superframe->cancelSyncEvents();
        if (operationId) phy->cancel(*operationId);
        if (managementClient)
            managementClient->handleSyncConfirm(cancelledId,{Ieee802154MacServiceAbortReason::CANCELLED});
        restoreIdle();
        return Ieee802154MacServiceCancelResult::CANCELLED;
    }
    if (startRequest && startRequest->id == id)
        return Ieee802154MacServiceCancelResult::NOT_CANCELLABLE;
    if (superframeStartRequest && superframeStartRequest->id == id && superframeStartRequest->awaitingFirstBoundary &&
            !resetRequest && !retiringRequests && started && superframeStartRequest->originalChannel &&
            *superframeStartRequest->originalChannel == superframeStartRequest->parameters.channel) {
        superframe->cancelBeaconPreparation();
        completeSuperframeStart(Ieee802154MacServiceAbortReason::CANCELLED);
        return Ieee802154MacServiceCancelResult::CANCELLED;
    }
    if (superframeStartRequest && superframeStartRequest->id == id)
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
    if (gtsRequest && gtsRequest->id == id && (!transaction || transaction->id != id)) {
        if (resetRequest || !started || retiringRequests)
            return Ieee802154MacServiceCancelResult::NOT_CANCELLABLE;
        completeGts(Ieee802154MacServiceAbortReason::CANCELLED);
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
            resumeAvailablePhy();
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
    if (!started || !phyAvailable || resetRequest || retiringRequests || (transaction && transaction->phase != Phase::WAIT_GTS) || ackExchange || managementRequest || startRequest ||
            (superframeStartRequest && ((superframeStartRequest->phase == SuperframeStartRequest::Phase::INITIAL && !superframeStartRequest->awaitingFirstBoundary) ||
                    superframeStartRequest->phase == SuperframeStartRequest::Phase::IMMEDIATE_ACTIVATION)) ||
            interruptedSuperframeChannel || interruptedScanCcaDuration || scanRequest || beaconPreparation || orphanResponseRequest || phyOperation || !phy)
        return;
    if (beaconResponses && !beaconScheduleActive()) {
        --beaconResponses;
        transmitBeacon(0,true);
        return;
    }
    if (autoPollPending && !pollRequest && !associationRequest && parentAssociated) {
        auto advertisedSource = *autoPollPending;
        autoPollPending.reset();
        auto address = std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value);
        if (getInteger("macCoordShortAddress") < 0xfffe)
            address = Ieee802154Address::fromShort(getInteger("macCoordShortAddress"));
        pollRequest = PollRequest{0,{uint16_t(getInteger("macPanId")),address}};
        pollRequest->automatic = true;
        pollRequest->advertisedSource = advertisedSource;
        startPoll();
        return;
    }
    if (pendingPanConflict && (!beaconScheduleActive() || slottedAccess)) {
        if (!panCoordinator && parentAssociated && getBoolean("macAssociatedPanCoord") && pendingPanConflict->panId == getInteger("macPanId") &&
                pendingPanConflict->coordinator == std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value)) {
            transmitPanConflict();
            return;
        }
        pendingPanConflict.reset();
    }
    bool idleReceive = getBoolean("macRxOnWhenIdle");
    if (beaconScheduleActive())
        idleReceive = idleReceive && incomingCapEnd && *incomingCapEnd > superframe->getCurrentClockTime();
    auto target = cfpReceive || outgoingCapActive || (synchronization && synchronization->installed && synchronization->wantsRx) || idleReceive || getBoolean("macPromiscuousMode") || (pollRequest && pollRequest->waiting) || receiverWantsRx() || broadcastReceivePending ? PhyState::RECEIVE : PhyState::OFF;
    if (target != phyState)
        requestState(target, PhyPurpose::IDLE);
    if (receiverWantsRx() && phyState == PhyState::RECEIVE && !receiverWindow->startsAt) {
        receiverWindow->startsAt = simTime();
        if (receiverWindow->duration != 0xffffff)
            scheduleAt(simTime() + SimTime(receiverWindow->duration * int64_t(16),SIMTIME_US),receiverEndTimer);
    }
}

void Ieee802154StandardMac::clearCfp()
{
    if (superframe)
        superframe->cancelCfpBoundary();
    cfpWindows.clear();
    cfpReceive = false;
}

void Ieee802154StandardMac::installCfpWindows(clocktime_t anchor, uint8_t superframeOrder, bool outgoing, const std::vector<Ieee802154GtsManager::Grant>& grants)
{
    std::vector<CfpWindow> ending;
    for (const auto& window : cfpWindows)
        if (window.outgoing && !window.activitySettled && window.physicalEnd <= simTime())
            ending.push_back(window);
    clearCfp();
    if (!superframe || superframeOrder >= 15 || scanRequest)
        return;
    auto slot = SimTime((int64_t(60) << superframeOrder) * 16,SIMTIME_US);
    auto now = superframe->getCurrentClockTime();
    for (const auto& grant : grants) {
        auto start = anchor + SIMTIME_AS_CLOCKTIME(slot * grant.start);
        auto end = anchor + SIMTIME_AS_CLOCKTIME(slot * (grant.start + grant.length));
        if (start <= now || end <= now)
            continue;
        cfpWindows.push_back({grant.key,superframe->getFutureSimulationTime(start),
                superframe->getFutureSimulationTime(end),outgoing ? grant.key.direction == Ieee802154GtsDirection::DEVICE_TRANSMIT :
                grant.key.direction == Ieee802154GtsDirection::DEVICE_RECEIVE,outgoing,false,
                outgoing && outgoingEpoch ? outgoingEpoch->beaconOrder : uint8_t(15),CLOCKTIME_AS_SIMTIME(anchor).raw()});
    }
    auto installed = cfpWindows;
    cfpWindows.insert(cfpWindows.end(),ending.begin(),ending.end());
    auto receiveEpoch = receiveRetirementEpoch;
    for (const auto& window : installed) {
        Ieee802154MacWindowObservation observation;
        observation.start = window.physicalStart; observation.end = window.physicalEnd;
        observation.outgoing = window.outgoing; observation.receive = window.receive;
        observation.shortAddress = window.key.shortAddress; observation.direction = window.key.direction;
        emit(cfpWindowInstalledSignal,1L,&observation);
        if (receiveEpoch != receiveRetirementEpoch || !started || resetRequest ||
                std::none_of(cfpWindows.begin(),cfpWindows.end(),[&](const auto& current) {
                    return current.physicalStart == window.physicalStart && current.physicalEnd == window.physicalEnd &&
                            current.outgoing == window.outgoing && current.key.panId == window.key.panId &&
                            current.key.extendedAddress == window.key.extendedAddress && current.key.shortAddress == window.key.shortAddress &&
                            current.key.direction == window.key.direction;
                }))
            return;
    }
    refreshCfp();
}

void Ieee802154StandardMac::refreshCfp()
{
    if (!superframe || !phy || !started || !phyAvailable || resetRequest) {
        clearCfp();
        return;
    }
    if (scanRequest) {
        superframe->cancelCfpBoundary();
        cfpReceive = false;
        return;
    }
    superframe->cancelCfpBoundary();
    cfpReceive = false;
    const CfpWindow *tx = nullptr;
    std::optional<simtime_t> nextPhysical;
    auto matches = [](const Ieee802154GtsManager::Key& a, const Ieee802154GtsManager::Key& b) {
        return a.panId == b.panId && a.extendedAddress == b.extendedAddress && a.shortAddress == b.shortAddress && a.direction == b.direction;
    };
    auto lead = phy->getTimingParameters().turnaroundTime;
    for (const auto& window : cfpWindows) {
        if (!gtsManager.findGrant(window.key))
            continue;
        if (window.outgoing && !window.activitySettled) {
            auto end = std::max(window.physicalEnd,simTime());
            if (!nextPhysical || end < *nextPhysical) nextPhysical = end;
        }
        bool wantedTx = !window.receive && transaction && transaction->gtsKey && matches(window.key,*transaction->gtsKey);
        if (!window.receive && !wantedTx)
            continue;
        if (window.physicalEnd <= simTime())
            continue;
        auto wake = window.physicalStart - lead;
        if (wake <= simTime()) {
            if (window.receive)
                cfpReceive = true;
            else if (transaction->phase == Phase::WAIT_GTS)
                tx = &window;
            if (!nextPhysical || window.physicalEnd < *nextPhysical) nextPhysical = window.physicalEnd;
        }
        else if (!nextPhysical || wake < *nextPhysical) nextPhysical = wake;
    }
    if (nextPhysical)
        superframe->scheduleCfpBoundary(*nextPhysical);
    if (tx && !cfpReceive && transaction && transaction->phase == Phase::WAIT_GTS && !ackExchange && !phyOperation) {
        auto duration = Ieee802154ExchangeTiming::exchangeDuration(transaction->frame->getChunkLength(),transaction->frame->getAckRequest(),
                SimTime(par("maxPropagationDelay")),getInteger("macSifsPeriod"),getInteger("macLifsPeriod"),phy->getTimingParameters());
        if (duration > tx->physicalEnd - tx->physicalStart) {
            completeData(Ieee802154MacStatus::FRAME_TOO_LONG);
            return;
        }
        auto start = std::max({tx->physicalStart,simTime() + lead,eligibleAt});
        if (Ieee802154ExchangeTiming::fits(start,tx->physicalEnd,duration)) {
            transaction->accessStartAt = start;
            transaction->gtsEnd = tx->physicalEnd;
            transaction->phase = Phase::WAIT_TX;
            requestState(PhyState::TRANSMIT,PhyPurpose::TX_READY);
            return;
        }
    }
    restoreIdle();
}

void Ieee802154StandardMac::handleCfpBoundary()
{
    Enter_Method("handleCfpBoundary");
    std::vector<Ieee802154GtsManager::Key> expired;
    for (auto& window : cfpWindows)
        if (window.outgoing && !window.activitySettled && window.physicalEnd <= simTime()) {
            window.activitySettled = true;
            if (gtsManager.finishOpportunity(window.key,window.beaconOrder,window.epoch))
                expired.push_back(window.key);
            else if (window.rephaseAfter)
                gtsManager.rephaseActivity(window.key,*window.rephaseAfter);
        }
    for (const auto& key : expired) {
        auto epoch = receiveRetirementEpoch;
        expireGtsGrant(key);
        if (epoch != receiveRetirementEpoch || !started || resetRequest)
            return;
    }
    refreshCfp();
}

void Ieee802154StandardMac::expireGtsGrant(const Ieee802154GtsManager::Key& key)
{
    if (constructedGtsCandidate) {
        auto& candidate = *constructedGtsCandidate;
        removeGtsFromLedger(candidate.ledger,key);
        candidate.removed.erase(std::remove_if(candidate.removed.begin(),candidate.removed.end(),[&](const auto& grant) {
            return grant.key.panId == key.panId && grant.key.extendedAddress == key.extendedAddress &&
                    grant.key.shortAddress == key.shortAddress && grant.key.direction == key.direction;
        }),candidate.removed.end());
    }
    removeGtsGrant(key);
}

void Ieee802154StandardMac::emitGtsObservation(simsignal_t signal, const Ieee802154GtsManager::Grant& grant)
{
    Ieee802154MacGtsObservation observation;
    observation.shortAddress = grant.key.shortAddress;
    observation.direction = grant.key.direction; observation.length = grant.length;
    emit(signal,1L,&observation);
}

bool Ieee802154StandardMac::settleSkippedGtsOpportunities()
{
    if (!panCoordinator || !outgoingEpoch || outgoingEpoch->beaconOrder >= 15 || outgoingEpoch->superframeOrder >= 15)
        return true;
    const auto bo = outgoingEpoch->beaconOrder;
    const auto interval = SimTime((int64_t(960) << bo) * 16,SIMTIME_US).raw();
    const auto duration = SimTime((int64_t(960) << outgoingEpoch->superframeOrder) * 16,SIMTIME_US).raw();
    const auto now = CLOCKTIME_AS_SIMTIME(superframe->getCurrentClockTime()).raw();
    auto grants = gtsManager.getGrants();
    for (const auto& grant : grants) {
        if (!grant.activityArmed || !grant.lastOpportunity)
            continue;
        auto first = *grant.lastOpportunity + interval;
        if (first >= now - duration)
            continue;
        auto count = (now - duration - 1 - first) / interval + 1;
        auto key = grant.key;
        if (grant.activitySuspended) {
            (void)gtsManager.finishOpportunity(key,bo,first + (count - 1) * interval);
            continue;
        }
        for (int64_t i = 0; i < count; ++i)
            if (gtsManager.finishOpportunity(key,bo,first + i * interval)) {
                auto epoch = receiveRetirementEpoch;
                expireGtsGrant(key);
                if (epoch != receiveRetirementEpoch || !started || resetRequest || !periodicBeaconing)
                    return false;
                break;
            }
    }
    return true;
}

void Ieee802154StandardMac::retireInvalidGtsData()
{
    if (!transaction || !transaction->gtsKey || gtsManager.findGrant(*transaction->gtsKey))
        return;
    bool unstarted = transaction->phase == Phase::WAIT_GTS || transaction->phase == Phase::WAIT_TX ||
            (transaction->phase == Phase::TRANSMIT && phyOperation && !phyOperation->transmitStarted);
    if (!unstarted)
        return;
    auto outerRetirement = retiringRequests;
    retiringRequests = true;
    if (transaction->phase != Phase::WAIT_GTS)
        retireDataRadio();
    completeData(Ieee802154MacStatus::INVALID_GTS);
    retiringRequests = outerRetirement;
}

void Ieee802154StandardMac::startData()
{
    if (!transaction || resetRequest || !started || !phyAvailable || retiringRequests || ackExchange)
        return;
    if (!client) {
        completeData(Ieee802154MacServiceAbortReason::STOPPED);
        return;
    }
    if (transaction->gtsKey) {
        if (!gtsManager.findGrant(*transaction->gtsKey)) {
            completeData(Ieee802154MacStatus::INVALID_GTS);
            return;
        }
        transaction->phase = Phase::WAIT_GTS;
        refreshCfp();
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
    auto window = superframe ? superframe->getAccessWindow() : std::nullopt;
    bool toParent = false;
    if (periodicBeaconing && !panCoordinator && parentAssociated) {
        const auto& destination = transaction->frame->getDestinationAddress();
        auto parentExtended = std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value);
        toParent = transaction->kind == Transaction::Kind::GTS_REQUEST || (transaction->kind != Transaction::Kind::SUPERFRAME_REALIGN &&
                transaction->frame->getDestinationPanId() == getInteger("macPanId") &&
                ((destination.isShort() && destination.getShort() < 0xfffe && destination.getShort() == getInteger("macCoordShortAddress")) ||
                 (destination.isExtended() && !destination.isExtendedGroup() && destination == parentExtended)));
        auto required = toParent ? Ieee802154AccessWindowOrigin::INCOMING : Ieee802154AccessWindowOrigin::OUTGOING;
        if (window && window->origin != required) {
            if (transaction->accessPending) {
                transaction->phase = Phase::ACCESS;
                return; // Keep the provider's existing attempt until a matching window is published.
            }
            window.reset();
        }
    }
    if (window && window->beaconOrder < 15)
        transaction->capBeaconOrder = window->beaconOrder;
    else if (synchronization && synchronization->installed)
        transaction->capBeaconOrder = synchronization->beaconOrder;
    else if (transaction->capBeaconOrder == 15 && getInteger("macBeaconOrder") < 15)
        transaction->capBeaconOrder = getInteger("macBeaconOrder");
    bool scheduled = transaction->kind != Transaction::Kind::SCAN_REQUEST && slottedAccess && superframe && (window || periodicBeaconing ||
            (synchronization && synchronization->installed) || transaction->capBeaconOrder < 15 || transaction->transmittedInCap);
    if (transaction->search == Transaction::Search::FALLBACK && !transaction->transmittedInCap)
        scheduled = false;
    if (scheduled && !window) {
        transaction->phase = Phase::WAIT_CAP;
        if ((!periodicBeaconing || toParent) && !(synchronization && synchronization->installed) &&
                transaction->search == Transaction::Search::NONE) {
            transaction->search = Transaction::Search::SEARCHING;
            auto symbols = int64_t(960) * ((int64_t(1) << transaction->capBeaconOrder) + 1);
            superframe->scheduleAccessSearchDeadline(superframe->getCurrentClockTime() +
                    SIMTIME_AS_CLOCKTIME(SimTime(symbols * 16, SIMTIME_US)));
        }
        return;
    }
    if (window && transaction->search == Transaction::Search::SEARCHING) {
        superframe->cancelAccessSearchDeadline();
        transaction->search = Transaction::Search::NONE;
    }
    transaction->phase = Phase::ACCESS;
    ccaDeferred = false;
    slottedCcaNeedsAlignment = false;
    if (scheduled && transaction->slotted && transaction->accessPending) {
        auto id = transaction->accessId;
        auto nextWindow = *window;
        nextWindow.notBefore = std::max(nextWindow.notBefore, eligibleAt);
        auto admission = slottedAccess->updateWindow(id, nextWindow);
        if (admission != Ieee802154MacServiceAdmission::ACCEPTED && transaction && transaction->accessId == id)
            completeData(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE);
        return;
    }
    if (accessId == UINT64_MAX)
        throw cRuntimeError("IEEE 802.15.4 internal access request IDs exhausted");
    transaction->accessId = ++accessId;
    Ieee802154AccessRequest request{uint8_t(getInteger("macMinBe")), uint8_t(getInteger("macMaxBe")),
            uint8_t(getInteger("macMaxCsmaBackoffs")), SimTime(16, SIMTIME_US) * getInteger("macUnitBackoffPeriod")};
    request.exchangeDuration = Ieee802154ExchangeTiming::exchangeDuration(transaction->frame->getChunkLength(),
            transaction->frame->getAckRequest(),SimTime(par("maxPropagationDelay")),getInteger("macSifsPeriod"),getInteger("macLifsPeriod"),phy->getTimingParameters());
    request.ifsDuration = interframeSpacing(transaction->frame->getChunkLength());
    if (scheduled) {
        request.window = *window;
        request.window->notBefore = std::max(request.window->notBefore, eligibleAt);
        request.ccaDuration = pib->getCcaDuration();
        request.turnaroundTime = phy->getTimingParameters().turnaroundTime;
        if (transaction->frame->getAckRequest()) {
            auto aifs = phy->getTimingParameters().symbolPeriod * getInteger("macSifsPeriod");
            request.exchangeDuration += Ieee802154ExchangeTiming::slottedAckWait(phy->getTimingParameters(), aifs,
                    request.unitBackoffPeriod, SimTime(par("maxPropagationDelay"))) -
                    Ieee802154ExchangeTiming::ackWait(phy->getTimingParameters()) - 2 * SimTime(par("maxPropagationDelay"));
        }
    }
    transaction->slotted = scheduled;
    transaction->accessPending = true;
    auto id = transaction->accessId;
    auto admission = (scheduled ? slottedAccess.get() : access.get())->start(id, request);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED && transaction && transaction->accessId == id)
        completeData(admission == Ieee802154MacServiceAdmission::UNAVAILABLE ?
                Ieee802154MacServiceResult(Ieee802154MacServiceAbortReason::STOPPED) :
                Ieee802154MacServiceResult(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE));
}

void Ieee802154StandardMac::completeData(const Ieee802154MacServiceResult& result)
{
    auto completed = *transaction;
    transaction.reset();
    cancelEvent(dataTimer);
    cancelEvent(ackTimer);
    ccaDeferred = false;
    slottedCcaNeedsAlignment = false;
    if (superframe)
        superframe->cancelAccessSearchDeadline();
    if (completed.accessPending)
        (completed.slotted ? slottedAccess.get() : access.get())->cancel(completed.accessId, Ieee802154MacServiceAbortReason::CANCELLED);
    delete completed.msdu;
    if ((completed.kind == Transaction::Kind::BEACON || completed.kind == Transaction::Kind::AUTO_BEACON) &&
            result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) && completed.frame->getFramePending())
        pendingPolls.push_front({uint16_t(completed.frame->getSourcePanId()),Ieee802154Address::fromShort(0xffff),simTime()});
    if (completed.kind == Transaction::Kind::SUPERFRAME_REALIGN) {
        if (superframeStartRequest && superframeStartRequest->phase == SuperframeStartRequest::Phase::COMMAND) {
            if (result != Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS)) {
                completeSuperframeStart(result);
                scheduleNextPeriodicBeacon();
            }
            else if (periodicBeaconing)
                superframeStartRequest->phase = SuperframeStartRequest::Phase::WAIT_ACTIVATION;
            else {
                superframeStartRequest->phase = SuperframeStartRequest::Phase::IMMEDIATE_ACTIVATION;
                auto operationId = allocatePhyId();
                phyOperation = PhyOperation{operationId,PhyPurpose::SUPERFRAME_GET};
                if (phy->submitGet(operationId,{"phyCurrentChannelInfo"}) != PhyAdmission::ACCEPTED) {
                    phyOperation.reset();
                    completeSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
                }
            }
        }
    }
    else if (completed.kind == Transaction::Kind::PAN_CONFLICT) {
        auto conflict = pendingPanConflict;
        pendingPanConflict.reset();
        if (conflict && started && !resetRequest && parentAssociated && getBoolean("macAssociatedPanCoord") &&
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
            if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS)) {
                associationRequest->waitingBeacon = synchronization && synchronization->installed && synchronization->request.trackBeacon;
                scheduleAt(simTime() + SimTime(15360,SIMTIME_US) * getInteger("macResponseWaitTime"),associationTimer);
            }
            else
                completeAssociate(result);
        }
    }
    else if (completed.kind == Transaction::Kind::GTS_REQUEST) {
        if (gtsRequest && gtsRequest->id == completed.id) {
            if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) && !gtsRequest->request.characteristics.allocate)
                completeGts(result);
            else if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) && synchronization && superframe) {
                gtsRequest->waiting = true;
                auto symbols = int64_t(4 * 960) << synchronization->beaconOrder;
                auto deadline = superframe->getCurrentClockTime() + SIMTIME_AS_CLOCKTIME(SimTime(symbols * 16,SIMTIME_US));
                scheduleAt(superframe->getFutureSimulationTime(deadline),gtsTimer);
            }
            else
                completeGts(result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) ?
                        Ieee802154MacServiceResult(Ieee802154MacServiceAbortReason::STOPPED) : result);
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
        if (!completed.slotted && completed.pendingDevice && completed.pendingDevice->second.isBroadcast() &&
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
    if (resetRequest->retiring || pendingCallbacksActive || syncLossNotificationsActive) {
        resetRequest->deferredResult = result;
        return;
    }
    auto request = *resetRequest;
    if (result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS)) {
        auto defaultDsn = request.defaults ? intuniform(0, 255, par("dsnRng")) : 0;
        auto defaultBsn = request.defaults && par("enableManagement").boolValue() ? intuniform(0, 255, par("dsnRng")) : 0;
        pib->reset(request.defaults, defaultDsn, defaultBsn);
        if (request.defaults) {
            panCoordinator = initialPanCoordinator;
            coordinator = initialPanCoordinator;
            parentAssociated = false;
            interruptedSuperframeChannel.reset();
            interruptedScanCcaDuration.reset();
        }
        if (request.defaults)
            associatedDevices.clear();
        publishIdentity();
        phyState = PhyState::OFF;
        eligibleAt = simTime();
    }
    resetRequest.reset();
    if (interruptedScanCcaDuration)
        resumeAvailablePhy();
    if (client)
        client->handleResetConfirm(request.id, Ieee802154MacResetConfirm(request.defaults, result));
    restoreIdle();
}

void Ieee802154StandardMac::retireReceives()
{
    ++receiveRetirementEpoch;
    if (superframe)
        superframe->clearReceptionAnchors();
}

void Ieee802154StandardMac::stop()
{
    clearCfp();
    started = false;
    constructedGtsCandidate.reset();
    autoPollPending.reset();
    retireReceives();
    incomingSuperframe.reset();
    incomingCapEnd.reset();
    if (superframe) {
        superframe->clearAccessWindow();
        superframe->cancelAccessSearchDeadline();
    }
    retirePeriodicBeaconing(Ieee802154MacServiceAbortReason::STOPPED);
    retireSynchronization(Ieee802154MacServiceAbortReason::STOPPED);
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
    if (gtsRequest)
        completeGts(Ieee802154MacServiceAbortReason::STOPPED);
    gtsManager.clear();
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
            if (associationRequest->waitingBeacon) {
                completeAssociate(Ieee802154MacStatus::NO_DATA);
                return;
            }
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
        if (superframe)
            superframe->retireExpiredReceptionAnchors();
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
    if (message == gtsTimer) {
        completeGts(Ieee802154MacStatus::NO_DATA);
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
    slottedCcaNeedsAlignment = false;
    performCca();
}

void Ieee802154StandardMac::handleAvailabilityIndication(const physicallayer::Ieee802154PhyAvailabilityIndication& indication)
{
    Enter_Method("handleAvailabilityIndication");
    phyAvailable = indication.available;
    phyState = indication.state;
    if (indication.available) {
        resumeAvailablePhy();
        return;
    }
    bool outerRetirement = retiringRequests;
    retiringRequests = true;
    clearCfp();
    constructedGtsCandidate.reset();
    autoPollPending.reset();
    if (scanRequest && scanRequest->originalChannel)
        interruptedSuperframeChannel = scanRequest->originalChannel;
    if (scanRequest && scanRequest->originalCcaDuration)
        interruptedScanCcaDuration = scanRequest->originalCcaDuration;
    phyOperation.reset();
    beaconAfterAck.reset();
    retireReceives();
    incomingCapEnd.reset();
    if (superframe)
        superframe->cancelCapEnd();
    if (superframe) {
        superframe->clearAccessWindow();
        superframe->cancelAccessSearchDeadline();
    }
    setOutgoingCapActive(false);
    if (superframe) {
        superframe->cancelBeaconPreparation();
        if (synchronization && !synchronization->installed)
            retireSynchronization(Ieee802154MacServiceAbortReason::STOPPED);
        if (superframeStartRequest) {
            interruptedSuperframeChannel = superframeStartRequest->originalChannel;
            completeSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
        }
    }
    if (!started) {
        retiringRequests = outerRetirement;
        return;
    }
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
    if (gtsRequest)
        completeGts(Ieee802154MacServiceAbortReason::STOPPED);
    if (pollRequest)
        completePoll(Ieee802154MacServiceAbortReason::STOPPED);
    if (scanRequest)
        finishScan(Ieee802154MacServiceAbortReason::STOPPED);
    finishReset(Ieee802154MacServiceAbortReason::STOPPED);
    retiringRequests = outerRetirement;
    resumeAvailablePhy();
}

void Ieee802154StandardMac::resumeAvailablePhy()
{
    if (!started || !phyAvailable || retiringRequests || resetRequest)
        return;
    if (interruptedSuperframeChannel) {
        auto id = allocatePhyId();
        phyOperation = PhyOperation{id,PhyPurpose::SUPERFRAME_PHY_RESTORE};
        if (phy->submitSet(id,{"phyCurrentChannelInfo",*interruptedSuperframeChannel}) != PhyAdmission::ACCEPTED) {
            phyOperation.reset();
            interruptedSuperframeChannel.reset();
            retirePeriodicBeaconing(Ieee802154MacServiceAbortReason::STOPPED);
        }
        return;
    }
    if (interruptedScanCcaDuration) {
        auto id = allocatePhyId();
        phyOperation = PhyOperation{id,PhyPurpose::SCAN_PHY_CCA_RESTORE};
        if (phy->submitSet(id,{"phyCcaDuration",*interruptedScanCcaDuration}) != PhyAdmission::ACCEPTED)
            phyOperation.reset();
        return;
    }
    if (outgoingCapEnd && *outgoingCapEnd > superframe->getCurrentClockTime()) {
        setOutgoingCapActive(scheduleCapDutyEnd());
        if (!started || !phyAvailable || retiringRequests || resetRequest)
            return;
    }
    else
        outgoingCapEnd.reset();
    scheduleNextPeriodicBeacon();
    restoreIdle();
}

void Ieee802154StandardMac::performCca()
{
    if (!transaction || !ccaDeferred)
        return;
    if (ackExchange || phyOperation) {
        slottedCcaNeedsAlignment = transaction->slotted;
        return;
    }
    if (simTime() < eligibleAt) {
        slottedCcaNeedsAlignment = transaction->slotted;
        if (!dataTimer->isScheduled())
            scheduleAt(eligibleAt, dataTimer);
        return;
    }
    if (phyState != PhyState::RECEIVE) {
        slottedCcaNeedsAlignment = transaction->slotted;
        requestState(PhyState::RECEIVE, PhyPurpose::IDLE);
        return;
    }
    ccaDeferred = false;
    if (transaction->slotted && slottedCcaNeedsAlignment) {
        slottedCcaNeedsAlignment = false;
        auto id = transaction->accessId;
        slottedAccess->deferCca(id, simTime());
        return;
    }
    auto internalId = allocatePhyId();
    phyOperation = PhyOperation{internalId, PhyPurpose::CCA};
    auto admission = phy->submitCca(internalId);
    if (admission != PhyAdmission::ACCEPTED) {
        phyOperation.reset();
        completeData(admission == PhyAdmission::UNAVAILABLE ? Ieee802154MacServiceResult(Ieee802154MacServiceAbortReason::STOPPED) :
                Ieee802154MacServiceResult(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE));
    }
}

void Ieee802154StandardMac::handleAccessConfirm(uint64_t id, const Ieee802154MacServiceResult& result, uint8_t count, std::optional<simtime_t> startAt)
{
    Enter_Method("handleAccessConfirm");
    if (!transaction || transaction->accessId != id || transaction->phase != Phase::ACCESS)
        return;
    transaction->accessPending = false;
    if (result != Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS))
        completeData(result);
    else {
        if (startAt && *startAt < simTime()) {
            completeData(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE);
            return;
        }
        transaction->backoffs = std::min<int>(255, transaction->backoffs + count);
        transaction->accessStartAt = startAt;
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
    if (purpose == PhyPurpose::SYNC_READY && synchronization && !synchronization->installed) {
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS))
            retireSynchronization(macResult(confirm.result));
        else {
            phyState = *confirm.state;
            synchronization->installed = true;
            synchronization->wantsRx = true;
            scheduleAcquisitionWindow();
            completeSyncInstallation(Ieee802154MacStatus::SUCCESS);
        }
        return;
    }
    if (purpose == PhyPurpose::SUPERFRAME_READY) {
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
            if (superframeStartRequest && !superframeStartRequest->awaitingFirstBoundary)
                failSuperframeStart(macResult(confirm.result));
            else
                skipPeriodicBeacon();
        }
        else {
            phyState = *confirm.state;
            sendPeriodicBeacon();
        }
        return;
    }
    if (purpose == PhyPurpose::SUPERFRAME_RETUNE_OFF && superframeStartRequest) {
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
            failSuperframeStart(macResult(confirm.result));
            return;
        }
        phyState = PhyState::OFF;
        auto operationId = allocatePhyId();
        phyOperation = PhyOperation{operationId,PhyPurpose::SUPERFRAME_GET};
        if (phy->submitGet(operationId,{"phyCurrentChannelInfo"}) != PhyAdmission::ACCEPTED) {
            phyOperation.reset();
            failSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
        }
        return;
    }
    if (purpose == PhyPurpose::SUPERFRAME_ACTIVATION_READY && superframeStartRequest) {
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
            failSuperframeStart(macResult(confirm.result));
            return;
        }
        phyState = *confirm.state;
        auto request = superframeStartRequest->parameters;
        auto requestId = superframeStartRequest->id;
        auto geometry = Ieee802154SuperframeTiming::geometry(request.beaconOrder,request.superframeOrder);
        bool delayedChild = !request.panCoordinator && request.beaconOrder < 15 && request.startTime && incomingSuperframe;
        auto firstTarget = delayedChild ? std::optional<clocktime_t>(advanceChildBeaconTarget(request,*superframeStartRequest->firstTarget)) : std::nullopt;
        if (!request.panCoordinator && request.beaconOrder < 15 && !request.startTime && incomingSuperframe) {
            auto candidate = makeBeacon(request.panId,request.beaconOrder,request.superframeOrder,request.batteryLifeExtension,false);
            if (!candidate || overlapsIncomingSuperframe(superframe->getCurrentClockTime(),request.beaconOrder,request.superframeOrder,candidate->getChunkLength())) {
                failSuperframeStart(candidate ? Ieee802154MacStatus::SUPERFRAME_OVERLAP : Ieee802154MacStatus::FRAME_TOO_LONG);
                return;
            }
        }
        if (!commitSuperframeChannel())
            return;
        pib->updateSuperframeConfiguration(request.panId,request.beaconOrder,geometry->superframeOrder,request.batteryLifeExtension);
        panCoordinator = request.panCoordinator;
        coordinator = true;
        periodicBeaconing = request.beaconOrder < 15;
        periodicTarget = firstTarget;
        if (delayedChild) {
            auto rounded = Ieee802154SuperframeTiming::roundStartTime(request.startTime,getInteger("macUnitBackoffPeriod"));
            childPhase = SIMTIME_AS_CLOCKTIME(SimTime(int64_t(rounded % *geometry->beaconInterval) * 16,SIMTIME_US));
            outgoingEpoch = BeaconConfiguration{request.beaconOrder,geometry->superframeOrder,request.batteryLifeExtension,
                    uint32_t(getInteger("macBattLifeExtPeriods")),uint32_t(getInteger("macUnitBackoffPeriod"))};
            transmittedBeaconAnchor.reset();
        }
        publishIdentity();
        superframeStartRequest.reset();
        if (delayedChild)
            scheduleNextPeriodicBeacon();
        if (managementClient)
            managementClient->handleStartSuperframeConfirm(requestId,{Ieee802154MacStatus::SUCCESS});
        if (!delayedChild && periodicBeaconing && started && phyAvailable && !resetRequest && !phyOperation && !transaction)
            sendPeriodicBeacon();
        else
            restoreIdle();
        return;
    }
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
        if (transaction->gtsKey && (!gtsManager.findGrant(*transaction->gtsKey) || !transaction->gtsEnd)) {
            completeData(Ieee802154MacStatus::INVALID_GTS);
            return;
        }
        if (transaction->accessStartAt && *transaction->accessStartAt < simTime()) {
            if (transaction->gtsKey) {
                transaction->phase = Phase::WAIT_GTS;
                refreshCfp();
                return;
            }
            completeData(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE);
            return;
        }
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
        if (transaction->gtsKey && !gtsManager.findGrant(*transaction->gtsKey)) {
            delete packet;
            retireInvalidGtsData();
            return;
        }
        physicallayer::Ieee802154PhyTransmitRequest request;
        request.destination = transaction->frame->getDestinationAddress();
        request.startAt = transaction->accessStartAt;
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
        (transaction->slotted ? slottedAccess.get() : access.get())->handleCcaResult(transaction->accessId, *confirm.outcome == physicallayer::Ieee802154PhyCcaOutcome::IDLE);
    else
        completeData(std::holds_alternative<physicallayer::Ieee802154PhyServiceAbortReason>(confirm.result) ?
                macResult(confirm.result) : Ieee802154MacServiceResult(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE));
}

void Ieee802154StandardMac::handleTransmitStarted(uint64_t id, const physicallayer::Ieee802154PhyTransmitStart& start)
{
    Enter_Method("handleTransmitStarted");
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::ACK_TRANSMIT) {
        phyOperation->transmitStarted = true;
        phyOperation->expectedTransmitEnd = start.expectedPpduEnd;
        return;
    }
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::SUPERFRAME_TRANSMIT) {
        phyOperation->transmitStarted = true;
        auto candidate = std::move(constructedGtsCandidate);
        constructedGtsCandidate.reset();
        if (candidate) {
            candidate->ledger.copyActivityFrom(gtsManager);
            gtsManager = std::move(candidate->ledger);
        }
        gtsManager.markPublished(constructedGtsAdvertisements);
        auto now = superframe->getCurrentClockTime();
        if (candidate && candidate->agesImmediateEpoch)
            lastGtsAgedBoundary = now;
        auto previousAnchor = transmittedBeaconAnchor;
        transmittedBeaconAnchor = now;
        auto previousEpoch = outgoingEpoch;
        outgoingEpoch = constructedBeacon;
        if (panCoordinator && outgoingEpoch) {
            bool changedOrder = previousEpoch && previousEpoch->beaconOrder != outgoingEpoch->beaconOrder;
            auto interval = SimTime((int64_t(960) << outgoingEpoch->beaconOrder) * 16,SIMTIME_US);
            auto previous = CLOCKTIME_AS_SIMTIME(now).raw() - interval.raw();
            bool replacement = superframeStartRequest &&
                    ((superframeStartRequest->phase == SuperframeStartRequest::Phase::INITIAL && superframeStartRequest->preparingFirstBeacon) ||
                     superframeStartRequest->phase == SuperframeStartRequest::Phase::WAIT_ACTIVATION);
            bool changedPhase = replacement && !changedOrder && previousAnchor &&
                    (CLOCKTIME_AS_SIMTIME(now - *previousAnchor).raw() % interval.raw() != 0);
            if (changedOrder)
                clearCfp(); // A superseded copied opportunity cannot consume the new schedule.
            else if (changedPhase) {
                std::vector<CfpWindow> ending;
                for (auto window : cfpWindows)
                    if (window.outgoing && !window.activitySettled && window.physicalEnd <= simTime()) {
                        window.rephaseAfter = previous;
                        ending.push_back(window);
                    }
                clearCfp();
                cfpWindows = std::move(ending);
            }
            for (const auto& grant : constructedCfpGrants) {
                gtsManager.armActivity(grant.key,previous,changedOrder);
                bool awaitingEnd = std::any_of(cfpWindows.begin(),cfpWindows.end(),[&](const auto& window) {
                    return window.rephaseAfter && window.key.panId == grant.key.panId &&
                            window.key.extendedAddress == grant.key.extendedAddress &&
                            window.key.shortAddress == grant.key.shortAddress && window.key.direction == grant.key.direction;
                });
                if (changedPhase && !awaitingEnd)
                    gtsManager.rephaseActivity(grant.key,previous);
            }
        }
        auto timestamp = Ieee802154SuperframeTiming::timestamp(now);
        pib->updateBeaconTxTime(timestamp);
        auto outerNotifications = syncLossNotificationsActive;
        if (candidate)
            syncLossNotificationsActive = true;
        auto publishStart = [&]() {
            if (candidate) {
                for (const auto& grant : candidate->removed) {
                    emitGtsObservation(gtsDeallocatedSignal,grant);
                    if (managementClient)
                        managementClient->handleGtsIndication({Ieee802154Address::fromShort(grant.key.shortAddress),{grant.length,grant.key.direction,false}});
                }
            }
            emit(periodicBeaconStartSignal,long(timestamp));
            if (candidate) {
                syncLossNotificationsActive = outerNotifications;
                if (!outerNotifications && resetRequest && resetRequest->deferredResult) {
                    auto result = *resetRequest->deferredResult;
                    finishReset(result);
                }
            }
        };
        if (!candidate)
            publishStart();
        if (!phyOperation || phyOperation->id != id || phyOperation->purpose != PhyPurpose::SUPERFRAME_TRANSMIT)
            return;
        if (superframeStartRequest && ((superframeStartRequest->phase == SuperframeStartRequest::Phase::INITIAL && superframeStartRequest->preparingFirstBeacon) ||
                superframeStartRequest->phase == SuperframeStartRequest::Phase::WAIT_ACTIVATION)) {
            auto request = superframeStartRequest->parameters;
            auto requestId = superframeStartRequest->id;
            auto geometry = Ieee802154SuperframeTiming::geometry(request.beaconOrder,request.superframeOrder);
            if (!commitSuperframeChannel() || !phyOperation || phyOperation->id != id) {
                if (candidate) publishStart();
                return;
            }
            pib->updateSuperframeConfiguration(request.panId,request.beaconOrder,geometry->superframeOrder,request.batteryLifeExtension);
            panCoordinator = request.panCoordinator;
            coordinator = true;
            childPhase.reset();
            if (!request.panCoordinator && incomingSuperframe && synchronization && synchronization->installed && synchronization->request.trackBeacon) {
                auto interval = SimTime(int64_t(*geometry->beaconInterval) * 16,SIMTIME_US);
                auto phase = request.startTime ?
                        SimTime(int64_t(Ieee802154SuperframeTiming::roundStartTime(request.startTime,getInteger("macUnitBackoffPeriod"))) * 16,SIMTIME_US) :
                        CLOCKTIME_AS_SIMTIME(now - incomingSuperframe->anchor);
                auto raw = phase.raw() % interval.raw();
                if (raw < 0)
                    raw += interval.raw();
                childPhase = SIMTIME_AS_CLOCKTIME(SimTime::fromRaw(raw));
            }
            publishIdentity();
            if (candidate) publishStart();
            if (!superframeStartRequest || superframeStartRequest->id != requestId || !phyOperation || phyOperation->id != id)
                return;
            periodicBeaconing = true;
            periodicTarget = now + SIMTIME_AS_CLOCKTIME(SimTime(int64_t(*geometry->beaconInterval) * 16,SIMTIME_US));
            superframeStartRequest.reset();
            scheduleNextPeriodicBeacon();
            if (managementClient)
                managementClient->handleStartSuperframeConfirm(requestId,{Ieee802154MacStatus::SUCCESS});
        }
        else if (periodicBeaconing) {
            auto geometry = Ieee802154SuperframeTiming::geometry(outgoingEpoch->beaconOrder,outgoingEpoch->superframeOrder);
            auto interval = SIMTIME_AS_CLOCKTIME(SimTime(int64_t(*geometry->beaconInterval) * 16,SIMTIME_US));
            if (!childPhase && !panCoordinator && incomingSuperframe && synchronization && synchronization->installed && synchronization->request.trackBeacon) {
                auto raw = CLOCKTIME_AS_SIMTIME(now - incomingSuperframe->anchor).raw() % CLOCKTIME_AS_SIMTIME(interval).raw();
                if (raw < 0)
                    raw += CLOCKTIME_AS_SIMTIME(interval).raw();
                childPhase = SIMTIME_AS_CLOCKTIME(SimTime::fromRaw(raw));
            }
            if (!periodicTarget || (previousEpoch && previousEpoch->beaconOrder != outgoingEpoch->beaconOrder))
                periodicTarget = now + interval;
            else
                *periodicTarget += interval;
            scheduleNextPeriodicBeacon();
        }
        if (phyOperation && phyOperation->id == id && panCoordinator && outgoingEpoch)
            installCfpWindows(now,outgoingEpoch->superframeOrder,true,constructedCfpGrants);
        return;
    }
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::TRANSMIT && transaction) {
        phyOperation->transmitStarted = true;
        if (panCoordinator && transaction->gtsKey && transaction->gtsKey->direction == Ieee802154GtsDirection::DEVICE_RECEIVE)
            gtsManager.setActivitySuspended(*transaction->gtsKey,!transaction->frame->getAckRequest());
        if (transaction->slotted)
            transaction->transmittedInCap = true;
    }
    if (!phyOperation || phyOperation->id != id || phyOperation->purpose != PhyPurpose::TRANSMIT || !transaction ||
            (transaction->kind != Transaction::Kind::BEACON && transaction->kind != Transaction::Kind::AUTO_BEACON))
        return;
    auto timestamp = superframe ? Ieee802154SuperframeTiming::timestamp(superframe->getCurrentClockTime()) :
            uint32_t((start.ppduStart + SimTime(160,SIMTIME_US)).inUnit(SIMTIME_US) / 16) & 0xffffff;
    pib->updateBeaconTxTime(timestamp);
}

void Ieee802154StandardMac::handleTransmitConfirm(uint64_t id, const physicallayer::Ieee802154PhyTransmitConfirm& confirm)
{
    Enter_Method("handleTransmitConfirm");
    if (!phyOperation || phyOperation->id != id)
        return;
    auto purpose = phyOperation->purpose;
    auto transmitStarted = phyOperation->transmitStarted;
    phyOperation.reset();
    if (purpose == PhyPurpose::SUPERFRAME_TRANSMIT) {
        bool carryingBroadcast = transaction && transaction->kind == Transaction::Kind::INDIRECT &&
                transaction->pendingDevice && transaction->pendingDevice->second.isBroadcast();
        if (transmitStarted && confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) && transmittedBeaconBroadcastPending && !carryingBroadcast &&
                std::none_of(pendingPolls.begin(),pendingPolls.end(),[](const auto& poll) { return poll.device.isBroadcast(); }))
            pendingPolls.push_back({uint16_t(getInteger("macPanId")),Ieee802154Address::fromShort(0xffff),simTime()});
        if (superframeStartRequest && superframeStartRequest->phase == SuperframeStartRequest::Phase::WAIT_OLD_BEACON) {
            if (!transmitStarted || confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS))
                completeSuperframeStart(confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) ?
                        Ieee802154MacServiceResult(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE) : macResult(confirm.result));
            else {
                eligibleAt = std::max(eligibleAt, simTime() + transmittedBeaconIfs);
                startOutgoingCap();
                if (superframeStartRequest && superframeStartRequest->phase == SuperframeStartRequest::Phase::WAIT_OLD_BEACON) {
                    superframeStartRequest->phase = SuperframeStartRequest::Phase::COMMAND;
                    transmitSuperframeRealignment();
                }
            }
        }
        else if (superframeStartRequest && ((superframeStartRequest->phase == SuperframeStartRequest::Phase::INITIAL &&
                superframeStartRequest->preparingFirstBeacon && !superframeStartRequest->awaitingFirstBoundary) ||
                superframeStartRequest->phase == SuperframeStartRequest::Phase::WAIT_ACTIVATION))
            failSuperframeStart(confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) ?
                    Ieee802154MacServiceResult(Ieee802154MacStatus::INVALID_PARAMETER) : macResult(confirm.result));
        else {
            if (!transmitStarted)
                skipPeriodicBeacon();
            else if (confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
                eligibleAt = std::max(eligibleAt, simTime() + transmittedBeaconIfs);
                startOutgoingCap();
            }
            if (transaction)
                continueData();
            else {
                startPendingResponse();
                restoreIdle();
            }
        }
        if (panCoordinator && periodicBeaconing && outgoingEpoch && started && !resetRequest)
            maintainGtsCapacity(outgoingEpoch->superframeOrder);
        return;
    }
    if (purpose == PhyPurpose::ACK_TRANSMIT) {
        if (beaconAfterAck) {
            auto boundary = *beaconAfterAck;
            beaconAfterAck.reset();
            auto reservationId = allocatePhyId();
            phyOperation = PhyOperation{reservationId,PhyPurpose::SUPERFRAME_READY};
            finishAck(confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS));
            if (!phyOperation || phyOperation->id != reservationId || !ownsBeaconBoundary(boundary) || !started || !phyAvailable || resetRequest)
                return;
            phyOperation.reset();
            sendPeriodicBeacon();
            return;
        }
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
            auto ackWait = transaction->slotted ? Ieee802154ExchangeTiming::slottedAckWait(phy->getTimingParameters(),
                    phy->getTimingParameters().symbolPeriod * getInteger("macSifsPeriod"),
                    SimTime(16, SIMTIME_US) * getInteger("macUnitBackoffPeriod"), SimTime(par("maxPropagationDelay"))) :
                    2 * SimTime(par("maxPropagationDelay")) + Ieee802154ExchangeTiming::ackWait(phy->getTimingParameters());
            transaction->ackDeadline = confirm.timing->ppduEnd + ackWait;
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
    auto receiveEpoch = receiveRetirementEpoch;
    std::optional<Ieee802154SuperframeReceptionAnchor> anchor;
    if (superframe) {
        if (quality.transmissionId)
            anchor = superframe->takeReceptionAnchor(*quality.transmissionId);
        else
            superframe->retireExpiredReceptionAnchors();
    }
    bool consumed = consumeReceiverWindow();
    processReceivedPacket(packet,quality,anchor,receiveEpoch);
    if (consumed && receiveEpoch == receiveRetirementEpoch)
        restoreIdle();
}

void Ieee802154StandardMac::handleReceiveStarted(const physicallayer::Ieee802154PhyReceiveStart& start)
{
    Enter_Method("handleReceiveStarted");
    if (started && phyAvailable && superframe)
        superframe->recordReceptionStart(start);
}

void Ieee802154StandardMac::handleSuperframeBoundary(clocktime_t boundary)
{
    Enter_Method("handleSuperframeBoundary");
    if (scanRequest)
        return;
    bool firstBoundary = superframeStartRequest && superframeStartRequest->awaitingFirstBoundary && superframeStartRequest->firstTarget &&
            *superframeStartRequest->firstTarget == boundary && started && phyAvailable;
    if (!firstBoundary && periodicBeaconing && periodicTarget && *periodicTarget == boundary)
        ageGtsEpoch(boundary);
    if (firstBoundary) {
        superframeStartRequest->awaitingFirstBoundary = false;
        superframeStartRequest->preparingFirstBeacon = true;
    }
    else if (!periodicBeaconing || !periodicTarget || *periodicTarget != boundary || !started || !phyAvailable ||
            (superframeStartRequest && superframeStartRequest->phase == SuperframeStartRequest::Phase::INITIAL && !superframeStartRequest->awaitingFirstBoundary))
        return;
    if (superframeStartRequest && superframeStartRequest->phase == SuperframeStartRequest::Phase::WAIT_ACTIVATION) {
        if (superframeStartRequest->parameters.beaconOrder == 15 && !superframeStartRequest->awaitingShutdownBoundary) {
            superframeStartRequest->awaitingShutdownBoundary = true;
            if (!superframe->scheduleBeaconPreparation(boundary,SIMTIME_ZERO)) {
                superframeStartRequest->awaitingShutdownBoundary = false;
                failSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
            }
            return;
        }
        if (phyOperation && phyOperation->purpose == PhyPurpose::IDLE) {
            auto oldId = phyOperation->id;
            phyOperation.reset();
            phy->cancel(oldId);
            phyState = PhyState::OFF;
        }
        if (!superframeStartRequest || !started || !phyAvailable || resetRequest)
            return;
        if (phyOperation || transaction || ackExchange) {
            failSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
            return;
        }
        auto operationId = allocatePhyId();
        phyOperation = PhyOperation{operationId,PhyPurpose::SUPERFRAME_RETUNE_OFF};
        if (phy->submitState(operationId,PhyState::OFF) != PhyAdmission::ACCEPTED) {
            phyOperation.reset();
            failSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
        }
        return;
    }
    if (phyOperation && phyOperation->purpose == PhyPurpose::ACK_READY && ackExchange) {
        auto ackId = phyOperation->id;
        phyOperation.reset();
        phy->cancel(ackId);
        phyState = PhyState::OFF;
        auto reservationId = allocatePhyId();
        phyOperation = PhyOperation{reservationId,PhyPurpose::SUPERFRAME_READY};
        finishAck(false);
        if (!phyOperation || phyOperation->id != reservationId || !ownsBeaconBoundary(boundary) || !started || !phyAvailable || resetRequest)
            return;
        phyOperation.reset();
    }
    if (phyOperation && phyOperation->purpose == PhyPurpose::ACK_TRANSMIT && phyOperation->expectedTransmitEnd &&
            *phyOperation->expectedTransmitEnd <= superframe->getFutureSimulationTime(boundary)) {
        beaconAfterAck = boundary;
        return;
    }
    if (phyOperation && (phyOperation->purpose == PhyPurpose::IDLE ||
            (phyOperation->purpose == PhyPurpose::RX_FOR_ACCESS && transaction &&
             (transaction->phase == Phase::WAIT_RX || transaction->phase == Phase::WAIT_CAP)))) {
        auto id = phyOperation->id;
        phyOperation.reset();
        phy->cancel(id);
        phyState = PhyState::OFF;
        if (!ownsBeaconBoundary(boundary) || !started || !phyAvailable || resetRequest)
            return;
    }
    bool deferredAccess = transaction && (transaction->phase == Phase::WAIT_IFS || transaction->phase == Phase::WAIT_RX ||
            transaction->phase == Phase::WAIT_CAP || transaction->phase == Phase::WAIT_GTS ||
            (transaction->phase == Phase::ACCESS && transaction->slotted && transaction->accessPending));
    if (phyOperation || (transaction && !deferredAccess) || ackExchange || scanRequest || managementRequest) {
        if (firstBoundary && superframeStartRequest) {
            const auto& request = superframeStartRequest->parameters;
            auto geometry = Ieee802154SuperframeTiming::geometry(request.beaconOrder,request.superframeOrder);
            *superframeStartRequest->firstTarget += SIMTIME_AS_CLOCKTIME(SimTime(int64_t(*geometry->beaconInterval) * 16,SIMTIME_US));
            superframeStartRequest->awaitingFirstBoundary = true;
            superframeStartRequest->preparingFirstBeacon = false;
            scheduleNextPeriodicBeacon();
        }
        else
            skipPeriodicBeacon();
        return;
    }
    preparePeriodicBeacon();
}

void Ieee802154StandardMac::handleSuperframeCapEnd()
{
    Enter_Method("handleSuperframeCapEnd");
    auto now = superframe->getCurrentClockTime();
    if (incomingCapEnd && *incomingCapEnd <= now) {
        incomingCapEnd.reset();
        broadcastReceivePending = false;
    }
    if (outgoingCapEnd && *outgoingCapEnd <= now) {
        outgoingCapEnd.reset();
        setOutgoingCapActive(false);
    }
    if (!scheduleCapDutyEnd() && !outgoingCapEnd)
        setOutgoingCapActive(false);
    restoreIdle();
}

bool Ieee802154StandardMac::scheduleCapDutyEnd()
{
    if (!superframe)
        return false;
    superframe->cancelCapEnd();
    if (!started || !phyAvailable || resetRequest || scanRequest)
        return false;
    auto end = incomingCapEnd;
    if (outgoingCapEnd && (!end || *outgoingCapEnd < *end))
        end = outgoingCapEnd;
    if (!end || *end <= superframe->getCurrentClockTime())
        return false;
    if (superframe->scheduleCapEnd(*end))
        return true;
    incomingCapEnd.reset();
    outgoingCapEnd.reset();
    return false;
}

void Ieee802154StandardMac::setOutgoingCapActive(bool active)
{
    if (outgoingCapActive != active) {
        outgoingCapActive = active;
        emit(outgoingCapActiveSignal,active);
    }
}

void Ieee802154StandardMac::processReceivedPacket(Packet *packet, const physicallayer::Ieee802154PhyReceiveIndication& quality,
        const std::optional<Ieee802154SuperframeReceptionAnchor>& anchor, uint64_t receiveEpoch)
{
    emit(packetReceivedFromLowerSignal, packet);
    if (receiveEpoch != receiveRetirementEpoch || !started || !client || packet->hasBitError()) {
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
        if (receiveEpoch != receiveRetirementEpoch || !started || resetRequest || !client)
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
            receiveScanBeacon(frame, quality, anchor);
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
    if (par("enableManagement").boolValue() && coordinator && frame->getFrameType() == 3 && !frame->getSecurityEnabled() &&
            frame->getPayloadArraySize() == 1 && frame->getPayload(0) == 7 && frame->getSourceAddress().isNone() &&
            frame->getDestinationAddress().isShortBroadcast() && frame->getDestinationPanId() == 0xffff && !frame->getAckRequest()) {
        if (getBoolean("macBeaconAutoRespond") && !beaconScheduleActive()) {
            if (beaconResponses == UINT64_MAX)
                throw cRuntimeError("IEEE 802.15.4 beacon response counter exhausted");
            ++beaconResponses;
            restoreIdle();
        }
        else if (managementClient)
            managementClient->handleBeaconRequestIndication({frame->getSourceAddress(),frame->getDestinationPanId()});
        return;
    }
    if (coordinator && parsedCommand && std::holds_alternative<Ieee802154CommandCodec::OrphanNotification>(*parsedCommand) &&
            frame->getSourceAddress().isExtended() && !frame->getSourceAddress().isExtendedGroup() &&
            frame->getDestinationAddress().isShortBroadcast() && frame->getDestinationPanId() == 0xffff && frame->getPanCompression()) {
        if (managementClient)
            managementClient->handleOrphanIndication({frame->getSourceAddress()});
        return;
    }
    std::optional<Ieee802154GtsManager::Grant> observedActivity;
    if (panCoordinator && frame->getFrameType() == 1 && !frame->getSecurityEnabled() &&
            frame->getSourcePanId() == getInteger("macPanId") && frame->getSourceAddress().isShort() &&
            frame->getDestinationPanId() == getInteger("macPanId") &&
            (frame->getDestinationAddress() == std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value) ||
             (frame->getDestinationAddress().isShort() && frame->getDestinationAddress().getShort() == getInteger("macShortAddress"))))
        for (const auto& window : cfpWindows)
            if (window.outgoing && window.key.direction == Ieee802154GtsDirection::DEVICE_TRANSMIT &&
                    window.key.shortAddress == frame->getSourceAddress().getShort() &&
                    quality.timing.ppduStart >= window.physicalStart && quality.timing.ppduEnd <= window.physicalEnd &&
                    gtsManager.noteActivity(window.key))
                observedActivity = *gtsManager.findGrant(window.key);
    if (panCoordinator && frame->getFrameType() == 2 && transaction && transaction->gtsKey &&
            transaction->gtsKey->direction == Ieee802154GtsDirection::DEVICE_RECEIVE &&
            transaction->phase == Phase::WAIT_ACK && frame->getDsn() == transaction->frame->getDsn() &&
            quality.timing.ppduStart >= transaction->transmittedEnd + phy->getTimingParameters().turnaroundTime &&
            quality.timing.ppduEnd <= transaction->ackDeadline)
        for (const auto& window : cfpWindows)
            if (window.outgoing && window.key.panId == transaction->gtsKey->panId &&
                    window.key.extendedAddress == transaction->gtsKey->extendedAddress &&
                    window.key.shortAddress == transaction->gtsKey->shortAddress &&
                    window.key.direction == transaction->gtsKey->direction &&
                    quality.timing.ppduStart >= window.physicalStart && quality.timing.ppduEnd <= window.physicalEnd &&
                    gtsManager.noteActivity(window.key))
                observedActivity = *gtsManager.findGrant(window.key);
    if (observedActivity) {
        emitGtsObservation(gtsActivitySignal,*observedActivity);
        if (receiveEpoch != receiveRetirementEpoch || !started || resetRequest || !client)
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
        if (receiveEpoch != receiveRetirementEpoch || !started || !client)
            return;
        if (orphanScanId && (!scanRequest || scanRequest->id != *orphanScanId))
            return;
    }
    if (frame->getFrameType() == 2) {
        if (transaction && transaction->phase == Phase::WAIT_ACK && frame->getDsn() == transaction->frame->getDsn() &&
                quality.timing.ppduStart >= transaction->transmittedEnd + phy->getTimingParameters().turnaroundTime && quality.timing.ppduEnd <= transaction->ackDeadline) {
            cancelEvent(ackTimer);
            transaction->ackFramePending = frame->getFramePending();
            if (quality.meanPower)
                transaction->ackRssi = receivedRssi(quality);
            eligibleAt = std::max(eligibleAt, quality.timing.ppduEnd + interframeSpacing(transaction->frame->getChunkLength()));
            completeData(Ieee802154MacStatus::SUCCESS);
        }
        return;
    }
    if (par("enableManagement").boolValue() && receiveManagedBeacon(frame,quality,anchor,receiveEpoch))
        return;
    bool dataRequest = false;
    bool associationResponse = false;
    std::optional<uint8_t> associationCapability;
    std::optional<Ieee802154CommandCodec::AssociationResponse> responseContent;
    std::optional<uint8_t> disassociationReason;
    bool panConflict = false;
    std::optional<std::pair<Ieee802154GtsManager::Key,Ieee802154GtsCharacteristics>> gtsCommand;
    bool acceptRealignment = validRealignment && (orphanScanId || (parentAssociated &&
            frame->getSourceAddress() == std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value)));
    if (parsedCommand && !frame->getSourceAddress().isNone()) {
        auto command = &*parsedCommand;
            if (auto gts = std::get_if<Ieee802154CommandCodec::GtsRequest>(command)) {
                if (!panCoordinator || !periodicBeaconing || !superframe || !frame->getSourceAddress().isShort() ||
                        !frame->getDestinationAddress().isNone() || frame->getSourcePanId() != getInteger("macPanId") ||
                        !frame->getAckRequest() || !gts->characteristics.length)
                    return;
                for (const auto& [peer,shortAddress] : associatedDevices)
                    if (peer.first == frame->getSourcePanId() && shortAddress == frame->getSourceAddress().getShort() && peer.second.isExtended()) {
                        gtsCommand = std::make_pair(Ieee802154GtsManager::Key{peer.first,peer.second.getExtended(),shortAddress,gts->characteristics.direction},gts->characteristics);
                        break;
                    }
                if (!gtsCommand)
                    return;
            }
            panConflict = panCoordinator && std::holds_alternative<Ieee802154CommandCodec::PanConflictNotification>(*command) &&
                    frame->getSourceAddress().isExtended() && frame->getDestinationAddress().isExtended() && frame->getPanCompression() &&
                    frame->getSourcePanId() == getInteger("macPanId") &&
                    associatedDevices.count({uint16_t(getInteger("macPanId")),frame->getSourceAddress()});
            dataRequest = coordinator && std::holds_alternative<Ieee802154CommandCodec::DataRequest>(*command);
            associationResponse = std::holds_alternative<Ieee802154CommandCodec::AssociationResponse>(*command) &&
                    frame->getSourceAddress().isExtended() && frame->getDestinationAddress().isExtended() &&
                    frame->getPanCompression() && frame->getAckRequest();
            if (associationResponse)
                responseContent = std::get<Ieee802154CommandCodec::AssociationResponse>(*command);
            if (auto disassociation = std::get_if<Ieee802154CommandCodec::DisassociationNotification>(command)) {
                bool authority = (parentAssociated && frame->getSourceAddress() ==
                        std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value)) ||
                        (coordinator && associatedDevices.count({uint16_t(getInteger("macPanId")),frame->getSourceAddress()}));
                if (authority && frame->getSourceAddress().isExtended() && frame->getPanCompression() &&
                        frame->getDestinationPanId() == getInteger("macPanId") && !frame->getDestinationAddress().isNone() &&
                        !frame->getDestinationAddress().isBroadcast() && !frame->getDestinationAddress().isExtendedGroup() &&
                        (disassociation->reason == 1 || disassociation->reason == 2))
                    disassociationReason = disassociation->reason;
            }
            if (coordinator && std::holds_alternative<Ieee802154CommandCodec::AssociationRequest>(*command) &&
                    frame->getSourceAddress().isExtended() && frame->getSourcePanId() == 0xffff && frame->getAckRequest()) {
                if (!getBoolean("macAssociationPermit"))
                    return; // IEEE Std 802.15.4-2024, 10.21.2: ignore when association is not permitted.
                associationCapability = std::get<Ieee802154CommandCodec::AssociationRequest>(*command).capabilityInformation;
                if (*associationCapability & 0x10)
                    return; // Fast association requires a separately implemented direct-response procedure.
            }
    }
    bool securedCommand = frame->getFrameType() == 3 && frame->getSecurityEnabled() && par("enableManagement").boolValue();
    if (frame->getFrameType() != 1 && !dataRequest && !securedCommand && !associationResponse && !associationCapability && !disassociationReason && !acceptRealignment && !panConflict && !gtsCommand)
        return;
    auto pan = uint16_t(getInteger("macPanId"));
    const auto& destination = frame->getDestinationAddress();
    bool implicitBroadcast = destination.isNone() && getBoolean("macImplicitBroadcast");
    bool accepted;
    if (destination.isNone())
        accepted = implicitBroadcast || (!frame->getSourceAddress().isNone() && coordinator && frame->getSourcePanId() == pan);
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
    received.gtsCommand = gtsCommand;
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
        bool boundParentPan = parentAssociated && pollRequest->request.coordinatorPanId == getInteger("macPanId");
        if (boundParentPan && coordinator.isShort() && coordinator.getShort() == getInteger("macCoordShortAddress"))
            matches |= source == std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value);
        if (boundParentPan && coordinator.isExtended() && coordinator == std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value) &&
                source.isShort() && source.getShort() < 0xfffe)
            matches |= source.getShort() == getInteger("macCoordShortAddress");
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
    else if (!promiscuous && !dataRequest && !associationResponse && !associationCapability && !disassociationReason && !acceptRealignment && !panConflict && !gtsCommand && !(received.receivedPollResponse && !frame->getPayloadArraySize())) {
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
    auto symbolPeriod = phy ? phy->getTimingParameters().symbolPeriod : SimTime(16,SIMTIME_US);
    return Ieee802154ExchangeTiming::interframeSpacing(length,symbolPeriod,getInteger("macSifsPeriod"),getInteger("macLifsPeriod"));
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
    if (exchange.gtsCommand && ackSent)
        decideGts(exchange.gtsCommand->first,exchange.gtsCommand->second);
    if (exchange.associationResponse && associationRequest && exchange.receivedPollResponse && pollRequest && pollRequest->association) {
        associationRequest->response = exchange.associationResponse;
        associationRequest->coordinatorExtendedAddress = exchange.indication.sourceAddress;
    }
    if (exchange.disassociationReason) {
        auto source = exchange.indication.sourceAddress;
        auto pan = uint16_t(getInteger("macPanId"));
        bool leaving = parentAssociated &&
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
    auto boundary = nextBeaconBoundary();
    if (boundary) {
        auto timing = phy->getTimingParameters();
        auto ackDuration = Ieee802154ExchangeTiming::ackDuration(timing);
        if (*boundary <= superframe->getCurrentClockTime() ||
                simTime() + ackDuration > superframe->getFutureSimulationTime(*boundary)) {
            finishAck(false);
            return;
        }
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
    auto spacing = sent && completed.polledDevice && beaconScheduleActive() ?
            phy->getTimingParameters().symbolPeriod * getInteger("macSifsPeriod") : interframeSpacing(completed.receivedLength);
    eligibleAt = std::max(eligibleAt, simTime() + spacing);
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
    else if (transaction->phase == Phase::WAIT_GTS)
        refreshCfp();
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
    if (beaconScheduleActive() && !slottedAccess)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
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
    frame->setSourceAddress(pollRequest->advertisedSource.value_or(shortAddress < 0xfffe ? Ieee802154Address::fromShort(shortAddress) :
            std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value)));
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
    if (!started || !phyAvailable || resetRequest || retiringRequests || (beaconScheduleActive() && !slottedAccess) || transaction || ackExchange || managementRequest || startRequest || scanRequest || beaconPreparation || associationRequest || orphanResponseRequest || pollRequest || phyOperation || !client)
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
        if (poll.device.isBroadcast() || !startQuickPendingResponse(poll.receivedAt))
            startData();
        break;
    }
}

bool Ieee802154StandardMac::startQuickPendingResponse(simtime_t ackEnd)
{
    if (!transaction || transaction->frame->getFrameType() != 1 || !superframe || !slottedAccess || ackEnd != simTime())
        return false;
    auto window = superframe->getAccessWindow();
    if (!window || window->origin != Ieee802154AccessWindowOrigin::OUTGOING)
        return false;
    auto timing = phy->getTimingParameters();
    auto sifs = timing.symbolPeriod * getInteger("macSifsPeriod");
    auto period = SimTime(16,SIMTIME_US) * getInteger("macUnitBackoffPeriod");
    auto earliest = std::max({eligibleAt,ackEnd + sifs,simTime() + timing.turnaroundTime});
    auto now = superframe->getCurrentClockTime();
    auto candidate = superframe->getFutureClockTime(earliest);
    auto unit = SIMTIME_AS_CLOCKTIME(period);
    auto difference = CLOCKTIME_AS_SIMTIME(candidate - window->backoffOrigin).raw();
    auto ticks = period.raw();
    auto count = difference > 0 ? (difference + ticks - 1) / ticks : 0;
    candidate = window->backoffOrigin + unit * count;
    while (candidate < now || superframe->getFutureSimulationTime(candidate) < earliest)
        candidate += unit;
    if (candidate >= window->end)
        return false;
    auto start = superframe->getFutureSimulationTime(candidate);
    if (start > ackEnd + sifs + period)
        return false;
    auto exchange = Ieee802154ExchangeTiming::exchangeDuration(transaction->frame->getChunkLength(),
            transaction->frame->getAckRequest(),SimTime(par("maxPropagationDelay")),getInteger("macSifsPeriod"),getInteger("macLifsPeriod"),timing);
    auto ifs = interframeSpacing(transaction->frame->getChunkLength());
    if (transaction->frame->getAckRequest())
        exchange += Ieee802154ExchangeTiming::slottedAckWait(timing,sifs,period,SimTime(par("maxPropagationDelay"))) -
                Ieee802154ExchangeTiming::ackWait(timing) - 2 * SimTime(par("maxPropagationDelay"));
    if (start + exchange > window->physicalCapEnd || (window->latestCompletion && start + exchange - ifs > *window->latestCompletion))
        return false;
    transaction->slotted = true;
    transaction->accessStartAt = start;
    transaction->phase = Phase::WAIT_TX;
    requestState(PhyState::TRANSMIT,PhyPurpose::TX_READY);
    return true;
}

void Ieee802154StandardMac::completePoll(const Ieee802154MacServiceResult& result)
{
    if (!pollRequest)
        return;
    auto id = pollRequest->id;
    bool association = pollRequest->association;
    bool automatic = pollRequest->automatic;
    pollRequest.reset();
    cancelEvent(pollTimer);
    if (association && associationRequest) {
        if (associationRequest->response)
            completeAssociate(Ieee802154MacStatus::SUCCESS);
        else
            completeAssociate(result == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS) ?
                    Ieee802154MacServiceResult(Ieee802154MacStatus::NO_DATA) : result);
    }
    else if (!association && !automatic && managementClient)
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
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::SUPERFRAME_GET && superframeStartRequest) {
        phyOperation.reset();
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) || !confirm.value ||
                !std::holds_alternative<physicallayer::Ieee802154PhyChannel>(*confirm.value)) {
            failSuperframeStart(confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS) ?
                    Ieee802154MacServiceResult(Ieee802154MacStatus::INVALID_PARAMETER) : macResult(confirm.result));
            return;
        }
        superframeStartRequest->originalChannel = std::get<physicallayer::Ieee802154PhyChannel>(*confirm.value);
        auto operationId = allocatePhyId();
        phyOperation = PhyOperation{operationId,PhyPurpose::SUPERFRAME_CHANNEL};
        if (phy->submitSet(operationId,{"phyCurrentChannelInfo",superframeStartRequest->parameters.channel}) != PhyAdmission::ACCEPTED) {
            phyOperation.reset();
            failSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
        }
        return;
    }
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
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::SYNC_CHANNEL && synchronization && !synchronization->installed) {
        phyOperation.reset();
        // A PHY confirmation proves this replacement was accepted; retire its old CAP before any callback can reenter.
        superframe->clearAccessWindow();
        if (!panCoordinator) clearCfp();
        incomingSuperframe.reset();
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
            retireSynchronization(macResult(confirm.result));
            return;
        }
        superframe->cancelSyncEvents();
        auto operationId = allocatePhyId();
        phyOperation = PhyOperation{operationId,PhyPurpose::SYNC_READY};
        if (phy->submitState(operationId,PhyState::RECEIVE) != PhyAdmission::ACCEPTED) {
            phyOperation.reset();
            retireSynchronization(Ieee802154MacServiceAbortReason::STOPPED);
        }
        return;
    }
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::SUPERFRAME_PHY_RESTORE) {
        phyOperation.reset();
        interruptedSuperframeChannel.reset();
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS))
            retirePeriodicBeaconing(Ieee802154MacServiceAbortReason::STOPPED);
        resumeAvailablePhy();
        return;
    }
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::SCAN_PHY_CCA_RESTORE) {
        phyOperation.reset();
        if (confirm.result == physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
            interruptedScanCcaDuration.reset();
            resumeAvailablePhy();
        }
        return;
    }
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::SUPERFRAME_RESTORE && superframeStartRequest) {
        phyOperation.reset();
        auto requestId = superframeStartRequest->id;
        auto failure = *superframeStartRequest->failure;
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
            periodicBeaconing = false;
            periodicTarget.reset();
            transmittedBeaconAnchor.reset();
            constructedBeacon.reset();
            outgoingEpoch.reset();
            childPhase.reset();
            outgoingCapEnd.reset();
            scheduleCapDutyEnd();
            setOutgoingCapActive(false);
            if (!superframeStartRequest || superframeStartRequest->id != requestId)
                return;
        }
        completeSuperframeStart(failure);
        scheduleNextPeriodicBeacon();
        return;
    }
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::SUPERFRAME_CHANNEL && superframeStartRequest) {
        phyOperation.reset();
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS)) {
            failSuperframeStart(macResult(confirm.result));
            return;
        }
        auto request = superframeStartRequest->parameters;
        if (superframeStartRequest->phase == SuperframeStartRequest::Phase::IMMEDIATE_ACTIVATION) {
            auto state = request.beaconOrder < 15 ? PhyState::TRANSMIT :
                    (getBoolean("macRxOnWhenIdle") ? PhyState::RECEIVE : PhyState::OFF);
            auto operationId = allocatePhyId();
            phyOperation = PhyOperation{operationId,PhyPurpose::SUPERFRAME_ACTIVATION_READY};
            if (phy->submitState(operationId,state) != PhyAdmission::ACCEPTED) {
                phyOperation.reset();
                failSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
            }
        }
        else if (request.beaconOrder == 15) {
            if (!commitSuperframeChannel())
                return;
            auto window = superframe->getAccessWindow();
            if (!window || window->origin != Ieee802154AccessWindowOrigin::INCOMING)
                superframe->clearAccessWindow();
            periodicBeaconing = false;
            periodicTarget.reset();
            transmittedBeaconAnchor.reset();
            constructedBeacon.reset();
            outgoingEpoch.reset();
            childPhase.reset();
            outgoingCapEnd.reset();
            scheduleCapDutyEnd();
            auto requestId = superframeStartRequest->id;
            setOutgoingCapActive(false);
            if (!superframeStartRequest || superframeStartRequest->id != requestId)
                return;
            pib->updateSuperframeConfiguration(request.panId,15,15,request.batteryLifeExtension);
            panCoordinator = request.panCoordinator;
            coordinator = true;
            publishIdentity();
            completeSuperframeStart(Ieee802154MacStatus::SUCCESS);
        }
        else if (!request.panCoordinator && request.startTime && incomingSuperframe) {
            superframeStartRequest->firstTarget = advanceChildBeaconTarget(request,*superframeStartRequest->firstTarget);
            superframeStartRequest->awaitingFirstBoundary = true;
            superframeStartRequest->preparingFirstBeacon = false;
            scheduleNextPeriodicBeacon();
            restoreIdle();
        }
        else
            preparePeriodicBeacon();
        return;
    }
    if (phyOperation && phyOperation->id == id && phyOperation->purpose == PhyPurpose::SCAN_REALIGN_CHANNEL && scanRequest && scanRequest->realignment) {
        phyOperation.reset();
        if (confirm.result != physicallayer::Ieee802154PhyServiceResult(PhyStatus::SUCCESS))
            finishScan(macResult(confirm.result));
        else {
            auto id = scanRequest->id;
            const auto recovered = *scanRequest->realignment;
            bool changedBinding = getInteger("macPanId") != recovered.first.panId ||
                    getInteger("macShortAddress") != recovered.first.shortAddress ||
                    getInteger("macCoordShortAddress") != recovered.first.coordinatorShortAddress ||
                    std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value) != recovered.second ||
                    (scanRequest->originalChannel && scanRequest->originalChannel->channel != recovered.first.channelNumber);
            std::optional<GtsRequestState> retiredGts;
            if (changedBinding) {
                gtsManager.clear();
                retiredGts = std::move(gtsRequest);
                gtsRequest.reset();
                cancelEvent(gtsTimer);
            }
            pib->set("macPanId",uint64_t(recovered.first.panId));
            pib->set("macShortAddress",uint64_t(recovered.first.shortAddress));
            pib->set("macCoordShortAddress",uint64_t(recovered.first.coordinatorShortAddress));
            pib->set("macCoordExtendedAddress",recovered.second);
            parentAssociated = true;
            scanRequest->originalChannel.reset(); // The accepted recovered channel is the new operating configuration.
            auto outerNotifications = syncLossNotificationsActive;
            syncLossNotificationsActive = true;
            publishIdentity();
            if (retiredGts && managementClient)
                managementClient->handleGtsConfirm(retiredGts->id,{retiredGts->request.characteristics,Ieee802154MacServiceAbortReason::DISASSOCIATED});
            syncLossNotificationsActive = outerNotifications;
            if (!outerNotifications && resetRequest && resetRequest->deferredResult) {
                auto result = *resetRequest->deferredResult;
                finishReset(result);
            }
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
            bool acquired = synchronization && synchronization->installed && synchronization->anchored && incomingSuperframe;
            if (request.coordinatorAddress.isShort())
                pib->set("macCoordShortAddress",uint64_t(request.coordinatorAddress.getShort()));
            else if (!acquired)
                pib->set("macCoordShortAddress",uint64_t(0xffff));
            if (request.coordinatorAddress.isExtended())
                pib->set("macCoordExtendedAddress",request.coordinatorAddress);
            else if (!acquired)
                pib->set("macCoordExtendedAddress",Ieee802154Address());
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
            retireParentContext(true);
            if (getInteger("macPanId") != startRequest->parameters.panId)
                associatedDevices.clear();
            pib->set("macPanId", uint64_t(startRequest->parameters.panId));
            panCoordinator = true;
            coordinator = true;
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
    if (beaconScheduleActive())
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
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

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitSync(uint64_t id, const Ieee802154MacSyncRequest& request)
{
    Enter_Method("submitSync");
    auto admission = admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!superframe || !managementClient || !phy || periodicBeaconing)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if ((phyOperation && phyOperation->purpose != PhyPurpose::IDLE) || !pendingOriginals.empty())
        return Ieee802154MacServiceAdmission::BUSY;
    if (getInteger("macPanId") == 0xffff || getInteger("macBeaconOrder") == 15 ||
            request.channel.phy != physicallayer::Ieee802154PhyChannel::Phy::O_QPSK ||
            request.channel.band != physicallayer::Ieee802154PhyChannel::Band::MHZ_2450 ||
            request.channel.channel < 11 || request.channel.channel > 26) {
        managementClient->handleSyncConfirm(id,{Ieee802154MacStatus::INVALID_PARAMETER});
        return admission;
    }
    if (phyOperation) {
        auto oldId = phyOperation->id;
        phyOperation.reset();
        phy->cancel(oldId);
        phyState = PhyState::OFF;
        if (!started || !phyAvailable || resetRequest || retiringRequests)
            return Ieee802154MacServiceAdmission::UNAVAILABLE;
    }
    auto previous = std::move(synchronization);
    synchronization = Synchronization{id,request};
    synchronization->beaconOrder = getInteger("macBeaconOrder");
    auto operationId = allocatePhyId();
    phyOperation = PhyOperation{operationId,PhyPurpose::SYNC_CHANNEL};
    auto phyAdmission = phy->submitSet(operationId,{"phyCurrentChannelInfo",request.channel});
    if (phyAdmission != PhyAdmission::ACCEPTED) {
        phyOperation.reset();
        synchronization = std::move(previous);
        restoreIdle();
        return macAdmission(phyAdmission);
    }
    retireReceives(); // A synchronous replacement retires any beacon already in callback dispatch.
    if (synchronization && synchronization->id == id && !synchronization->installed) {
        superframe->clearAccessWindow();
        superframe->cancelSyncEvents();
    }
    return admission;
}

void Ieee802154StandardMac::completeSyncInstallation(const Ieee802154MacServiceResult& result)
{
    if (synchronization && managementClient)
        managementClient->handleSyncConfirm(synchronization->id,{result});
}

void Ieee802154StandardMac::retireSynchronization(const Ieee802154MacServiceResult& pendingResult)
{
    if (!synchronization)
        return;
    auto old = *synchronization;
    synchronization.reset();
    if (superframe)
        superframe->cancelSyncEvents();
    if (!old.installed && managementClient)
        managementClient->handleSyncConfirm(old.id,{pendingResult});
}

void Ieee802154StandardMac::scheduleAcquisitionWindow()
{
    auto symbols = int64_t(960) * ((int64_t(1) << synchronization->beaconOrder) + 1);
    auto now = superframe->getCurrentClockTime();
    if (synchronization->misses == 0)
        synchronization->windowStart = now;
    synchronization->deadline = now + SIMTIME_AS_CLOCKTIME(SimTime(symbols * 16,SIMTIME_US));
    synchronization->physicalDeadline = superframe->getFutureSimulationTime(synchronization->deadline);
    superframe->scheduleSyncDeadline(synchronization->deadline);
}

void Ieee802154StandardMac::scheduleTrackingWindow()
{
    auto& sync = *synchronization;
    auto interval = SIMTIME_AS_CLOCKTIME(SimTime((int64_t(960) << sync.beaconOrder) * 16,SIMTIME_US));
    auto elapsed = interval * (sync.misses + 1);
    auto elapsedTicks = CLOCKTIME_AS_SIMTIME(elapsed).raw();
    auto symbolTicks = SimTime(16,SIMTIME_US).raw();
    auto relativeGuard = [](int64_t ticks) { return (ticks / 1000000) * 81 + ((ticks % 1000000) * 81 + 999999) / 1000000; };
    auto roundSymbols = [symbolTicks](int64_t ticks) { return ((ticks + symbolTicks - 1) / symbolTicks) * symbolTicks; };
    auto quantizationAndPropagation = 2 * symbolTicks + SimTime(par("maxPropagationDelay")).raw() + 2;
    auto uncertaintyTicks = roundSymbols(relativeGuard(elapsedTicks) + quantizationAndPropagation + relativeGuard(quantizationAndPropagation));
    auto uncertainty = SimTime::fromRaw(uncertaintyTicks);
    auto expected = sync.anchor + elapsed;
    auto earliest = expected - SIMTIME_AS_CLOCKTIME(uncertainty);
    sync.windowStart = earliest;
    auto timing = phy->getTimingParameters();
    auto maximumPpdu = timing.shrDuration + timing.phrDuration + SimTime(timing.maxPsduLength.get<units::values::B>() * 8.0 / timing.bitrate.get<units::values::bps>());
    auto maximumTicks = maximumPpdu.raw();
    auto completionAllowance = SimTime::fromRaw(roundSymbols(maximumTicks + relativeGuard(maximumTicks)));
    auto readinessTicks = timing.turnaroundTime.raw();
    auto readinessAllowance = SimTime::fromRaw(roundSymbols(readinessTicks + relativeGuard(readinessTicks)));
    auto wake = earliest - SIMTIME_AS_CLOCKTIME(readinessAllowance);
    sync.deadline = expected + SIMTIME_AS_CLOCKTIME(uncertainty + completionAllowance);
    sync.wantsRx = false;
    auto now = superframe->getCurrentClockTime();
    if (wake > now)
        superframe->scheduleSyncWake(wake);
    else
        sync.wantsRx = true;
    if (sync.deadline > now) {
        sync.physicalDeadline = superframe->getFutureSimulationTime(sync.deadline);
        superframe->scheduleSyncDeadline(sync.deadline);
    }
    else {
        handleSyncDeadline();
        return;
    }
}

void Ieee802154StandardMac::handleSyncWake()
{
    Enter_Method("handleSyncWake");
    if (!synchronization || !synchronization->installed)
        return;
    synchronization->wantsRx = true;
    restoreIdle();
}

void Ieee802154StandardMac::handleSyncDeadline()
{
    Enter_Method("handleSyncDeadline");
    if (!synchronization || !synchronization->installed)
        return;
    auto id = synchronization->id;
    auto misses = ++synchronization->misses;
    if (!panCoordinator)
        clearCfp();
    if (misses >= 4) {
        auto channel = synchronization->request.channel;
        auto pan = uint16_t(getInteger("macPanId"));
        auto coordinator = std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value);
        if (coordinator.isNone() && getInteger("macCoordShortAddress") < 0xfffe)
            coordinator = Ieee802154Address::fromShort(getInteger("macCoordShortAddress"));
        auto outerNotifications = syncLossNotificationsActive;
        syncLossNotificationsActive = true;
        retireSynchronization(Ieee802154MacServiceAbortReason::STOPPED);
        if (transaction && transaction->gtsKey) {
            retireDataRadio();
            completeData(Ieee802154MacServiceAbortReason::STOPPED);
        }
        if (gtsRequest) {
            auto outerGtsRetirement = retiringRequests;
            retiringRequests = true;
            if (transaction && transaction->kind == Transaction::Kind::GTS_REQUEST) {
                retireDataRadio();
                completeData(Ieee802154MacServiceAbortReason::STOPPED);
            }
            if (gtsRequest)
                completeGts(Ieee802154MacServiceAbortReason::STOPPED);
            retiringRequests = outerGtsRetirement;
        }
        gtsManager.clear();
        auto dependentStart = superframeStartRequest && (childPhase || (!superframeStartRequest->parameters.panCoordinator &&
                superframeStartRequest->parameters.beaconOrder < 15 && superframeStartRequest->parameters.startTime)) ?
                superframeStartRequest->id : IEEE802154_INVALID_REQUEST_ID;
        bool ownedCommand = dependentStart && transaction && transaction->id == dependentStart &&
                transaction->kind == Transaction::Kind::SUPERFRAME_REALIGN;
        bool ownedPreparation = dependentStart && phyOperation &&
                (phyOperation->purpose == PhyPurpose::SUPERFRAME_GET || phyOperation->purpose == PhyPurpose::SUPERFRAME_CHANNEL ||
                 phyOperation->purpose == PhyPurpose::SUPERFRAME_READY || phyOperation->purpose == PhyPurpose::SUPERFRAME_ACTIVATION_READY ||
                 phyOperation->purpose == PhyPurpose::SUPERFRAME_RETUNE_OFF || phyOperation->purpose == PhyPurpose::SUPERFRAME_TRANSMIT);
        auto outerRetirement = retiringRequests;
        if (ownedCommand) {
            retiringRequests = true;
            superframeStartRequest.reset(); // Detach both obligations before any START callback.
        }
        if (ownedCommand || ownedPreparation)
            retireDataRadio();
        if (ownedCommand && transaction && transaction->id == dependentStart && transaction->kind == Transaction::Kind::SUPERFRAME_REALIGN)
            completeData(Ieee802154MacServiceAbortReason::STOPPED);
        if (childPhase)
            retirePeriodicBeaconing(Ieee802154MacServiceAbortReason::STOPPED);
        if (ownedCommand) {
            retiringRequests = outerRetirement;
            if (managementClient)
                managementClient->handleStartSuperframeConfirm(dependentStart,{Ieee802154MacServiceAbortReason::STOPPED});
        }
        if (dependentStart && superframeStartRequest && superframeStartRequest->id == dependentStart) {
            superframe->cancelBeaconPreparation();
            completeSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
        }
        if (started && managementClient)
            managementClient->handleSyncLossIndication({Ieee802154SyncLossReason::BEACON_LOST,pan,channel,coordinator,{},{}});
        emit(syncMissedSignal,long(misses));
        emit(syncLostSignal,1L);
        syncLossNotificationsActive = outerNotifications;
        if (!outerNotifications && resetRequest && resetRequest->deferredResult) {
            auto result = *resetRequest->deferredResult;
            finishReset(result);
        }
        if (!synchronization && transaction && (transaction->phase == Phase::WAIT_CAP ||
                (transaction->phase == Phase::ACCESS && transaction->slotted && transaction->accessPending))) {
            auto accessRequest = transaction->accessId;
            bool cancelAccess = transaction->slotted && transaction->accessPending;
            transaction->phase = Phase::WAIT_CAP;
            transaction->accessPending = false;
            if (cancelAccess)
                slottedAccess->cancel(accessRequest, Ieee802154MacServiceAbortReason::CANCELLED);
            if (transaction && transaction->accessId == accessRequest && started && phyAvailable && !resetRequest)
                startData();
        }
        restoreIdle();
        return;
    }
    emit(syncMissedSignal,long(misses));
    if (!synchronization || synchronization->id != id || !synchronization->installed)
        return;
    if (synchronization->anchored) {
        scheduleTrackingWindow();
        restoreIdle();
    }
    else {
        synchronization->wantsRx = true;
        scheduleAcquisitionWindow();
        restoreIdle();
    }
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitStartSuperframe(uint64_t id, const Ieee802154MacStartSuperframeRequest& request)
{
    Enter_Method("submitStartSuperframe");
    auto admission = admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!superframe || !managementClient || !phy || (request.coordinatorRealignment && !access))
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if (phyOperation || !pendingOriginals.empty())
        return Ieee802154MacServiceAdmission::BUSY;
    superframeStartRequest = SuperframeStartRequest{id,request};
    if (!Ieee802154SuperframeTiming::geometry(request.beaconOrder,request.superframeOrder) || request.startTime > 0xffffff ||
            request.beaconSecurity.securityLevel > 7 || request.realignmentSecurity.securityLevel > 7) {
        completeSuperframeStart(Ieee802154MacStatus::INVALID_PARAMETER);
        return admission;
    }
    if (request.beaconSecurity.securityLevel || request.realignmentSecurity.securityLevel) {
        completeSuperframeStart(Ieee802154MacStatus::UNSUPPORTED_SECURITY);
        return admission;
    }
    if (request.channel.phy != physicallayer::Ieee802154PhyChannel::Phy::O_QPSK ||
            request.channel.band != physicallayer::Ieee802154PhyChannel::Band::MHZ_2450 ||
            request.channel.channel < 11 || request.channel.channel > 26) {
        completeSuperframeStart(Ieee802154MacStatus::BAD_CHANNEL);
        return admission;
    }
    if (getInteger("macShortAddress") == 0xffff) {
        completeSuperframeStart(Ieee802154MacStatus::NO_SHORT_ADDRESS);
        return admission;
    }
    if (!request.panCoordinator && !parentAssociated) {
        completeSuperframeStart(Ieee802154MacStatus::INVALID_PARAMETER);
        return admission;
    }
    if (!request.panCoordinator && request.beaconOrder < 15 && incomingSuperframe &&
            (request.beaconOrder != incomingSuperframe->beaconOrder || request.superframeOrder != incomingSuperframe->superframeOrder)) {
        completeSuperframeStart(Ieee802154MacStatus::INVALID_PARAMETER);
        return admission;
    }
    if (!request.panCoordinator && request.beaconOrder < 15 && request.startTime &&
            (!synchronization || !synchronization->installed || !synchronization->request.trackBeacon || !incomingSuperframe)) {
        completeSuperframeStart(Ieee802154MacStatus::TRACKING_OFF);
        return admission;
    }
    if (!request.panCoordinator && request.beaconOrder < 15 && request.startTime &&
            !(synchronization->request.channel == request.channel)) {
        completeSuperframeStart(Ieee802154MacStatus::INVALID_PARAMETER);
        return admission;
    }
    if (!request.panCoordinator && request.beaconOrder < 15 && request.startTime && incomingSuperframe) {
        auto target = firstChildBeaconTarget(request);
        auto candidate = makeBeacon(request.panId,request.beaconOrder,request.superframeOrder,request.batteryLifeExtension,false);
        if (!candidate) {
            completeSuperframeStart(Ieee802154MacStatus::FRAME_TOO_LONG);
            return admission;
        }
        if (overlapsIncomingSuperframe(target,request.beaconOrder,request.superframeOrder,candidate->getChunkLength())) {
            completeSuperframeStart(Ieee802154MacStatus::SUPERFRAME_OVERLAP);
            return admission;
        }
        superframeStartRequest->firstTarget = target;
    }
    if (request.coordinatorRealignment) {
        if (periodicBeaconing) {
            superframeStartRequest->phase = SuperframeStartRequest::Phase::WAIT_OLD_BEACON;
            scheduleNextPeriodicBeacon();
        }
        else {
            superframeStartRequest->phase = SuperframeStartRequest::Phase::COMMAND;
            transmitSuperframeRealignment();
        }
        return admission;
    }
    superframe->cancelBeaconPreparation();
    auto internalId = allocatePhyId();
    phyOperation = PhyOperation{internalId,PhyPurpose::SUPERFRAME_GET};
    auto phyAdmission = phy->submitGet(internalId,{"phyCurrentChannelInfo"});
    if (phyAdmission != PhyAdmission::ACCEPTED) {
        phyOperation.reset();
        superframeStartRequest.reset();
        scheduleNextPeriodicBeacon();
        return macAdmission(phyAdmission);
    }
    return admission;
}

void Ieee802154StandardMac::completeSuperframeStart(const Ieee802154MacServiceResult& result)
{
    if (!superframeStartRequest)
        return;
    constructedGtsCandidate.reset();
    auto id = superframeStartRequest->id;
    bool awaitingBoundary = superframeStartRequest->awaitingFirstBoundary;
    superframeStartRequest.reset();
    if (awaitingBoundary) {
        superframe->cancelBeaconPreparation();
        scheduleNextPeriodicBeacon();
    }
    if (managementClient)
        managementClient->handleStartSuperframeConfirm(id,{result});
    if (panCoordinator && periodicBeaconing && outgoingEpoch && started && !resetRequest)
        maintainGtsCapacity(outgoingEpoch->superframeOrder);
    restoreIdle();
}

bool Ieee802154StandardMac::commitSuperframeChannel()
{
    if (!superframeStartRequest)
        return false;
    auto requestId = superframeStartRequest->id;
    auto request = superframeStartRequest->parameters;
    bool channelChanged = superframeStartRequest->originalChannel && !(*superframeStartRequest->originalChannel == request.channel);
    bool promoted = request.panCoordinator && (!panCoordinator || parentAssociated);
    if (channelChanged || promoted)
        retireParentContext(promoted);
    if (!superframeStartRequest || superframeStartRequest->id != requestId || !started || !phyAvailable || resetRequest)
        return false;
    if (getInteger("macPanId") != request.panId)
        associatedDevices.clear();
    return true;
}

void Ieee802154StandardMac::retireParentContext(bool clearAssociation)
{
    if (!panCoordinator)
        clearCfp();
    if (superframe)
        superframe->clearAccessWindow();
    incomingSuperframe.reset();
    incomingCapEnd.reset();
    auto receiveEpoch = receiveRetirementEpoch;
    if (!scheduleCapDutyEnd())
        setOutgoingCapActive(false);
    if (receiveEpoch != receiveRetirementEpoch)
        return;
    childPhase.reset();
    retireSynchronization(Ieee802154MacServiceAbortReason::STOPPED);
    retireReceives();
    if (!panCoordinator) {
        gtsManager.clear();
        auto epoch = receiveRetirementEpoch;
        auto outerRetirement = retiringRequests;
        retiringRequests = true;
        if (transaction && (transaction->kind == Transaction::Kind::GTS_REQUEST || transaction->gtsKey)) {
            retireDataRadio();
            completeData(Ieee802154MacServiceAbortReason::STOPPED);
        }
        if (gtsRequest)
            completeGts(Ieee802154MacServiceAbortReason::STOPPED);
        retiringRequests = outerRetirement;
        if (epoch != receiveRetirementEpoch)
            return;
    }
    if (clearAssociation) {
        pendingPanConflict.reset();
        broadcastReceivePending = false;
        autoPollPending.reset();
        pib->clearParentAssociation();
        parentAssociated = false;
    }
}

void Ieee802154StandardMac::retirePeriodicBeaconing(const Ieee802154MacServiceResult& result)
{
    if (panCoordinator)
        clearCfp();
    auto requestId = superframeStartRequest ? superframeStartRequest->id : 0;
    bool ownedCommand = requestId && transaction && transaction->id == requestId &&
            transaction->kind == Transaction::Kind::SUPERFRAME_REALIGN;
    auto outerRetirement = retiringRequests;
    if (ownedCommand) {
        retiringRequests = true;
        superframeStartRequest.reset();
        retireDataRadio();
        completeData(result);
    }
    if (superframe && periodicBeaconing) {
        auto window = superframe->getAccessWindow();
        if (!window || window->origin != Ieee802154AccessWindowOrigin::INCOMING)
            superframe->clearAccessWindow();
    }
    if (superframe)
        superframe->cancelBeaconPreparation();
    periodicBeaconing = false;
    periodicTarget.reset();
    beaconAfterAck.reset();
    transmittedBeaconAnchor.reset();
    constructedBeacon.reset();
    outgoingEpoch.reset();
    childPhase.reset();
    outgoingCapEnd.reset();
    scheduleCapDutyEnd();
    if (!interruptedScanCcaDuration)
        interruptedSuperframeChannel.reset();
    setOutgoingCapActive(false);
    if (ownedCommand) {
        retiringRequests = outerRetirement;
        if (managementClient)
            managementClient->handleStartSuperframeConfirm(requestId,{result});
    }
    else if (superframeStartRequest && superframeStartRequest->id == requestId)
        completeSuperframeStart(result);
}

void Ieee802154StandardMac::failSuperframeStart(const Ieee802154MacServiceResult& result)
{
    if (!superframeStartRequest)
        return;
    auto requestId = superframeStartRequest->id;
    if (superframeStartRequest->originalChannel && phyAvailable && started && !resetRequest) {
        superframeStartRequest->failure = result;
        auto id = allocatePhyId();
        phyOperation = PhyOperation{id,PhyPurpose::SUPERFRAME_RESTORE};
        if (phy->submitSet(id,{"phyCurrentChannelInfo",*superframeStartRequest->originalChannel}) == PhyAdmission::ACCEPTED)
            return;
        phyOperation.reset();
        periodicBeaconing = false;
        periodicTarget.reset();
        transmittedBeaconAnchor.reset();
        constructedBeacon.reset();
        outgoingEpoch.reset();
        outgoingCapEnd.reset();
        scheduleCapDutyEnd();
        setOutgoingCapActive(false);
        if (!superframeStartRequest || superframeStartRequest->id != requestId)
            return;
    }
    completeSuperframeStart(result);
    scheduleNextPeriodicBeacon();
}

clocktime_t Ieee802154StandardMac::firstChildBeaconTarget(const Ieee802154MacStartSuperframeRequest& request) const
{
    auto offset = Ieee802154SuperframeTiming::roundStartTime(request.startTime,getInteger("macUnitBackoffPeriod"));
    auto target = incomingSuperframe->anchor + SIMTIME_AS_CLOCKTIME(SimTime(int64_t(offset) * 16,SIMTIME_US));
    return advanceChildBeaconTarget(request,target);
}

clocktime_t Ieee802154StandardMac::advanceChildBeaconTarget(const Ieee802154MacStartSuperframeRequest& request, clocktime_t target) const
{
    auto geometry = Ieee802154SuperframeTiming::geometry(request.beaconOrder,request.superframeOrder);
    auto interval = SIMTIME_AS_CLOCKTIME(SimTime(int64_t(*geometry->beaconInterval) * 16,SIMTIME_US));
    auto earliest = superframe->getFutureClockTime(simTime() + phy->getTimingParameters().turnaroundTime);
    if (target < earliest) {
        auto delta = CLOCKTIME_AS_SIMTIME(earliest - target).raw();
        auto ticks = CLOCKTIME_AS_SIMTIME(interval).raw();
        target += interval * ((delta + ticks - 1) / ticks);
    }
    return target;
}

std::optional<clocktime_t> Ieee802154StandardMac::nextBeaconBoundary() const
{
    std::optional<clocktime_t> target = periodicBeaconing ? periodicTarget : std::nullopt;
    if (superframeStartRequest && superframeStartRequest->phase == SuperframeStartRequest::Phase::INITIAL &&
            superframeStartRequest->firstTarget && (!target || *superframeStartRequest->firstTarget < *target))
        target = superframeStartRequest->firstTarget;
    return target;
}

bool Ieee802154StandardMac::ownsBeaconBoundary(clocktime_t boundary) const
{
    return (periodicBeaconing && periodicTarget && *periodicTarget == boundary) ||
            (superframeStartRequest && superframeStartRequest->phase == SuperframeStartRequest::Phase::INITIAL &&
             superframeStartRequest->firstTarget && *superframeStartRequest->firstTarget == boundary);
}

void Ieee802154StandardMac::scheduleNextPeriodicBeacon()
{
    if (!superframe || !started || !phyAvailable || scanRequest ||
            (superframeStartRequest && superframeStartRequest->phase == SuperframeStartRequest::Phase::INITIAL && !superframeStartRequest->awaitingFirstBoundary))
        return;
    if (!settleSkippedGtsOpportunities())
        return;
    auto readinessLead = phy->getTimingParameters().turnaroundTime;
    auto earliest = superframe->getFutureClockTime(simTime() + readinessLead);
    auto advance = [&](clocktime_t& target, uint8_t bo, uint8_t so) {
        if (target < earliest) {
            auto geometry = Ieee802154SuperframeTiming::geometry(bo,so);
            auto interval = SIMTIME_AS_CLOCKTIME(SimTime(int64_t(*geometry->beaconInterval) * 16,SIMTIME_US));
            auto delta = CLOCKTIME_AS_SIMTIME(earliest - target).raw();
            auto ticks = CLOCKTIME_AS_SIMTIME(interval).raw();
            auto missed = (delta + ticks - 1) / ticks;
            if (&target == (periodicTarget ? &*periodicTarget : nullptr)) {
                int64_t first = 0;
                if (lastGtsAgedBoundary && *lastGtsAgedBoundary >= target)
                    first = CLOCKTIME_AS_SIMTIME(*lastGtsAgedBoundary - target).raw() / ticks + 1;
                for (int64_t i = first; i < missed && i - first < 4; ++i)
                    ageGtsEpoch(target + interval * i);
                auto last = target + interval * (missed - 1);
                if (!lastGtsAgedBoundary || last > *lastGtsAgedBoundary)
                    lastGtsAgedBoundary = last;
            }
            target += interval * missed;
        }
    };
    if (periodicBeaconing && periodicTarget && outgoingEpoch)
        advance(*periodicTarget,outgoingEpoch->beaconOrder,outgoingEpoch->superframeOrder);
    if (superframeStartRequest && superframeStartRequest->awaitingFirstBoundary && superframeStartRequest->firstTarget) {
        const auto& request = superframeStartRequest->parameters;
        advance(*superframeStartRequest->firstTarget,request.beaconOrder,request.superframeOrder);
    }
    auto target = nextBeaconBoundary();
    if (target && !superframe->scheduleBeaconPreparation(*target,readinessLead)) {
        if (superframeStartRequest && superframeStartRequest->awaitingFirstBoundary && superframeStartRequest->firstTarget &&
                *superframeStartRequest->firstTarget == *target) {
            const auto& request = superframeStartRequest->parameters;
            auto geometry = Ieee802154SuperframeTiming::geometry(request.beaconOrder,request.superframeOrder);
            *superframeStartRequest->firstTarget += SIMTIME_AS_CLOCKTIME(SimTime(int64_t(*geometry->beaconInterval) * 16,SIMTIME_US));
        }
        if (periodicTarget && outgoingEpoch && *periodicTarget == *target) {
            auto geometry = Ieee802154SuperframeTiming::geometry(outgoingEpoch->beaconOrder,outgoingEpoch->superframeOrder);
            *periodicTarget += SIMTIME_AS_CLOCKTIME(SimTime(int64_t(*geometry->beaconInterval) * 16,SIMTIME_US));
        }
        auto next = nextBeaconBoundary();
        if (next && !superframe->scheduleBeaconPreparation(*next,readinessLead) &&
                superframeStartRequest && superframeStartRequest->awaitingFirstBoundary) {
            superframeStartRequest->awaitingFirstBoundary = false;
            completeSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
        }
    }
}

void Ieee802154StandardMac::skipPeriodicBeacon()
{
    if (!periodicBeaconing || !periodicTarget)
        return;
    auto geometry = Ieee802154SuperframeTiming::geometry(outgoingEpoch->beaconOrder,outgoingEpoch->superframeOrder);
    *periodicTarget += SIMTIME_AS_CLOCKTIME(SimTime(int64_t(*geometry->beaconInterval) * 16,SIMTIME_US));
    scheduleNextPeriodicBeacon();
}

void Ieee802154StandardMac::preparePeriodicBeacon()
{
    auto id = allocatePhyId();
    phyOperation = PhyOperation{id,PhyPurpose::SUPERFRAME_READY};
    auto admission = phy->submitState(id,PhyState::TRANSMIT);
    if (admission != PhyAdmission::ACCEPTED) {
        phyOperation.reset();
        if (superframeStartRequest && !superframeStartRequest->awaitingFirstBoundary)
            failSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
        else
            skipPeriodicBeacon();
    }
}

void Ieee802154StandardMac::transmitSuperframeRealignment()
{
    auto frame = makeShared<Ieee802154Frame>();
    frame->setFrameType(3);
    frame->setFrameVersion(1);
    frame->setDsn(getInteger("macDsn"));
    frame->setSourceAddress(std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value));
    frame->setSourcePanId(getInteger("macPanId"));
    frame->setDestinationAddress(Ieee802154Address::SHORT_BROADCAST_ADDRESS);
    frame->setDestinationPanId(0xffff);
    frame->setAckRequest(false);
    const auto& request = superframeStartRequest->parameters;
    auto bytes = Ieee802154CommandCodec::encode(Ieee802154CommandCodec::CoordinatorRealignment{
            request.panId,uint16_t(getInteger("macShortAddress")),uint8_t(request.channel.channel),0xffff});
    frame->setPayloadArraySize(bytes.size());
    for (size_t i = 0; i < bytes.size(); ++i)
        frame->setPayload(i,bytes[i]);
    Ieee802154FrameCodec::finalize(*frame);
    pib->set("macDsn",uint64_t((frame->getDsn() + 1) & 255));
    transaction = Transaction{superframeStartRequest->id,nullptr,0,frame};
    transaction->kind = Transaction::Kind::SUPERFRAME_REALIGN;
    startData();
}

void Ieee802154StandardMac::sendPeriodicBeacon()
{
    auto initial = superframeStartRequest && ((superframeStartRequest->phase == SuperframeStartRequest::Phase::INITIAL && superframeStartRequest->preparingFirstBeacon) ||
            superframeStartRequest->phase == SuperframeStartRequest::Phase::WAIT_ACTIVATION);
    auto panId = initial ? superframeStartRequest->parameters.panId : getInteger("macPanId");
    auto bo = initial ? superframeStartRequest->parameters.beaconOrder : getInteger("macBeaconOrder");
    auto so = initial ? superframeStartRequest->parameters.superframeOrder : getInteger("macSuperframeOrder");
    auto ble = initial ? superframeStartRequest->parameters.batteryLifeExtension : getBoolean("macBattLifeExt");
    if (!initial && panCoordinator && periodicBeaconing && !gtsManager.getGrants().empty()) {
        auto target = periodicTarget;
        auto epoch = receiveRetirementEpoch;
        auto reservationId = allocatePhyId();
        phyOperation = PhyOperation{reservationId,PhyPurpose::SUPERFRAME_READY};
        maintainGtsCapacity(so);
        if (!started || !phyAvailable || resetRequest || !phyOperation || phyOperation->id != reservationId || !periodicBeaconing || periodicTarget != target || epoch != receiveRetirementEpoch) {
            if (phyOperation && phyOperation->id == reservationId) {
                phyOperation.reset();
                restoreIdle();
            }
            return;
        }
        phyOperation.reset();
        panId = getInteger("macPanId");
        bo = getInteger("macBeaconOrder");
        so = getInteger("macSuperframeOrder");
        ble = getBoolean("macBattLifeExt");
    }
    constructedGtsCandidate.reset();
    if (initial && superframeStartRequest->parameters.panCoordinator &&
            (!gtsManager.getGrants().empty() || !gtsManager.getAdvertisements().empty())) {
        GtsCandidate candidate{gtsManager,{}};
        bool replacesPan = panId != getInteger("macPanId") || (!panCoordinator && parentAssociated);
        bool immediateReplacement = periodicBeaconing && superframeStartRequest->phase == SuperframeStartRequest::Phase::INITIAL &&
                !superframeStartRequest->firstTarget && transmittedBeaconAnchor.has_value();
        if (immediateReplacement) {
            candidate.ledger.ageAdvertisements();
            candidate.agesImmediateEpoch = true;
        }
        if (replacesPan) {
            for (const auto& grant : candidate.ledger.getGrants()) candidate.removed.push_back(grant);
            candidate.ledger.clear();
        }
        while (!candidate.ledger.getGrants().empty()) {
            auto ordinaryLedger = candidate.ledger;
            ordinaryLedger.clearAdvertisements();
            auto preview = makeBeacon(panId,bo,so,ble,false,&ordinaryLedger);
            if (!preview)
                break;
            auto ordinary = Ieee802154ExchangeTiming::ppduDuration(preview->getChunkLength(),phy->getTimingParameters());
            unsigned int used = 0;
            for (const auto& grant : candidate.ledger.getGrants()) used += grant.length;
            auto target = superframeStartRequest->firstTarget ? superframeStartRequest->firstTarget : periodicTarget;
            bool scheduled = target && (superframeStartRequest->phase == SuperframeStartRequest::Phase::WAIT_ACTIVATION ||
                    (!superframeStartRequest->parameters.panCoordinator && superframeStartRequest->parameters.startTime));
            if (gtsPhysicalCapDuration(so,16 - used,!scheduled,scheduled ? target : std::nullopt) - ordinary >= SimTime(440 * 16,SIMTIME_US))
                break;
            auto grant = candidate.ledger.getGrants().back();
            removeGtsFromLedger(candidate.ledger,grant.key);
            candidate.removed.push_back(grant);
        }
        if (replacesPan || immediateReplacement || !candidate.removed.empty())
            constructedGtsCandidate = std::move(candidate);
    }
    auto ledger = constructedGtsCandidate ? &constructedGtsCandidate->ledger : &gtsManager;
    auto frame = makeBeacon(panId,bo,so,ble,false,ledger);
    if (!frame) {
        if (initial)
            failSuperframeStart(Ieee802154MacStatus::FRAME_TOO_LONG);
        else
            skipPeriodicBeacon();
        return;
    }
    bool advertisedBroadcast = frame->getFramePending();
    auto target = initial && superframeStartRequest->firstTarget ? superframeStartRequest->firstTarget : periodicTarget;
    auto geometryTarget = target;
    if (initial && !superframeStartRequest->parameters.panCoordinator && !superframeStartRequest->parameters.startTime)
        geometryTarget = superframe->getCurrentClockTime();
    if (incomingSuperframe && geometryTarget &&
            (childPhase || (initial && !superframeStartRequest->parameters.panCoordinator)) &&
            overlapsIncomingSuperframe(*geometryTarget,bo,so,frame->getChunkLength())) {
        if (initial)
            failSuperframeStart(Ieee802154MacStatus::SUPERFRAME_OVERLAP);
        else {
            auto channel = synchronization ? synchronization->request.channel : physicallayer::Ieee802154PhyChannel{};
            auto pan = uint16_t(getInteger("macPanId"));
            auto address = std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value);
            auto epoch = receiveRetirementEpoch;
            retirePeriodicBeaconing(Ieee802154MacServiceAbortReason::STOPPED);
            if (epoch == receiveRetirementEpoch && managementClient)
                managementClient->handleSyncLossIndication({Ieee802154SyncLossReason::SUPERFRAME_OVERLAP,pan,channel,address,{},{}});
            restoreIdle();
        }
        return;
    }
    pib->set("macBsn",uint64_t((frame->getDsn() + 1) & 255));
    if (superframeStartRequest && superframeStartRequest->phase == SuperframeStartRequest::Phase::WAIT_OLD_BEACON) {
        frame->setFramePending(true);
        Ieee802154FrameCodec::finalize(*frame);
    }
    constructedBeacon = BeaconConfiguration{uint8_t(bo),uint8_t(so),ble,
            uint32_t(getInteger("macBattLifeExtPeriods")),uint32_t(getInteger("macUnitBackoffPeriod")),
            uint8_t(frame->getPayload(1) & 15)};
    constructedGtsAdvertisements = ledger->getAdvertisements();
    constructedCfpGrants = ledger->getGrants();
    auto packet = new Packet("IEEE802154-periodic-beacon");
    transmittedBeaconIfs = interframeSpacing(frame->getChunkLength());
    transmittedBeaconBroadcastPending = advertisedBroadcast;
    packet->insertAtBack(frame);
    packet->addTag<PacketProtocolTag>()->setProtocol(&Protocol::ieee802154);
    auto id = allocatePhyId();
    auto initialRequestId = initial ? std::optional<uint64_t>(superframeStartRequest->id) : std::nullopt;
    phyOperation = PhyOperation{id,PhyPurpose::SUPERFRAME_TRANSMIT};
    emit(packetSentToLowerSignal,packet);
    if (!phyOperation || phyOperation->id != id || !started || !phyAvailable ||
            (initialRequestId && (!superframeStartRequest || superframeStartRequest->id != *initialRequestId)) ||
            (!initialRequestId && !periodicBeaconing)) {
        if (phyOperation && phyOperation->id == id)
            phyOperation.reset();
        delete packet;
        restoreIdle();
        return;
    }
    physicallayer::Ieee802154PhyTransmitRequest request;
    if (target && (!initial || superframeStartRequest->phase == SuperframeStartRequest::Phase::WAIT_ACTIVATION ||
            (!superframeStartRequest->parameters.panCoordinator && superframeStartRequest->parameters.startTime)))
        request.startAt = superframe->getFutureSimulationTime(*target);
    if (request.startAt && *request.startAt < simTime()) {
        delete packet;
        phyOperation.reset();
        skipPeriodicBeacon();
        return;
    }
    if (phy->submitTransmit(id,packet,request) != PhyAdmission::ACCEPTED) {
        delete packet;
        phyOperation.reset();
        if (initial)
            failSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
        else
            skipPeriodicBeacon();
    }
}

void Ieee802154StandardMac::startOutgoingCap()
{
    if (!periodicBeaconing || !transmittedBeaconAnchor || !outgoingEpoch)
        return;
    auto epoch = *outgoingEpoch;
    auto geometry = Ieee802154SuperframeTiming::geometry(epoch.beaconOrder,epoch.superframeOrder);
    if (!geometry->activeDuration)
        return;
    clocktime_t end;
    if (epoch.batteryLifeExtension) {
        auto afterIfs = superframe->getFutureClockTime(simTime() + transmittedBeaconIfs);
        end = afterIfs + SIMTIME_AS_CLOCKTIME(SimTime(int64_t(epoch.batteryLifePeriods) * int64_t(epoch.backoffPeriod) * 16,SIMTIME_US));
    }
    else
        end = *transmittedBeaconAnchor + SIMTIME_AS_CLOCKTIME(SimTime((int64_t(60) << epoch.superframeOrder) * (epoch.finalCapSlot + 1) * 16,SIMTIME_US));
    outgoingCapEnd = end > superframe->getCurrentClockTime() ? std::optional<clocktime_t>(end) : std::nullopt;
    auto anchor = *transmittedBeaconAnchor;
    bool scheduled = scheduleCapDutyEnd();
    setOutgoingCapActive(scheduled);
    if (periodicBeaconing && transmittedBeaconAnchor && *transmittedBeaconAnchor == anchor)
        publishAccessWindow(anchor, simTime(), transmittedBeaconIfs, epoch.beaconOrder, epoch.superframeOrder, epoch.finalCapSlot,
                epoch.batteryLifeExtension, epoch.backoffPeriod, epoch.batteryLifePeriods, Ieee802154AccessWindowOrigin::OUTGOING);
}

void Ieee802154StandardMac::publishAccessWindow(clocktime_t anchor, simtime_t beaconEnd, simtime_t beaconIfs,
        uint8_t beaconOrder, uint8_t superframeOrder, uint8_t finalCapSlot, bool batteryLifeExtension,
        uint32_t backoffPeriod, uint32_t batteryLifePeriods, Ieee802154AccessWindowOrigin origin)
{
    if (!superframe || !slottedAccess || superframeOrder >= 15 || finalCapSlot > 15)
        return;
    auto end = anchor + SIMTIME_AS_CLOCKTIME(SimTime((int64_t(60) << superframeOrder) * (finalCapSlot + 1) * 16, SIMTIME_US));
    if (end <= superframe->getCurrentClockTime())
        return;
    if (origin == Ieee802154AccessWindowOrigin::INCOMING) {
        incomingCapEnd = end;
        auto receiveEpoch = receiveRetirementEpoch;
        if (!scheduleCapDutyEnd())
            setOutgoingCapActive(false);
        if (receiveEpoch != receiveRetirementEpoch)
            return;
    }
    auto start = superframe->getFutureClockTime(beaconEnd);
    auto period = SIMTIME_AS_CLOCKTIME(SimTime(16, SIMTIME_US) * backoffPeriod);
    Ieee802154AccessWindow window{0, start, end, anchor, {}};
    window.beaconOrder = beaconOrder;
    window.origin = origin;
    window.notBefore = beaconEnd;
    window.physicalCapStart = beaconEnd;
    window.physicalCapEnd = superframe->getFutureSimulationTime(end);
    if (window.physicalCapEnd <= beaconEnd)
        return;
    if (batteryLifeExtension) {
        auto earliest = superframe->getFutureClockTime(beaconEnd + beaconIfs);
        auto distance = CLOCKTIME_AS_SIMTIME(earliest - anchor).raw();
        auto ticks = CLOCKTIME_AS_SIMTIME(period).raw();
        auto count = distance > 0 ? (distance + ticks - 1) / ticks : 0;
        auto first = anchor + period * count;
        while (first < superframe->getCurrentClockTime() || superframe->getFutureSimulationTime(first) < beaconEnd + beaconIfs)
            first += period;
        window.bleStart = first;
        window.bleEnd = first + period * int64_t(batteryLifePeriods);
    }
    auto nextBoundary = nextBeaconBoundary();
    if (nextBoundary && *nextBoundary > superframe->getCurrentClockTime()) {
        auto preparation = superframe->getFutureSimulationTime(*nextBoundary) - phy->getTimingParameters().turnaroundTime;
        if (preparation >= simTime()) {
            auto preparationClock = superframe->getFutureClockTime(preparation);
            window.latestCompletion = preparationClock >= superframe->getCurrentClockTime() ?
                    superframe->getFutureSimulationTime(preparationClock) : simTime();
        }
        else
            window.latestCompletion = simTime();
    }
    auto installed = superframe->publishAccessWindow(window);
    auto receiveEpoch = receiveRetirementEpoch;
    Ieee802154MacWindowObservation observation;
    observation.start = installed.physicalCapStart; observation.end = installed.physicalCapEnd;
    observation.outgoing = origin == Ieee802154AccessWindowOrigin::OUTGOING;
    emit(capWindowInstalledSignal,1L,&observation);
    auto current = superframe->getAccessWindow();
    if (receiveEpoch != receiveRetirementEpoch || !started || resetRequest || !current || current->epoch != installed.epoch)
        return;
    resumeCapAccess();
}

void Ieee802154StandardMac::resumeCapAccess()
{
    if (!transaction || !started || !phyAvailable || retiringRequests || resetRequest || ackExchange || phyOperation)
        return;
    if (transaction->phase == Phase::WAIT_CAP ||
            (transaction->phase == Phase::ACCESS && transaction->slotted && transaction->accessPending))
        startData();
}

void Ieee802154StandardMac::handleAccessSearchDeadline()
{
    Enter_Method("handleAccessSearchDeadline");
    if (!transaction || transaction->search != Transaction::Search::SEARCHING)
        return;
    if (transaction->transmittedInCap) {
        transaction->search = Transaction::Search::NONE;
        return;
    }
    transaction->search = Transaction::Search::FALLBACK;
    startData();
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
    if (!pendingOriginals.empty() || (phyOperation && phyOperation->purpose != PhyPurpose::IDLE))
        return Ieee802154MacServiceAdmission::BUSY;
    if (phyOperation) {
        auto oldId = phyOperation->id;
        phyOperation.reset();
        phy->cancel(oldId);
        phyState = PhyState::OFF;
        if (!started || !phyAvailable || resetRequest || retiringRequests)
            return Ieee802154MacServiceAdmission::UNAVAILABLE;
    }
    scanRequest = ScanRequest{id, request, {request.type}, {}, 0, getBoolean("macAutoRequest")};
    if (superframe) superframe->cancelCfpBoundary();
    cfpReceive = false;
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
    if (superframe) {
        superframe->cancelBeaconPreparation();
        superframe->cancelCapEnd();
        superframe->clearAccessWindow();
        outgoingCapEnd.reset();
        incomingCapEnd.reset();
        setOutgoingCapActive(false);
        if (!scanRequest || scanRequest->id != id || !started || resetRequest)
            return admission;
    }
    auto internalId = allocatePhyId();
    phyOperation = PhyOperation{internalId, PhyPurpose::SCAN_GET};
    admission = macAdmission(phy->submitGet(internalId,{"phyCurrentChannelInfo"}));
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED) {
        phyOperation.reset();
        scanRequest.reset();
        scheduleNextPeriodicBeacon();
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
    if (superframe)
        superframe->clearReceptionAnchors();
    if (!resetRequest)
        scheduleNextPeriodicBeacon();
    if (managementClient)
        managementClient->handleScanConfirm(completed.id, completed.confirm);
    if (resetRequest)
        beginResetPhy();
    else if (superframe)
        refreshCfp();
    else
        restoreIdle();
}

void Ieee802154StandardMac::receiveScanBeacon(const Ptr<Ieee802154Frame>& frame,
        const physicallayer::Ieee802154PhyReceiveIndication& quality,
        const std::optional<Ieee802154SuperframeReceptionAnchor>& anchor)
{
    if (scanRequest->closing || !scanTimer->isScheduled() || simTime() > scanTimer->getArrivalTime() ||
            quality.timing.ppduStart < scanRequest->listeningSince || frame->getFrameType() != 0 ||
            frame->getSourceAddress().isNone())
        return;
    auto description = describeBeacon(frame,quality,anchor);
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
    if (notify && started && managementClient)
        managementClient->handleBeaconNotifyIndication(descriptor);
    if (scanRequest && scanRequest->id == id && !scanRequest->closing && limit)
        finishScan(Ieee802154MacStatus::LIMIT_REACHED);
}

std::optional<Ieee802154PanDescriptor> Ieee802154StandardMac::describeBeacon(const Ptr<Ieee802154Frame>& frame,
        const physicallayer::Ieee802154PhyReceiveIndication& quality,
        const std::optional<Ieee802154SuperframeReceptionAnchor>& anchor) const
{
    const auto& source = frame->getSourceAddress();
    if (frame->getFrameType() != 0 || source.isNone() || source.isBroadcast() || source.isExtendedGroup() || source.isShortUnallocated())
        return {};
    if (superframe && !anchor)
        return {}; // No first-symbol clock sample is available for this reception.
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
    auto timestamp = anchor ? Ieee802154SuperframeTiming::timestamp(anchor->localStart) :
            uint32_t((quality.timing.ppduStart + SimTime(160,SIMTIME_US)).inUnit(SIMTIME_US) / 16) & 0xffffff;
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
    if (beaconScheduleActive())
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
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
    auto frame = makeBeacon(getInteger("macPanId"),15,15,false);
    if (!frame) {
        if (!automatic && managementClient)
            managementClient->handleBeaconConfirm(id,{Ieee802154MacStatus::FRAME_TOO_LONG});
        return;
    }
    transaction = Transaction{id,nullptr,0,frame};
    transaction->kind = automatic ? Transaction::Kind::AUTO_BEACON : Transaction::Kind::BEACON;
    startData();
}

bool Ieee802154StandardMac::overlapsIncomingSuperframe(clocktime_t target, uint8_t beaconOrder,
        uint8_t superframeOrder, b psduLength) const
{
    if (!incomingSuperframe)
        return false;
    const auto& incoming = *incomingSuperframe;
    auto parent = Ieee802154SuperframeTiming::geometry(incoming.beaconOrder,incoming.superframeOrder);
    auto outgoing = Ieee802154SuperframeTiming::geometry(beaconOrder,superframeOrder);
    if (!parent || !parent->beaconInterval || !outgoing || !outgoing->beaconInterval)
        return false;
    auto now = superframe->getCurrentClockTime();
    auto childStart = target <= now ? simTime() : superframe->getFutureSimulationTime(target);
    auto childEnd = outgoing->activeDuration ? superframe->getFutureSimulationTime(target +
            SIMTIME_AS_CLOCKTIME(SimTime(int64_t(*outgoing->activeDuration) * 16,SIMTIME_US))) :
            childStart + Ieee802154ExchangeTiming::ppduDuration(psduLength,phy->getTimingParameters());
    auto period = SIMTIME_AS_CLOCKTIME(SimTime(int64_t(*parent->beaconInterval) * 16,SIMTIME_US));
    auto delta = CLOCKTIME_AS_SIMTIME(target - incoming.anchor).raw();
    auto periodTicks = CLOCKTIME_AS_SIMTIME(period).raw();
    if (parent->activeDuration) {
        auto phase = delta % periodTicks;
        if (phase < 0)
            phase += periodTicks;
        auto activeTicks = SimTime(int64_t(*parent->activeDuration) * 16,SIMTIME_US).raw();
        if (phase < activeTicks)
            return true; // The predicted parent cycle can already have started when START is submitted.
    }
    int64_t cycle = delta > 0 ? delta / periodTicks : 0;
    for (int64_t index = std::max<int64_t>(0,cycle - 1); index <= cycle + 1; ++index) {
        auto parentClock = incoming.anchor + period * index;
        simtime_t parentStart;
        if (index == 0)
            parentStart = incoming.physicalStart;
        else if (parentClock > now)
            parentStart = superframe->getFutureSimulationTime(parentClock);
        else
            continue; // No actually received parent beacon is known for this past cycle.
        simtime_t parentEnd;
        if (parent->activeDuration) {
            auto endClock = parentClock + SIMTIME_AS_CLOCKTIME(SimTime(int64_t(*parent->activeDuration) * 16,SIMTIME_US));
            if (endClock <= now)
                continue;
            parentEnd = superframe->getFutureSimulationTime(endClock);
        }
        else
            parentEnd = index == 0 ? incoming.physicalEnd : parentStart + (incoming.physicalEnd - incoming.physicalStart);
        if (index > 0) {
            auto uncertainty = superframe->getForecastUncertainty(parentStart - incoming.physicalStart,SimTime(par("maxPropagationDelay")));
            parentStart -= uncertainty;
            parentEnd += uncertainty;
        }
        if (Ieee802154SuperframeTiming::physicalIntervalsOverlap(childStart,childEnd,parentStart,parentEnd))
            return true;
    }
    return false;
}

Ptr<Ieee802154Frame> Ieee802154StandardMac::makeBeacon(uint16_t panId, uint8_t beaconOrder, uint8_t superframeOrder, bool batteryLifeExtension, bool consumeSequence, const Ieee802154GtsManager *ledger)
{
    if (!ledger) ledger = &gtsManager;
    auto frame = makeShared<Ieee802154Frame>();
    frame->setFrameType(0);
    frame->setFrameVersion(0);
    frame->setDsn(getInteger("macBsn"));
    auto shortAddress = getInteger("macShortAddress");
    frame->setSourceAddress(shortAddress < 0xfffe ? Ieee802154Address::fromShort(shortAddress) :
            std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value));
    frame->setSourcePanId(panId);
    frame->setFramePending(pending->count(frame->getSourcePanId(),Ieee802154Address::fromShort(0xffff),simTime()) != 0);
    Ieee802154BeaconCodec::Content content;
    bool beaconPanCoordinator = superframeStartRequest &&
                    ((superframeStartRequest->phase == SuperframeStartRequest::Phase::INITIAL && superframeStartRequest->preparingFirstBeacon) || superframeStartRequest->phase == SuperframeStartRequest::Phase::WAIT_ACTIVATION) ?
                    superframeStartRequest->parameters.panCoordinator : panCoordinator;
    uint8_t finalCapSlot = 15;
    if (beaconPanCoordinator && beaconOrder < 15 && superframeOrder < 15)
        for (const auto& grant : ledger->getGrants())
            finalCapSlot = std::min(finalCapSlot,uint8_t(grant.start - 1));
    content.superframeSpecification = beaconOrder | (superframeOrder << 4) | (finalCapSlot << 8) |
            (batteryLifeExtension ? 0x1000 : 0) | (beaconPanCoordinator ? 0x4000 : 0) |
            (getBoolean("macAssociationPermit") ? 0x8000 : 0);
    if (beaconPanCoordinator && beaconOrder < 15) {
        content.gtsPermit = getBoolean("macGtsPermit");
        std::vector<Ieee802154GtsDescriptor> descriptors;
        for (const auto& advertisement : ledger->getAdvertisements())
            descriptors.push_back({advertisement.key.shortAddress,advertisement.start,advertisement.length,advertisement.key.direction});
        content.setGtsDescriptors(descriptors);
    }
    content.payload = std::get<std::vector<uint8_t>>(*pib->get("macBeaconPayload").value);
    auto overhead = size_t(7 + (frame->getSourceAddress().isShort() ? 2 : 8) + 4 +
            (content.gtsDescriptors.empty() ? 0 : 1 + content.gtsDescriptors.size()));
    if (!content.gtsDescriptors.empty()) {
        size_t pendingBytes = 0;
        for (const auto& address : pending->addresses(frame->getSourcePanId(),simTime()))
            pendingBytes += address.isShort() ? 2 : 8;
        if (overhead + content.payload.size() + pendingBytes > 127)
            content.payload.clear();
    }
    if (content.payload.size() > 127 - overhead) {
        if (content.gtsDescriptors.empty())
            return nullptr;
        content.payload.clear(); // Mandatory GTS records take precedence over optional beacon payload.
    }
    content.pendingAddresses = pending->addresses(frame->getSourcePanId(),simTime(),127 - overhead - content.payload.size());
    auto bytes = Ieee802154BeaconCodec::encode(content);
    if (bytes.size() + 7 + (frame->getSourceAddress().isShort() ? 2 : 8) > 127)
        return nullptr;
    frame->setPayloadArraySize(bytes.size());
    for (size_t i = 0; i < bytes.size(); i++) frame->setPayload(i,bytes[i]);
    Ieee802154FrameCodec::finalize(*frame);
    if (consumeSequence)
        pib->set("macBsn",uint64_t((frame->getDsn() + 1) & 255));
    return frame;
}

bool Ieee802154StandardMac::fitsBeaconPayload(const std::vector<uint8_t>& payload) const
{
    auto sourceBytes = getInteger("macShortAddress") < 0xfffe ? 2 : 8;
    size_t overhead = 7 + sourceBytes + 4;
    auto addresses = pending->addresses(getInteger("macPanId"),simTime());
    for (const auto& address : addresses)
        overhead += address.isShort() ? 2 : 8;
    return overhead <= 127 && payload.size() <= 127 - overhead;
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
    if (!par("enableManagement").boolValue() || !managementClient || !coordinator || !phy || !access)
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
    auto previous = associatedDevices.find({pan,response.deviceAddress});
    bool changesBinding = previous != associatedDevices.end() &&
            (response.associationStatus != 0 || response.shortAddress != previous->second);
    bool ownsGts = changesBinding && response.deviceAddress.isExtended() &&
            std::any_of(gtsManager.getGrants().begin(),gtsManager.getGrants().end(),[&](const auto& grant) {
                return grant.key.panId == pan && grant.key.extendedAddress == response.deviceAddress.getExtended();
            });
    if (!failure && response.associationStatus == 0 && response.shortAddress < 0xfffe) {
        for (const auto& [peer,shortAddress] : associatedDevices)
            if (peer.first == pan && shortAddress == response.shortAddress && peer.second != response.deviceAddress)
                failure = Ieee802154MacStatus::INVALID_ADDRESS;
    }
    if (failure) {
        managementClient->handleAssociateResponseStatus(id,{response.deviceAddress,*failure});
        return admission;
    }
    if (constructedGtsCandidate && phyOperation && phyOperation->purpose == PhyPurpose::SUPERFRAME_TRANSMIT &&
            !phyOperation->transmitStarted && ownsGts)
        return Ieee802154MacServiceAdmission::BUSY;
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
    auto expiresAt = simTime() + transactionPersistence();
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
    if (ownsGts) {
        auto grants = gtsManager.getGrants();
        for (const auto& grant : grants) {
            if (grant.key.panId == pan && grant.key.extendedAddress == response.deviceAddress.getExtended())
                removeGtsGrant(grant.key);
            if (!pendingOriginals.count(id))
                return admission;
        }
    }
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
    if (beaconScheduleActive() && (!slottedAccess || !synchronization || !synchronization->installed || !synchronization->anchored))
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if (beaconScheduleActive()) {
        if (!incomingSuperframe || incomingSuperframe->panId != request.coordinatorPanId || !(incomingSuperframe->channel == request.channel))
            return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
        bool samePeer = incomingSuperframe->coordinator == request.coordinatorAddress;
        if (!samePeer && parentAssociated && request.coordinatorPanId == getInteger("macPanId"))
            samePeer = isAssociatedCoordinator(request.coordinatorPanId,request.coordinatorAddress) &&
                    isAssociatedCoordinator(incomingSuperframe->panId,incomingSuperframe->coordinator);
        if (!samePeer)
            return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    }
    if (!par("enableManagement").boolValue() || !managementClient || !phy || !access || panCoordinator)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if ((phyOperation && phyOperation->purpose != PhyPurpose::IDLE) || !pendingOriginals.empty())
        return Ieee802154MacServiceAdmission::BUSY;
    if (phyOperation) {
        auto oldId = phyOperation->id;
        phyOperation.reset();
        phy->cancel(oldId);
        phyState = PhyState::OFF;
        if (!started || !phyAvailable || resetRequest || retiringRequests)
            return Ieee802154MacServiceAdmission::UNAVAILABLE;
    }
    std::optional<Ieee802154MacStatus> failure;
    if (request.coordinatorAddress.isNone() || request.coordinatorAddress.isBroadcast() ||
            request.coordinatorAddress.isExtendedGroup() || request.coordinatorAddress.isShortUnallocated())
        failure = Ieee802154MacStatus::INVALID_ADDRESS;
    else if (parentAssociated || (request.capabilityInformation & ~0x8e) || request.security.securityLevel > 7)
        failure = Ieee802154MacStatus::INVALID_PARAMETER;
    else if (request.security.securityLevel != 0)
        failure = Ieee802154MacStatus::UNSUPPORTED_SECURITY;
    if (failure) {
        managementClient->handleAssociateConfirm(id,{0xffff,{},*failure});
        return admission;
    }
    associationRequest = AssociationRequest{id,request};
    if (incomingSuperframe && synchronization && synchronization->installed && synchronization->anchored)
        associationRequest->parameters.coordinatorIsPanCoordinator = incomingSuperframe->panCoordinator;
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
        parentAssociated = true;
        pib->set("macAssociatedPanCoord",completed.parameters.coordinatorIsPanCoordinator);
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
    bool leaving = parentAssociated &&
            (request.deviceAddress == coordinatorExtended || (request.deviceAddress.isShort() &&
                    coordinatorShort < 0xfffe && request.deviceAddress.getShort() == coordinatorShort));
    bool indirect = request.indirectTx && coordinator && !leaving;
    auto admission = indirect ? admit(id,true) : admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (beaconScheduleActive() && !indirect && !slottedAccess)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if (!par("enableManagement").boolValue() || !managementClient || !phy || !access)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    std::optional<Ieee802154MacStatus> failure;
    auto peer = canonicalAddress(request.devicePanId,request.deviceAddress);
    if (request.deviceAddress.isNone() || request.deviceAddress.isBroadcast() || request.deviceAddress.isExtendedGroup() ||
            request.deviceAddress.isShortUnallocated())
        failure = Ieee802154MacStatus::INVALID_ADDRESS;
    else if (request.devicePanId != getInteger("macPanId") || request.reason != (leaving ? 2 : 1) ||
            (!leaving && (!coordinator || !associatedDevices.count({request.devicePanId,peer}))) || request.security.securityLevel > 7)
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
        auto expiresAt = simTime() + transactionPersistence();
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
    bool affectsCandidate = constructedGtsCandidate && (!peer || (peer->isExtended() &&
            std::any_of(gtsManager.getGrants().begin(),gtsManager.getGrants().end(),[&](const auto& grant) {
                return grant.key.panId == panId && grant.key.extendedAddress == peer->getExtended();
            })));
    if (affectsCandidate && phyOperation && phyOperation->purpose == PhyPurpose::SUPERFRAME_TRANSMIT && !phyOperation->transmitStarted) {
        auto operationId = phyOperation->id;
        phyOperation.reset();
        constructedGtsCandidate.reset();
        phy->cancel(operationId);
        failSuperframeStart(Ieee802154MacServiceAbortReason::STOPPED);
    }
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
    if (peer && peer->isExtended()) {
        auto grants = gtsManager.getGrants();
        for (const auto& grant : grants)
            if (grant.key.panId == panId && grant.key.extendedAddress == peer->getExtended())
                removeGtsGrant(grant.key);
    }
    if (peer)
        associatedDevices.erase({panId,*peer});
    else {
        gtsManager.clear();
        pendingPanConflict.reset();
        broadcastReceivePending = false;
        autoPollPending.reset();
        if (childPhase)
            retirePeriodicBeaconing(Ieee802154MacServiceAbortReason::DISASSOCIATED);
        retireParentContext(false);
        pib->clearAssociation();
        parentAssociated = false;
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
    if (!peer && gtsRequest)
        completeGts(Ieee802154MacServiceAbortReason::DISASSOCIATED);
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
    if (beaconScheduleActive() && response.associatedMember && !slottedAccess)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if (!par("enableManagement").boolValue() || !managementClient || !phy || !access || !coordinator)
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
    auto requestId = orphanResponseRequest->id;
    auto request = orphanResponseRequest->parameters;
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
    auto oldBinding = associatedDevices.find({pan,request.orphanAddress});
    bool changedBinding = oldBinding != associatedDevices.end() && oldBinding->second != request.shortAddress;
    associatedDevices[{pan,request.orphanAddress}] = request.shortAddress;
    pib->set("macDsn",uint64_t((frame->getDsn() + 1) & 255));
    if (changedBinding) {
        auto grants = gtsManager.getGrants();
        for (const auto& grant : grants) {
            if (grant.key.panId == pan && grant.key.extendedAddress == request.orphanAddress.getExtended())
                removeGtsGrant(grant.key);
            if (!orphanResponseRequest || orphanResponseRequest->id != requestId || !started || resetRequest)
                return;
        }
    }
    transaction = Transaction{requestId,nullptr,0,frame};
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
        const physicallayer::Ieee802154PhyReceiveIndication& quality,
        const std::optional<Ieee802154SuperframeReceptionAnchor>& anchor, uint64_t receiveEpoch)
{
    if (frame->getFrameType() != 0)
        return false;
    auto descriptor = describeBeacon(frame,quality,anchor);
    if (!descriptor)
        return true;
    bool foreignSyncBeacon = synchronization && synchronization->installed && !matchesSyncCoordinator(*descriptor);
    bool located = renewSynchronization(*descriptor,anchor,quality.timing.ppduEnd);
    bool awaitingCapRetry = transaction && transaction->transmittedInCap && transaction->phase == Phase::WAIT_CAP && !synchronization;
    if (!located && transaction && (transaction->search == Transaction::Search::SEARCHING || awaitingCapRetry) && anchor && descriptor->contentAvailable &&
            descriptor->coordinatorPanId == getInteger("macPanId")) {
        auto address = descriptor->coordinatorAddress;
        located = (address.isShort() && address.getShort() < 0xfffe && address.getShort() == getInteger("macCoordShortAddress")) ||
                (address.isExtended() && !address.isExtendedGroup() && address == std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value));
    }
    if (receiveEpoch != receiveRetirementEpoch)
        return true;
    if (located && descriptor->contentAvailable && parentAssociated) {
        auto shortAddress = uint16_t(getInteger("macShortAddress"));
        auto extended = std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value).getExtended();
        for (const auto& advertised : descriptor->beacon.getGtsDescriptors()) {
            if (advertised.shortAddress != shortAddress)
                continue;
            Ieee802154GtsManager::Key key{uint16_t(getInteger("macPanId")),extended,shortAddress,advertised.direction};
            auto current = gtsManager.findGrant(key);
            if (!current)
                continue;
            auto grant = *current;
            if (!advertised.start) {
                gtsManager.removeGrant(key);
                retireInvalidGtsData();
                if (receiveEpoch != receiveRetirementEpoch)
                    return true;
                if (managementClient)
                    managementClient->handleGtsIndication({Ieee802154Address::fromShort(shortAddress),{grant.length,grant.key.direction,false}});
                if (receiveEpoch != receiveRetirementEpoch)
                    return true;
            }
            else if (advertised.length == grant.length && advertised.start + advertised.length <= 16 && advertised.start != grant.start) {
                grant.start = advertised.start;
                gtsManager.putGrant(grant);
            }
        }
    }
    if (located && descriptor->contentAvailable && gtsRequest && gtsRequest->waiting &&
            descriptor->coordinatorPanId == getInteger("macPanId")) {
        auto requested = gtsRequest->request.characteristics;
        auto shortAddress = uint16_t(getInteger("macShortAddress"));
        for (const auto& grant : descriptor->beacon.getGtsDescriptors()) {
            if (grant.shortAddress != shortAddress || grant.direction != requested.direction)
                continue;
            auto result = grant.start && grant.length == requested.length ? Ieee802154MacStatus::SUCCESS : Ieee802154MacStatus::DENIED;
            if (result == Ieee802154MacStatus::SUCCESS) {
                auto extended = std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value).getExtended();
                if (!gtsManager.putGrant({{uint16_t(getInteger("macPanId")),extended,shortAddress,grant.direction},grant.start,grant.length}))
                    result = Ieee802154MacStatus::DENIED;
            }
            completeGts(result);
            if (receiveEpoch != receiveRetirementEpoch)
                return true;
            break;
        }
    }
    if (located && anchor && descriptor->contentAvailable) {
        auto specification = descriptor->beacon.superframeSpecification;
        auto geometry = Ieee802154SuperframeTiming::geometry(specification & 15, (specification >> 4) & 15);
        if (geometry && geometry->beaconInterval) {
            incomingSuperframe = IncomingSuperframe{anchor->localStart,geometry->beaconOrder,geometry->superframeOrder,
                    anchor->physicalStart,quality.timing.ppduEnd,(specification & 0x4000) != 0,
                    descriptor->coordinatorPanId,descriptor->coordinatorAddress,descriptor->channel};
            if (superframeStartRequest && superframeStartRequest->awaitingFirstBoundary && superframeStartRequest->firstTarget &&
                    *superframeStartRequest->firstTarget > superframe->getCurrentClockTime()) {
                auto request = superframeStartRequest->parameters;
                auto candidate = makeBeacon(request.panId,request.beaconOrder,request.superframeOrder,request.batteryLifeExtension,false);
                if (candidate && overlapsIncomingSuperframe(*superframeStartRequest->firstTarget,request.beaconOrder,request.superframeOrder,candidate->getChunkLength())) {
                    superframe->cancelBeaconPreparation();
                    completeSuperframeStart(Ieee802154MacStatus::SUPERFRAME_OVERLAP);
                    if (receiveEpoch != receiveRetirementEpoch)
                        return true;
                }
            }
            if (childPhase && periodicBeaconing && outgoingEpoch) {
                auto outgoing = Ieee802154SuperframeTiming::geometry(outgoingEpoch->beaconOrder,outgoingEpoch->superframeOrder);
                auto period = SIMTIME_AS_CLOCKTIME(SimTime(int64_t(*outgoing->beaconInterval) * 16,SIMTIME_US));
                auto target = anchor->localStart + *childPhase;
                if (!transmittedBeaconAnchor && periodicTarget && target < *periodicTarget) {
                    auto delta = CLOCKTIME_AS_SIMTIME(*periodicTarget - target).raw();
                    auto ticks = CLOCKTIME_AS_SIMTIME(period).raw();
                    target += period * ((delta + ticks / 2) / ticks);
                }
                auto earliest = superframe->getFutureClockTime(simTime() + phy->getTimingParameters().turnaroundTime);
                if (target < earliest) {
                    auto delta = CLOCKTIME_AS_SIMTIME(earliest - target).raw();
                    auto ticks = CLOCKTIME_AS_SIMTIME(period).raw();
                    auto count = (delta + ticks - 1) / ticks;
                    target += period * count;
                }
                auto candidate = makeBeacon(getInteger("macPanId"),outgoingEpoch->beaconOrder,outgoingEpoch->superframeOrder,
                        outgoingEpoch->batteryLifeExtension,false);
                bool overlap = candidate && overlapsIncomingSuperframe(target,outgoingEpoch->beaconOrder,
                        outgoingEpoch->superframeOrder,candidate->getChunkLength());
                if (overlap) {
                    auto channel = descriptor->channel;
                    auto pan = descriptor->coordinatorPanId;
                    auto address = descriptor->coordinatorAddress;
                    retirePeriodicBeaconing(Ieee802154MacServiceAbortReason::STOPPED);
                    if (receiveEpoch != receiveRetirementEpoch)
                        return true;
                    if (managementClient)
                        managementClient->handleSyncLossIndication({Ieee802154SyncLossReason::SUPERFRAME_OVERLAP,pan,channel,address,{},{}});
                    if (receiveEpoch != receiveRetirementEpoch)
                        return true;
                }
                else {
                    superframe->cancelBeaconPreparation();
                    periodicTarget = target;
                    scheduleNextPeriodicBeacon();
                }
            }
            if (geometry->activeDuration)
                publishAccessWindow(anchor->localStart, quality.timing.ppduEnd, interframeSpacing(frame->getChunkLength()),
                    geometry->beaconOrder, geometry->superframeOrder, (specification >> 8) & 15, (specification & 0x1000) != 0,
                    getInteger("macUnitBackoffPeriod"),getInteger("macBattLifeExtPeriods"),Ieee802154AccessWindowOrigin::INCOMING);
            if (receiveEpoch != receiveRetirementEpoch || !started || resetRequest)
                return true;
            if (synchronization && synchronization->installed && synchronization->request.trackBeacon)
                installCfpWindows(anchor->localStart,geometry->superframeOrder,false,gtsManager.getGrants());
        }
        if (receiveEpoch != receiveRetirementEpoch)
            return true;
    }
    if (descriptor->contentAvailable && associationRequest && associationRequest->waitingBeacon && !pollRequest && !transaction &&
            descriptor->coordinatorPanId == associationRequest->parameters.coordinatorPanId &&
            (descriptor->coordinatorAddress == associationRequest->parameters.coordinatorAddress ||
             (located && incomingSuperframe && descriptor->coordinatorAddress == incomingSuperframe->coordinator)) &&
            std::find(descriptor->beacon.pendingAddresses.begin(),descriptor->beacon.pendingAddresses.end(),
                    std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value)) != descriptor->beacon.pendingAddresses.end()) {
        pollRequest = PollRequest{associationRequest->id,{associationRequest->parameters.coordinatorPanId,associationRequest->parameters.coordinatorAddress}};
        pollRequest->association = true;
        startPoll();
    }
    if (descriptor->contentAvailable && isAssociatedCoordinator(descriptor->coordinatorPanId,descriptor->coordinatorAddress)) {
        broadcastReceivePending = frame->getFramePending();
        const auto& addresses = descriptor->beacon.pendingAddresses;
        auto shortAddress = Ieee802154Address::fromShort(getInteger("macShortAddress"));
        auto extendedAddress = std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value);
        auto advertised = std::find_if(addresses.begin(),addresses.end(),[&](const auto& address) {
            return (getInteger("macShortAddress") < 0xfffe && address == shortAddress) || address == extendedAddress;
        });
        if (advertised == addresses.end() || !getBoolean("macAutoRequest"))
            autoPollPending.reset();
        else if (!pollRequest)
            autoPollPending = *advertised;
        restoreIdle();
        if (receiveEpoch != receiveRetirementEpoch)
            return true;
    }
    bool conflict = descriptor->contentAvailable && (descriptor->beacon.superframeSpecification & 0x4000) &&
            descriptor->coordinatorPanId == getInteger("macPanId");
    auto coordinator = std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value);
    if (conflict && panCoordinator && managementClient)
        managementClient->handleSyncLossIndication({Ieee802154SyncLossReason::PAN_ID_CONFLICT,descriptor->coordinatorPanId,
                descriptor->channel,std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value),{},{}});
    else if (conflict && !panCoordinator && parentAssociated && getBoolean("macAssociatedPanCoord") && coordinator.isExtended() &&
            descriptor->coordinatorAddress != coordinator && descriptor->coordinatorAddress !=
            Ieee802154Address::fromShort(getInteger("macCoordShortAddress")) && !pendingPanConflict) {
        pendingPanConflict = PanConflict{descriptor->coordinatorPanId,coordinator,descriptor->channel};
        restoreIdle();
    }
    if (receiveEpoch != receiveRetirementEpoch)
        return true;
    if (started && managementClient && (foreignSyncBeacon || !getBoolean("macAutoRequest") || getBoolean("macNotifyAllBeacons") ||
            (descriptor->contentAvailable && !descriptor->beacon.payload.empty())))
        managementClient->handleBeaconNotifyIndication(*descriptor);
    if (receiveEpoch == receiveRetirementEpoch)
        restoreIdle();
    return true;
}

bool Ieee802154StandardMac::matchesSyncCoordinator(const Ieee802154PanDescriptor& descriptor) const
{
    if (!synchronization || descriptor.coordinatorPanId != getInteger("macPanId") || !(descriptor.channel == synchronization->request.channel))
        return false;
    auto address = descriptor.coordinatorAddress;
    return (address.isShort() && address.getShort() < 0xfffe && address.getShort() == getInteger("macCoordShortAddress")) ||
            (address.isExtended() && !address.isExtendedGroup() && address == std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value));
}

bool Ieee802154StandardMac::renewSynchronization(const Ieee802154PanDescriptor& descriptor,
        const std::optional<Ieee802154SuperframeReceptionAnchor>& anchor, simtime_t physicalEnd)
{
    if (!synchronization || !synchronization->installed || !synchronization->wantsRx || !anchor || !descriptor.contentAvailable ||
            !matchesSyncCoordinator(descriptor) || anchor->localStart < synchronization->windowStart ||
            physicalEnd > synchronization->physicalDeadline)
        return false;
    auto specification = descriptor.beacon.superframeSpecification;
    auto geometry = Ieee802154SuperframeTiming::geometry(specification & 15,(specification >> 4) & 15);
    if (!geometry || !geometry->beaconInterval)
        return false;
    superframe->cancelSyncEvents();
    synchronization->anchor = anchor->localStart;
    synchronization->beaconOrder = geometry->beaconOrder;
    synchronization->misses = 0;
    synchronization->anchored = true;
    if (!synchronization->request.trackBeacon) {
        retireSynchronization(Ieee802154MacServiceAbortReason::STOPPED);
    }
    else
        scheduleTrackingWindow();
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
    return parentAssociated && panId == getInteger("macPanId") &&
            ((address.isExtended() && !address.isExtendedGroup() && address == std::get<Ieee802154Address>(*pib->get("macCoordExtendedAddress").value)) ||
            (address.isShort() && address.getShort() < 0xfffe && address.getShort() == getInteger("macCoordShortAddress")));
}

bool Ieee802154StandardMac::receiverWantsRx() const
{
    return receiverWindow && receiverWindow->duration && simTime() >= receiverWindow->prepareAt &&
            (!receiverWindow->startsAt || receiverWindow->duration == 0xffffff ||
            simTime() < receiverWindow->endsAt.value_or(*receiverWindow->startsAt + SimTime(int64_t(receiverWindow->duration) * 16,SIMTIME_US)));
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
    auto deadline = receiverWindow->endsAt.value_or(*receiverWindow->startsAt + SimTime(int64_t(receiverWindow->duration) * 16,SIMTIME_US));
    auto timestamp = receiverWindow->endTimestamp.value_or(uint32_t(deadline.inUnit(SIMTIME_US) / 16) & 0xffffff);
    restoreIdle();
    if (!receiverWindow || receiverWindow->id != id)
        return;
    clearReceiverWindow();
    if (managementClient)
        managementClient->handleRxEnableIndication({timestamp});
}

bool Ieee802154StandardMac::consumeReceiverWindow()
{
    if (!receiverWindow || !receiverWindow->autoOff || !receiverWindow->startsAt || simTime() < *receiverWindow->startsAt)
        return false;
    if (receiverWindow->duration != 0xffffff &&
            simTime() > receiverWindow->endsAt.value_or(*receiverWindow->startsAt + SimTime(int64_t(receiverWindow->duration) * 16,SIMTIME_US)))
        return false;
    clearReceiverWindow();
    return true;
}

Ieee802154MacServiceAdmission Ieee802154StandardMac::submitGts(uint64_t id, const Ieee802154MacGtsRequest& request)
{
    Enter_Method("submitGts");
    auto admission = admitRadio(id);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        return admission;
    if (!par("enableManagement").boolValue() || !managementClient || !superframe || !slottedAccess || !phy)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if (gtsRequest)
        return Ieee802154MacServiceAdmission::BUSY;
    auto shortAddress = getInteger("macShortAddress");
    auto result = Ieee802154MacStatus::SUCCESS;
    if (shortAddress >= 0xfffe)
        result = Ieee802154MacStatus::NO_SHORT_ADDRESS;
    else if (!request.characteristics.length || request.characteristics.length > 15 || uint8_t(request.characteristics.direction) > 1)
        result = Ieee802154MacStatus::INVALID_PARAMETER;
    else if (request.security.securityLevel)
        result = Ieee802154MacStatus::UNSUPPORTED_SECURITY;
    if (result != Ieee802154MacStatus::SUCCESS) {
        managementClient->handleGtsConfirm(id,{request.characteristics,result});
        return admission;
    }
    if (panCoordinator && (request.characteristics.allocate || !request.coordinatorDevice))
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    if (request.coordinatorDevice && !panCoordinator) {
        managementClient->handleGtsConfirm(id,{request.characteristics,Ieee802154MacStatus::INVALID_PARAMETER});
        return admission;
    }
    if (request.coordinatorDevice) {
        if (!periodicBeaconing) {
            managementClient->handleGtsConfirm(id,{request.characteristics,Ieee802154MacStatus::INVALID_PARAMETER});
            return admission;
        }
        std::optional<Ieee802154GtsManager::Key> key;
        for (const auto& grant : gtsManager.getGrants())
            if (grant.key.panId == getInteger("macPanId") && grant.key.shortAddress == *request.coordinatorDevice &&
                    grant.key.direction == request.characteristics.direction && grant.length == request.characteristics.length &&
                    associatedDevices.count({grant.key.panId,Ieee802154Address::fromExtended(grant.key.extendedAddress)}) &&
                    associatedDevices.at({grant.key.panId,Ieee802154Address::fromExtended(grant.key.extendedAddress)}) == grant.key.shortAddress) {
                key = grant.key;
                break;
            }
        if (!key) {
            managementClient->handleGtsConfirm(id,{request.characteristics,Ieee802154MacStatus::INVALID_PARAMETER});
            return admission;
        }
        auto outerNotifications = syncLossNotificationsActive;
        syncLossNotificationsActive = true;
        removeGtsGrant(*key,true,false);
        managementClient->handleGtsConfirm(id,{request.characteristics,Ieee802154MacStatus::SUCCESS});
        syncLossNotificationsActive = outerNotifications;
        if (!outerNotifications && resetRequest && resetRequest->deferredResult) {
            auto deferred = *resetRequest->deferredResult;
            finishReset(deferred);
        }
        return admission;
    }
    if (!parentAssociated || !getBoolean("macAssociatedPanCoord") || !synchronization || !synchronization->installed ||
            !synchronization->request.trackBeacon || !synchronization->anchored || !incomingSuperframe || !incomingSuperframe->panCoordinator)
        return Ieee802154MacServiceAdmission::UNSUPPORTED_OPERATION;
    std::optional<Ieee802154GtsManager::Key> localRemoval;
    if (!request.characteristics.allocate) {
        auto extended = std::get<Ieee802154Address>(*pib->get("macExtendedAddress").value).getExtended();
        Ieee802154GtsManager::Key key{uint16_t(getInteger("macPanId")),extended,uint16_t(shortAddress),request.characteristics.direction};
        auto grant = gtsManager.findGrant(key);
        if (!grant || grant->length != request.characteristics.length) {
            managementClient->handleGtsConfirm(id,{request.characteristics,Ieee802154MacStatus::INVALID_PARAMETER});
            return admission;
        }
        localRemoval = key;
    }
    auto frame = makeShared<Ieee802154Frame>();
    frame->setFrameType(3);
    frame->setFrameVersion(0);
    frame->setDsn(getInteger("macDsn"));
    frame->setSourceAddress(Ieee802154Address::fromShort(shortAddress));
    frame->setSourcePanId(getInteger("macPanId"));
    frame->setAckRequest(true);
    auto content = Ieee802154CommandCodec::encode(Ieee802154CommandCodec::GtsRequest{request.characteristics});
    frame->setPayloadArraySize(content.size());
    for (size_t i = 0; i < content.size(); ++i) frame->setPayload(i,content[i]);
    Ieee802154FrameCodec::finalize(*frame);
    pib->set("macDsn",uint64_t((frame->getDsn() + 1) & 255));
    gtsRequest = GtsRequestState{id,request};
    transaction = Transaction{id,nullptr,0,frame};
    transaction->kind = Transaction::Kind::GTS_REQUEST;
    if (localRemoval) {
        gtsManager.removeGrant(*localRemoval);
        gtsManager.removeAdvertisement(*localRemoval);
        refreshCfp();
    }
    startData();
    return admission;
}

void Ieee802154StandardMac::completeGts(const Ieee802154MacServiceResult& result)
{
    if (!gtsRequest)
        return;
    auto completed = *gtsRequest;
    gtsRequest.reset();
    cancelEvent(gtsTimer);
    if (managementClient)
        managementClient->handleGtsConfirm(completed.id,{completed.request.characteristics,result});
}

void Ieee802154StandardMac::decideGts(const Ieee802154GtsManager::Key& key, const Ieee802154GtsCharacteristics& characteristics)
{
    auto membership = associatedDevices.find({key.panId,Ieee802154Address::fromExtended(key.extendedAddress)});
    if (!panCoordinator || !periodicBeaconing || !outgoingEpoch || outgoingEpoch->superframeOrder >= 15 ||
            key.panId != getInteger("macPanId") || membership == associatedDevices.end() || membership->second != key.shortAddress)
        return;
    if (!characteristics.allocate) {
        auto grant = gtsManager.findGrant(key);
        if (grant && grant->length == characteristics.length)
            removeGtsGrant(key,false);
        return;
    }
    if (!getBoolean("macGtsPermit") || gtsManager.findGrant(key) || !gtsManager.canReserve(key))
        return;
    unsigned int used = 0;
    for (const auto& grant : gtsManager.getGrants()) used += grant.length;
    uint8_t maximum = 0;
    for (unsigned int length = 1; length + used < 16; ++length) {
        auto candidate = gtsManager;
        auto start = uint8_t(16 - used - length);
        if (!candidate.putGrant({key,start,uint8_t(length)}) || !candidate.advertise(key,start,length))
            break;
        candidate.clearAdvertisements();
        auto beacon = makeBeacon(getInteger("macPanId"),outgoingEpoch->beaconOrder,outgoingEpoch->superframeOrder,
                outgoingEpoch->batteryLifeExtension,false,&candidate);
        if (!beacon)
            break;
        auto ordinaryDuration = Ieee802154ExchangeTiming::ppduDuration(beacon->getChunkLength(),phy->getTimingParameters());
        if (gtsPhysicalCapDuration(outgoingEpoch->superframeOrder,16 - used - length) - ordinaryDuration >= SimTime(440 * 16,SIMTIME_US))
            maximum = length;
    }
    bool granted = characteristics.length <= maximum;
    auto start = granted ? uint8_t(16 - used - characteristics.length) : uint8_t(0);
    auto length = granted ? characteristics.length : maximum;
    if (granted && !gtsManager.putGrant({key,start,length}))
        return;
    if (!gtsManager.advertise(key,start,length)) {
        if (granted) gtsManager.removeGrant(key);
        return;
    }
    if (gtsManager.reservedCount() >= 7)
        pib->set("macGtsPermit",false);
    if (granted) {
        auto outerNotifications = syncLossNotificationsActive;
        syncLossNotificationsActive = true;
        emitGtsObservation(gtsAllocatedSignal,{key,start,length});
        if (started && managementClient)
            managementClient->handleGtsIndication({Ieee802154Address::fromShort(key.shortAddress),characteristics});
        syncLossNotificationsActive = outerNotifications;
        if (!outerNotifications && resetRequest && resetRequest->deferredResult) {
            auto deferred = *resetRequest->deferredResult;
            finishReset(deferred);
        }
    }
}

void Ieee802154StandardMac::removeGtsGrant(const Ieee802154GtsManager::Key& key, bool advertiseRemoval, bool notify)
{
    auto grant = gtsManager.findGrant(key);
    if (!grant)
        return;
    auto removed = *grant;
    auto length = removed.length;
    auto outerNotifications = syncLossNotificationsActive;
    syncLossNotificationsActive = true;
    removeGtsFromLedger(gtsManager,key,advertiseRemoval);
    retireInvalidGtsData();
    emitGtsObservation(gtsDeallocatedSignal,removed);
    refreshCfp();
    if (notify && managementClient)
        managementClient->handleGtsIndication({Ieee802154Address::fromShort(key.shortAddress),{length,key.direction,false}});
    syncLossNotificationsActive = outerNotifications;
    if (!outerNotifications && resetRequest && resetRequest->deferredResult) {
        auto deferred = *resetRequest->deferredResult;
        finishReset(deferred);
    }
}

void Ieee802154StandardMac::removeGtsFromLedger(Ieee802154GtsManager& ledger, const Ieee802154GtsManager::Key& key, bool advertiseRemoval)
{
    auto grant = ledger.findGrant(key);
    if (!grant)
        return;
    auto length = grant->length;
    ledger.removeGrant(key);
    if (!advertiseRemoval)
        ledger.removeAdvertisement(key);
    else if (!ledger.advertise(key,0,length))
        throw cRuntimeError("Cannot retain IEEE 802.15.4 GTS removal descriptor");
    unsigned int used = 0;
    auto grants = ledger.getGrants();
    for (auto grant : grants) {
        used += grant.length;
        auto start = uint8_t(16 - used);
        if (start != grant.start) {
            grant.start = start;
            if (!ledger.putGrant(grant) || !ledger.advertise(grant.key,grant.start,grant.length))
                throw cRuntimeError("Cannot retain IEEE 802.15.4 GTS relocation descriptor");
        }
    }
}

void Ieee802154StandardMac::maintainGtsCapacity(uint8_t superframeOrder)
{
    while (!gtsManager.getGrants().empty()) {
        auto ordinaryLedger = gtsManager;
        ordinaryLedger.clearAdvertisements();
        auto beacon = makeBeacon(getInteger("macPanId"),getInteger("macBeaconOrder"),superframeOrder,
                getBoolean("macBattLifeExt"),false,&ordinaryLedger);
        if (!beacon)
            return;
        auto ordinaryDuration = Ieee802154ExchangeTiming::ppduDuration(beacon->getChunkLength(),phy->getTimingParameters());
        unsigned int used = 0;
        for (const auto& grant : gtsManager.getGrants()) used += grant.length;
        if (gtsPhysicalCapDuration(superframeOrder,16 - used) - ordinaryDuration >= SimTime(440 * 16,SIMTIME_US))
            return;
        auto key = gtsManager.getGrants().back().key;
        auto epoch = receiveRetirementEpoch;
        removeGtsGrant(key);
        if (!started || resetRequest || !periodicBeaconing || !panCoordinator || epoch != receiveRetirementEpoch)
            return;
    }
}

void Ieee802154StandardMac::ageGtsEpoch(clocktime_t boundary)
{
    if (!lastGtsAgedBoundary || boundary > *lastGtsAgedBoundary) {
        gtsManager.ageAdvertisements();
        lastGtsAgedBoundary = boundary;
    }
}

simtime_t Ieee802154StandardMac::gtsPhysicalCapDuration(uint8_t superframeOrder, unsigned int slots, bool immediate, std::optional<clocktime_t> target) const
{
    if (superframeOrder >= 15)
        return SIMTIME_ZERO;
    auto now = superframe->getCurrentClockTime();
    auto anchor = target.value_or(now);
    if (!target && !immediate && periodicTarget && outgoingEpoch) {
        anchor = *periodicTarget;
        if (anchor <= now) {
            auto interval = SIMTIME_AS_CLOCKTIME(SimTime((int64_t(960) << outgoingEpoch->beaconOrder) * 16,SIMTIME_US));
            auto elapsed = CLOCKTIME_AS_SIMTIME(now - anchor).raw();
            auto period = CLOCKTIME_AS_SIMTIME(interval).raw();
            anchor += interval * (elapsed / period + 1);
        }
    }
    auto physicalStart = anchor > now ? superframe->getFutureSimulationTime(anchor) : simTime();
    auto duration = SimTime((int64_t(60) << superframeOrder) * slots * 16,SIMTIME_US);
    return superframe->getFutureSimulationTime(anchor + SIMTIME_AS_CLOCKTIME(duration)) - physicalStart;
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
    std::optional<simtime_t> end;
    std::optional<uint32_t> endTimestamp;
    if (request.instances.size() > 1)
        status = Ieee802154MacStatus::INVALID_PARAMETER;
    else if (request.ranging)
        status = Ieee802154MacStatus::RANGING_NOT_SUPPORTED;
    else if (!request.instances.empty()) {
        const auto& instance = request.instances.front();
        if (instance.onTime > 0xffffff || instance.duration > 0xffffff)
            status = Ieee802154MacStatus::INVALID_PARAMETER;
        else if (beaconScheduleActive() && superframe) {
            auto anchor = panCoordinator ? transmittedBeaconAnchor : incomingSuperframe ? std::optional<clocktime_t>(incomingSuperframe->anchor) : std::nullopt;
            auto order = panCoordinator && outgoingEpoch ? outgoingEpoch->beaconOrder : incomingSuperframe ? incomingSuperframe->beaconOrder : uint8_t(15);
            if (!anchor || order >= 15)
                status = Ieee802154MacStatus::PAST_TIME;
            else {
                auto intervalSymbols = int64_t(960) << order;
                if (uint64_t(instance.onTime) + instance.duration >= uint64_t(intervalSymbols))
                    status = Ieee802154MacStatus::ON_TIME_TOO_LONG;
                else {
                    auto interval = SIMTIME_AS_CLOCKTIME(SimTime(intervalSymbols * 16,SIMTIME_US));
                    auto now = superframe->getCurrentClockTime();
                    auto target = *anchor + SIMTIME_AS_CLOCKTIME(SimTime(int64_t(instance.onTime) * 16,SIMTIME_US));
                    auto elapsed = CLOCKTIME_AS_SIMTIME(now - *anchor).raw();
                    if (elapsed > 0)
                        target += interval * (elapsed / CLOCKTIME_AS_SIMTIME(interval).raw());
                    auto sifs = SIMTIME_AS_CLOCKTIME(phy->getTimingParameters().symbolPeriod * getInteger("macSifsPeriod"));
                    if (now >= target - sifs) {
                        if (request.deferPermit)
                            target += interval;
                        else
                            status = Ieee802154MacStatus::PAST_TIME;
                    }
                    if (status == Ieee802154MacStatus::SUCCESS) {
                        auto endClock = target + SIMTIME_AS_CLOCKTIME(SimTime(int64_t(instance.duration) * 16,SIMTIME_US));
                        start = superframe->getFutureSimulationTime(target);
                        end = instance.duration ? superframe->getFutureSimulationTime(endClock) : *start;
                        endTimestamp = Ieee802154SuperframeTiming::timestamp(endClock);
                    }
                }
            }
        }
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
        receiverWindow = ReceiverWindow{id,start,prepareAt,instance.duration,instance.autoOff,end,endTimestamp};
        if (prepareAt > simTime())
            scheduleAt(prepareAt,receiverPrepareTimer);
        if (start && instance.duration && instance.duration != 0xffffff)
            scheduleAt(end.value_or(*start + SimTime(int64_t(instance.duration) * 16,SIMTIME_US)),receiverEndTimer);
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
