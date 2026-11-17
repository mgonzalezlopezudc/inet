// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/applications/ieee802154/Ieee802154ManagementApp.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/BytesChunk.h"

namespace inet {

Define_Module(Ieee802154ManagementApp);

Ieee802154ManagementApp::~Ieee802154ManagementApp() { cancelAndDelete(actionTimer); }

void Ieee802154ManagementApp::initialize()
{
    service.reference(this,"serviceModule",true);
    management.reference(this,"serviceModule",true);
    coordinator = par("coordinator");
    beaconOperation = par("beaconOperation");
    gtsOperation = par("gtsOperation");
    if (par("panId").intValue() < 0 || par("panId").intValue() >= 65535 ||
            par("channelNumber").intValue() < 11 || par("channelNumber").intValue() > 26 || par("cycles").intValue() < 1 ||
            (gtsOperation && !beaconOperation) || (par("enhancedBeaconOperation").boolValue() && (!gtsOperation || par("cycles").intValue() != 1)) || (beaconOperation && par("enhancedDiscovery").boolValue()) || par("gtsBatchSize").intValue() < 1 || SimTime(par("gtsBatchDelay")) <= SIMTIME_ZERO)
        throw cRuntimeError("Invalid management example PAN/channel/cycle configuration");
    actionTimer = new cMessage("actionTimer");
    action = coordinator ? Action::START : Action::RESET;
    scheduleAt(SimTime(coordinator ? 1 : 2,SIMTIME_MS),actionTimer);
}

uint64_t Ieee802154ManagementApp::allocateId()
{
    if (nextId == UINT64_MAX)
        throw cRuntimeError("Native service request IDs exhausted");
    return ++nextId;
}

void Ieee802154ManagementApp::accepted(Ieee802154MacServiceAdmission admission) const
{
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        throw cRuntimeError("Management example request refused (%d)",int(admission));
}

void Ieee802154ManagementApp::succeeded(const Ieee802154MacServiceResult& result) const
{
    if (result != Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS))
        throw cRuntimeError("Management example primitive did not succeed");
}

physicallayer::Ieee802154PhyChannel Ieee802154ManagementApp::channel() const
{
    return {physicallayer::Ieee802154PhyChannel::Phy::O_QPSK,physicallayer::Ieee802154PhyChannel::Band::MHZ_2450,int(par("channelNumber"))};
}

void Ieee802154ManagementApp::set(const char *name, const Ieee802154MacPibValue& value)
{
    accepted(service->submitSet(allocateId(),{name,value}));
}

void Ieee802154ManagementApp::handleMessage(cMessage *message)
{
    if (message != actionTimer)
        throw cRuntimeError("Unexpected management example input");
    if (action == Action::START) {
        set("macMinBe",uint64_t(0));
        set("macAssociationPermit",true);
        if (beaconOperation) {
            Ieee802154MacStartSuperframeRequest request{uint16_t(par("panId")),channel()};
            request.beaconOrder = 3; request.superframeOrder = 1;
            accepted(management->submitStartSuperframe(allocateId(),request));
        }
        else
            accepted(management->submitStart(allocateId(),{uint16_t(par("panId")),channel()}));
    }
    else if (action == Action::RESET) {
        selectedPan.reset();
        accepted(service->submitReset(allocateId(),{true}));
    }
    else if (action == Action::POLL)
        accepted(management->submitPoll(allocateId(),{*selectedPan->coordinatorPanId,selectedPan->coordinatorAddress}));
    else if (action == Action::GTS_REQUEST)
        requestGts(pendingGtsDirection);
    else {
        auto id = allocateId();
        permitRequest = id;
        auto admission = service->submitSet(id,{"macGtsPermit",true});
        if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
            permitRequest.reset(); // A later configured batch may attempt reopening again.
    }
}

void Ieee802154ManagementApp::handleResetConfirm(uint64_t, const Ieee802154MacResetConfirm& confirm)
{
    Enter_Method("handleResetConfirm");
    succeeded(confirm.result);
    set("macMinBe",uint64_t(0));
    set("macResponseWaitTime",uint64_t(beaconOperation ? 32 : 2));
    Ieee802154MacScanRequest request;
    request.type = beaconOperation ? Ieee802154ScanType::PASSIVE : par("enhancedDiscovery").boolValue() ? Ieee802154ScanType::ENHANCED_ACTIVE_SCAN : Ieee802154ScanType::ACTIVE;
    auto first = channel(); first.channel = 11;
    request.channels = {first,channel()};
    request.duration = beaconOperation ? 8640 : 256;
    accepted(management->submitScan(allocateId(),request));
}

void Ieee802154ManagementApp::handleStartConfirm(uint64_t, const Ieee802154MacStartConfirm& confirm)
{
    Enter_Method("handleStartConfirm"); succeeded(confirm.result);
}

void Ieee802154ManagementApp::handleStartSuperframeConfirm(uint64_t, const Ieee802154MacStartSuperframeConfirm& confirm)
{
    Enter_Method("handleStartSuperframeConfirm"); succeeded(confirm.result);
}

void Ieee802154ManagementApp::handleSyncConfirm(uint64_t, const Ieee802154MacSyncConfirm& confirm)
{
    Enter_Method("handleSyncConfirm"); succeeded(confirm.result);
}

void Ieee802154ManagementApp::handleScanConfirm(uint64_t, const Ieee802154MacScanConfirm& confirm)
{
    Enter_Method("handleScanConfirm"); succeeded(confirm.result);
    for (const auto& descriptor : confirm.panDescriptors)
        if (descriptor.coordinatorPanId == par("panId").intValue() && (descriptor.contentAvailable || (beaconOperation && descriptor.enhancedSuperframe) || (par("enhancedDiscovery").boolValue() && descriptor.frameVersion == 2)) &&
                !descriptor.coordinatorAddress.isNone() &&
                descriptor.securityStatus == Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS)) {
            selectedPan = descriptor;
            break;
        }
    if (!selectedPan)
        throw cRuntimeError("Requested PAN not discovered");
    if (beaconOperation) {
        auto specification = selectedPan->enhancedSuperframe ? selectedPan->enhancedSuperframe->specification : selectedPan->beacon.superframeSpecification;
        auto bo = specification & 15;
        if (bo == 15)
            throw cRuntimeError("Beacon operation requires a periodic coordinator");
        set("macPanId",uint64_t(*selectedPan->coordinatorPanId));
        if (selectedPan->coordinatorAddress.isShort())
            set("macCoordShortAddress",uint64_t(selectedPan->coordinatorAddress.getShort()));
        else
            set("macCoordExtendedAddress",selectedPan->coordinatorAddress);
        set("macBeaconOrder",uint64_t(bo));
        set("macAutoRequest",false);
        awaitingAssociationBeacon = true;
        accepted(management->submitSync(allocateId(),{selectedPan->channel,true}));
        return;
    }
    associateSelectedPan();
}

void Ieee802154ManagementApp::associateSelectedPan()
{
    Ieee802154MacAssociateRequest request{selectedPan->channel,*selectedPan->coordinatorPanId,selectedPan->coordinatorAddress};
    request.coordinatorIsPanCoordinator = selectedPan->enhancedSuperframe ? (selectedPan->enhancedSuperframe->specification & 0x4000) != 0 : selectedPan->frameVersion == 2 ? par("enhancedCoordinatorIsPanCoordinator").boolValue() :
            (selectedPan->beacon.superframeSpecification & 0x4000) != 0;
    accepted(management->submitAssociate(allocateId(),request));
}

void Ieee802154ManagementApp::handleAssociateIndication(const Ieee802154MacAssociateIndication& indication)
{
    Enter_Method("handleAssociateIndication");
    auto found = allocations.find(indication.deviceAddress);
    if (found == allocations.end()) {
        if (nextShortAddress >= 0xfffe)
            throw cRuntimeError("Example short-address allocation exhausted");
        found = allocations.emplace(indication.deviceAddress,nextShortAddress++).first;
    }
    accepted(management->submitAssociateResponse(allocateId(),{indication.deviceAddress,found->second,0}));
}

void Ieee802154ManagementApp::handleAssociateResponseStatus(uint64_t, const Ieee802154MacAssociateResponseStatus& status)
{
    Enter_Method("handleAssociateResponseStatus"); succeeded(status.result);
    if (par("enhancedBeaconOperation").boolValue()) set("macUseEnhancedBeacon",true);
}

void Ieee802154ManagementApp::sendData(const Ieee802154Address& destination, uint16_t panId, bool indirect)
{
    auto packet = new Packet(indirect ? "sleeping device reply" : "associated device DATA");
    packet->insertAtBack(makeShared<BytesChunk>(std::vector<uint8_t>{uint8_t(completedCycles),0x42}));
    Ieee802154MacDataRequest request;
    request.destinationAddress = destination; request.destinationPanId = panId;
    request.sourceAddressMode = coordinator && !gtsOperation ? Ieee802154Address::AddressType::EXTENDED : Ieee802154Address::AddressType::SHORT;
    request.ackTx = true; request.indirectTx = indirect; request.gtsTx = gtsOperation;
    if (par("enhancedData").boolValue()) {
        request.legacyTx = false; request.ackTx = false;
        request.sequenceSuppressed = par("enhancedSequenceSuppressed").boolValue();
        request.panIdSuppressed = par("enhancedPanIdSuppressed").boolValue();
    }
    auto admission = service->submitData(allocateId(),packet,request);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED)
        delete packet;
    accepted(admission);
}

void Ieee802154ManagementApp::handleAssociateConfirm(uint64_t, const Ieee802154MacAssociateConfirm& confirm)
{
    Enter_Method("handleAssociateConfirm"); succeeded(confirm.result);
    if (confirm.associationStatus != 0)
        throw cRuntimeError("Example association rejected");
    if (par("enhancedBeaconOperation").boolValue()) {
        set("macAutoRequest",false);
        awaitingEnhancedGtsBeacon = true;
        return;
    }
    if (beaconOperation)
        set("macAutoRequest",true);
    if (gtsOperation)
        requestGts(Ieee802154GtsDirection::DEVICE_TRANSMIT);
    else
        sendData(selectedPan->coordinatorAddress,*selectedPan->coordinatorPanId,false);
}

void Ieee802154ManagementApp::handleDataConfirm(uint64_t, const Ieee802154MacDataConfirm& confirm)
{
    Enter_Method("handleDataConfirm"); succeeded(confirm.result); confirmedMsdus++;
    if (!coordinator && !gtsOperation) {
        action = Action::POLL;
        scheduleAfter(SimTime(5,SIMTIME_MS),actionTimer);
    }
}

void Ieee802154ManagementApp::handleDataIndication(Packet *packet, const Ieee802154MacDataIndication& indication)
{
    Enter_Method("handleDataIndication"); take(packet); receivedMsdus++; delete packet;
    if (coordinator)
        sendData(indication.sourceAddress,indication.effectiveSourcePanId.value(),!gtsOperation);
    else if (gtsOperation)
        accepted(management->submitDisassociate(allocateId(),{*selectedPan->coordinatorPanId,selectedPan->coordinatorAddress,2,false}));
}

void Ieee802154ManagementApp::handlePollConfirm(uint64_t, const Ieee802154MacPollConfirm& confirm)
{
    Enter_Method("handlePollConfirm"); succeeded(confirm.result);
    accepted(management->submitDisassociate(allocateId(),{*selectedPan->coordinatorPanId,selectedPan->coordinatorAddress,2,false}));
}

void Ieee802154ManagementApp::handleDisassociateConfirm(uint64_t, const Ieee802154MacDisassociateConfirm& confirm)
{
    Enter_Method("handleDisassociateConfirm"); succeeded(confirm.result); completedCycles++;
    emit(registerSignal("completedCycle"),long(completedCycles));
    EV_INFO << "Managed cycle " << completedCycles << " completed with " << receivedMsdus << " received replies.\n";
    if (completedCycles < par("cycles").intValue()) {
        action = Action::RESET;
        scheduleAfter(SimTime(1,SIMTIME_MS),actionTimer);
    }
}

void Ieee802154ManagementApp::handleDisassociateIndication(const Ieee802154MacDisassociateIndication& indication)
{
    Enter_Method("handleDisassociateIndication"); allocations.erase(indication.deviceAddress);
}

void Ieee802154ManagementApp::finish()
{
    recordScalar("completedCycles",completedCycles);
    recordScalar("receivedMsdus",receivedMsdus);
    recordScalar("confirmedMsdus",confirmedMsdus);
}

void Ieee802154ManagementApp::handleSetConfirm(uint64_t id, const Ieee802154MacSetConfirm& confirm) { Enter_Method("handleSetConfirm"); if (permitRequest && *permitRequest == id) { permitRequest.reset(); return; } succeeded(confirm.result); }
void Ieee802154ManagementApp::handleGetConfirm(uint64_t, const Ieee802154MacGetConfirm&) { throw cRuntimeError("Example did not issue GET"); }
void Ieee802154ManagementApp::handlePurgeConfirm(uint64_t, const Ieee802154MacPurgeConfirm&) { throw cRuntimeError("Example did not issue PURGE"); }
void Ieee802154ManagementApp::handleBeaconNotifyIndication(const Ieee802154PanDescriptor& descriptor) { Enter_Method("handleBeaconNotifyIndication"); if (!selectedPan || descriptor.coordinatorPanId != selectedPan->coordinatorPanId || descriptor.coordinatorAddress != selectedPan->coordinatorAddress) return; if (awaitingAssociationBeacon) { awaitingAssociationBeacon = false; associateSelectedPan(); } else if (awaitingEnhancedGtsBeacon && descriptor.enhancedSuperframe) { awaitingEnhancedGtsBeacon = false; requestGts(Ieee802154GtsDirection::DEVICE_TRANSMIT); } }
void Ieee802154ManagementApp::handleBeaconConfirm(uint64_t, const Ieee802154MacBeaconConfirm&) { throw cRuntimeError("Example did not issue BEACON"); }
void Ieee802154ManagementApp::handleBeaconRequestIndication(const Ieee802154MacBeaconRequestIndication&) { Enter_Method("handleBeaconRequestIndication"); }
void Ieee802154ManagementApp::handleOrphanIndication(const Ieee802154MacOrphanIndication&) { Enter_Method("handleOrphanIndication"); }
void Ieee802154ManagementApp::handleOrphanResponseStatus(uint64_t, const Ieee802154MacOrphanResponseStatus&) { throw cRuntimeError("Example did not issue ORPHAN response"); }
void Ieee802154ManagementApp::handleSyncLossIndication(const Ieee802154MacSyncLossIndication&) { Enter_Method("handleSyncLossIndication"); }
void Ieee802154ManagementApp::handleRxEnableConfirm(uint64_t, const Ieee802154MacRxEnableConfirm&) { throw cRuntimeError("Example did not issue RX-ENABLE"); }
void Ieee802154ManagementApp::handleRxEnableIndication(const Ieee802154MacRxEnableIndication&) { Enter_Method("handleRxEnableIndication"); }
void Ieee802154ManagementApp::requestGts(Ieee802154GtsDirection direction)
{
    pendingGtsDirection = direction;
    Ieee802154MacGtsRequest request{{2,direction,true}};
    auto admission = management->submitGts(allocateId(),request);
    if (admission != Ieee802154MacServiceAdmission::ACCEPTED) {
        action = Action::GTS_REQUEST;
        scheduleAfter(SimTime(par("gtsBatchDelay")),actionTimer);
    }
}

void Ieee802154ManagementApp::handleGtsConfirm(uint64_t, const Ieee802154MacGtsConfirm& confirm)
{
    Enter_Method("handleGtsConfirm");
    if (confirm.result != Ieee802154MacServiceResult(Ieee802154MacStatus::SUCCESS)) {
        action = Action::GTS_REQUEST;
        scheduleAfter(SimTime(par("gtsBatchDelay")),actionTimer);
    }
    else if (confirm.characteristics.direction == Ieee802154GtsDirection::DEVICE_TRANSMIT)
        requestGts(Ieee802154GtsDirection::DEVICE_RECEIVE);
    else
        sendData(selectedPan->coordinatorAddress,*selectedPan->coordinatorPanId,false);
}

void Ieee802154ManagementApp::handleGtsIndication(const Ieee802154MacGtsIndication& indication)
{
    Enter_Method("handleGtsIndication");
    if (gtsOperation && coordinator && indication.characteristics.allocate && ++gtsRequestsInBatch >= par("gtsBatchSize").intValue()) {
        gtsRequestsInBatch = 0;
        if (!actionTimer->isScheduled()) {
            action = Action::REOPEN_PERMIT;
            scheduleAfter(SimTime(par("gtsBatchDelay")),actionTimer);
        }
    }
}
void Ieee802154ManagementApp::handlePromiscuousIndication(Packet *packet, const Ieee802154MacPromiscuousIndication&) { Enter_Method("handlePromiscuousIndication"); take(packet); delete packet; }
void Ieee802154ManagementApp::handleCommStatusIndication(Packet *packet, const Ieee802154MacCommStatusIndication&) { Enter_Method("handleCommStatusIndication"); if (packet) { take(packet); delete packet; } }

} // namespace inet
