//
// Copyright (C) 2006 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include <algorithm>
#include <cmath>

#include "inet/common/ModuleAccess.h"
#include "inet/common/Simsignals.h"
#include "inet/linklayer/common/MacAddressTag_m.h"

#ifdef INET_WITH_ETHERNET
#include "inet/linklayer/ethernet/common/EthernetMacHeader_m.h"
#endif // ifdef INET_WITH_ETHERNET

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211SubtypeTag_m.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211HeMgmtElements.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtAp.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211Primitives_m.h"
#include "inet/linklayer/ieee80211/twt/ITwtManager.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"

namespace inet {

namespace ieee80211 {

using namespace physicallayer;

Define_Module(Ieee80211MgmtAp);
Register_Class(Ieee80211MgmtAp::NotificationInfoSta);

static std::ostream& operator<<(std::ostream& os, const Ieee80211MgmtAp::StaInfo& sta)
{
    os << "address:" << sta.address;
    return os;
}

static void addApHeManagementElements(const Ptr<Ieee80211MgmtFrame>& body, Ieee80211Mib *mib)
{
    body->setHeCapabilitiesPresent(true);
    body->setHeCapabilities(makeHeCapabilitiesElement(mib->localHeCapabilities));
    body->setHeOperationPresent(true);
    body->setHeOperation(makeHeOperationElement(mib->heOperation));
}

static void setTwtSetupFrameFields(const Ptr<Ieee80211TwtSetupFrame>& frame, const TwtAgreement& agreement)
{
    uint64_t intervalUs = agreement.wakeInterval.inUnit(SIMTIME_US);
    uint8_t exponent = 0;
    while (intervalUs > 0xffff && exponent < 31) {
        intervalUs = (intervalUs + 1) / 2;
        exponent++;
    }
    frame->setTrigger(agreement.triggerEnabled);
    frame->setImplicit(agreement.implicit);
    frame->setAnnounced(agreement.announced);
    frame->setBroadcast(agreement.broadcast);
    frame->setFlowId(agreement.flowId);
    frame->setBroadcastId(agreement.broadcastId);
    frame->setTargetWakeTime(agreement.nextWakeTime.inUnit(SIMTIME_US));
    frame->setWakeIntervalExponent(exponent);
    frame->setWakeIntervalMantissa(intervalUs);
    frame->setNominalWakeDuration(std::clamp((int)std::ceil(agreement.wakeDuration.inUnit(SIMTIME_US) / 256.0), 1, 255));
    frame->setPersistence(agreement.persistence);
}

Ieee80211MgmtAp::~Ieee80211MgmtAp()
{
    cancelAndDelete(beaconTimer);
}

void Ieee80211MgmtAp::initialize(int stage)
{
    Ieee80211MgmtApBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        // read params and init vars
        ssid = par("ssid").stdstringValue();
        beaconInterval = par("beaconInterval");
        numAuthSteps = par("numAuthSteps");
        if (numAuthSteps != 2 && numAuthSteps != 4)
            throw cRuntimeError("parameter 'numAuthSteps' (number of frames exchanged during authentication) must be 2 or 4, not %d", numAuthSteps);
        channelNumber = -1; // value will arrive from physical layer in receiveChangeNotification()
        WATCH(ssid);
        WATCH(channelNumber);
        WATCH(beaconInterval);
        WATCH(numAuthSteps);
        WATCH(staList);

        // TODO fill in supportedRates

        // subscribe for notifications
        cModule *radioModule = getModuleFromPar<cModule>(par("radioModule"), this);
        radioModule->subscribe(Ieee80211Radio::radioChannelChangedSignal, this);

        // start beacon timer (randomize startup time)
        beaconTimer = new cMessage("beaconTimer");
    }
}

void Ieee80211MgmtAp::handleTimer(cMessage *msg)
{
    if (msg == beaconTimer) {
        sendBeacon();
        scheduleAfter(beaconInterval, beaconTimer);
    }
    else {
        throw cRuntimeError("internal error: unrecognized timer '%s'", msg->getName());
    }
}

void Ieee80211MgmtAp::handleCommand(int msgkind, cObject *ctrl)
{
    auto manager = dynamic_cast<ITwtManager *>(getModuleFromPar<cModule>(par("twtModule"), this));
    auto sendTwtConfirm = [this] (Ieee80211PrimConfirm *confirm, Ieee80211PrimResultCode result) {
        confirm->setResultCode(result);
        auto message = new cMessage(confirm->getClassName());
        message->setControlInfo(confirm);
        send(message, "agentOut");
    };
    if (auto cmd = dynamic_cast<Ieee80211Prim_TwtSetupRequest *>(ctrl)) {
        auto confirm = new Ieee80211Prim_TwtSetupConfirm();
        confirm->setAgreement(*cmd);
        if (manager == nullptr || !manager->isEnabled())
            sendTwtConfirm(confirm, PRC_REFUSED);
        else {
            TwtAgreement agreement;
            agreement.peerAddress = cmd->getPeerAddress();
            agreement.flowId = cmd->getFlowId();
            agreement.broadcast = cmd->getBroadcast();
            agreement.broadcastId = cmd->getBroadcastId();
            agreement.implicit = cmd->getImplicit();
            agreement.announced = cmd->getAnnounced();
            agreement.triggerEnabled = cmd->getTriggerEnabled();
            agreement.nextWakeTime = cmd->getTargetWakeTime();
            agreement.wakeInterval = cmd->getWakeInterval();
            agreement.wakeDuration = cmd->getWakeDuration();
            agreement.persistence = cmd->getPersistence();
            agreement.active = cmd->getWakeInterval() > SIMTIME_ZERO && cmd->getWakeDuration() > SIMTIME_ZERO;
            if (!agreement.active)
                sendTwtConfirm(confirm, PRC_INVALID_PARAMETERS);
            else {
                if (agreement.broadcast) {
                    TwtBroadcastSchedule schedule;
                    if (!manager->findBroadcastSchedule(agreement.broadcastId, schedule) ||
                            !manager->addBroadcastMember(agreement.broadcastId, agreement.peerAddress))
                        sendTwtConfirm(confirm, PRC_REFUSED);
                    else
                        sendTwtConfirm(confirm, PRC_SUCCESS);
                }
                else {
                    manager->installAgreement(agreement);
                    sendTwtConfirm(confirm, PRC_SUCCESS);
                }
            }
        }
    }
    else if (auto cmd = dynamic_cast<Ieee80211Prim_BroadcastTwtScheduleRequest *>(ctrl)) {
        auto confirm = new Ieee80211Prim_BroadcastTwtScheduleConfirm();
        confirm->setBroadcastId(cmd->getBroadcastId());
        if (manager == nullptr || !manager->isEnabled())
            sendTwtConfirm(confirm, PRC_REFUSED);
        else if (cmd->getOperation() == 2) {
            manager->removeBroadcastSchedule(cmd->getBroadcastId());
            sendTwtConfirm(confirm, PRC_SUCCESS);
        }
        else if (cmd->getWakeInterval() <= SIMTIME_ZERO || cmd->getWakeDuration() <= SIMTIME_ZERO) {
            sendTwtConfirm(confirm, PRC_INVALID_PARAMETERS);
        }
        else {
            TwtBroadcastSchedule schedule;
            schedule.peerAddress = MacAddress::BROADCAST_ADDRESS;
            schedule.broadcast = true;
            schedule.broadcastId = cmd->getBroadcastId();
            schedule.implicit = cmd->getImplicit();
            schedule.announced = cmd->getAnnounced();
            schedule.triggerEnabled = cmd->getTriggerEnabled();
            schedule.nextWakeTime = cmd->getTargetWakeTime();
            schedule.wakeInterval = cmd->getWakeInterval();
            schedule.wakeDuration = cmd->getWakeDuration();
            schedule.persistence = cmd->getPersistence();
            schedule.active = true;
            manager->installBroadcastSchedule(schedule);
            sendTwtConfirm(confirm, PRC_SUCCESS);
        }
    }
    else
        throw cRuntimeError("handleCommand(): unsupported control info class");
    delete ctrl;
}

void Ieee80211MgmtAp::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (signalID == Ieee80211Radio::radioChannelChangedSignal) {
        EV << "updating channel number\n";
        channelNumber = value;
    }
}

Ieee80211MgmtAp::StaInfo *Ieee80211MgmtAp::lookupSenderSTA(const Ptr<const Ieee80211MgmtHeader>& header)
{
    auto it = staList.find(header->getTransmitterAddress());
    return it == staList.end() ? nullptr : &(it->second);
}

void Ieee80211MgmtAp::sendManagementFrame(const char *name, const Ptr<Ieee80211MgmtFrame>& body, int subtype, const MacAddress& destAddr)
{
    auto packet = new Packet(name);
    packet->addTag<MacAddressReq>()->setDestAddress(destAddr);
    packet->addTag<Ieee80211SubtypeReq>()->setSubtype(subtype);
    packet->insertAtBack(body);
    sendDown(packet);
}

void Ieee80211MgmtAp::sendTwtActionFrame(const char *name, const Ptr<Ieee80211ActionFrame>& frame, const MacAddress& destAddr)
{
    auto packet = new Packet(name);
    frame->setReceiverAddress(destAddr);
    frame->setTransmitterAddress(mib->address);
    frame->setAddress3(mib->bssData.bssid);
    packet->insertAtFront(frame);
    sendDown(packet);
}

TwtAgreement Ieee80211MgmtAp::makeTwtAgreement(const Ptr<const Ieee80211TwtSetupFrame>& frame, const MacAddress& peer) const
{
    TwtAgreement agreement;
    agreement.peerAddress = peer;
    agreement.flowId = frame->getFlowId();
    agreement.broadcast = frame->getBroadcast();
    agreement.broadcastId = frame->getBroadcastId();
    agreement.implicit = frame->getImplicit();
    agreement.announced = frame->getAnnounced();
    agreement.triggerEnabled = frame->getTrigger();
    agreement.nextWakeTime = SimTime((int64_t)frame->getTargetWakeTime(), SIMTIME_US);
    agreement.wakeInterval = SimTime((int64_t)frame->getWakeIntervalMantissa() * (uint64_t(1) << frame->getWakeIntervalExponent()), SIMTIME_US);
    agreement.wakeDuration = SimTime((int64_t)frame->getNominalWakeDuration() * 256, SIMTIME_US);
    agreement.persistence = frame->getPersistence();
    agreement.active = agreement.wakeInterval > SIMTIME_ZERO && agreement.wakeDuration > SIMTIME_ZERO && agreement.wakeDuration <= agreement.wakeInterval;
    return agreement;
}

void Ieee80211MgmtAp::sendBeacon()
{
    EV << "Sending beacon\n";
    const auto& body = makeShared<Ieee80211BeaconFrame>();
    body->setSSID(ssid.c_str());
    body->setSupportedRates(supportedRates);
    body->setBeaconInterval(beaconInterval);
    body->setChannelNumber(channelNumber);
    addApHeManagementElements(body, mib);
    if (auto manager = dynamic_cast<ITwtManager *>(getModuleFromPar<cModule>(par("twtModule"), this)); manager != nullptr && manager->isEnabled()) {
        auto schedules = manager->getBroadcastSchedules();
        if (!schedules.empty()) {
            body->setBroadcastTwtPresent(true);
            body->setBroadcastTwtSchedulesArraySize(schedules.size());
            for (size_t i = 0; i < schedules.size(); ++i) {
                Ieee80211BroadcastTwtScheduleElement element;
                element.broadcastId = schedules[i].broadcastId;
                element.implicit = schedules[i].implicit;
                element.announced = schedules[i].announced;
                element.triggerEnabled = schedules[i].triggerEnabled;
                element.persistence = schedules[i].persistence;
                element.targetWakeTime = schedules[i].nextWakeTime;
                element.wakeInterval = schedules[i].wakeInterval;
                element.wakeDuration = schedules[i].wakeDuration;
                body->setBroadcastTwtSchedules(i, element);
            }
        }
    }
    body->setChunkLength(B(8 + 2 + 2 + (2 + ssid.length()) + (2 + supportedRates.numRates)) + getHeMgmtElementsLength(body));
    sendManagementFrame("Beacon", body, ST_BEACON, MacAddress::BROADCAST_ADDRESS);
}

void Ieee80211MgmtAp::handleAuthenticationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    const auto& requestBody = packet->peekData<Ieee80211AuthenticationFrame>();
    int frameAuthSeq = requestBody->getSequenceNumber();
    EV << "Processing Authentication frame, seqNum=" << frameAuthSeq << "\n";

    // create STA entry if needed
    StaInfo *sta = lookupSenderSTA(header);
    if (!sta) {
        MacAddress staAddress = header->getTransmitterAddress();
        sta = &staList[staAddress]; // this implicitly creates a new entry
        sta->address = staAddress;
        mib->bssAccessPointData.stations[staAddress] = Ieee80211Mib::NOT_AUTHENTICATED;
        sta->authSeqExpected = 1;
    }

    // reset authentication status, when starting a new auth sequence
    // The statements below are added because the L2 handover time was greater than before when
    // a STA wants to re-connect to an AP with which it was associated before. When the STA wants to
    // associate again with the previous AP, then since the AP is already having an entry of the STA
    // because of old association, and thus it is expecting an authentication frame number 3 but it
    // receives authentication frame number 1 from STA, which will cause the AP to return an Auth-Error
    // making the MN STA to start the handover process all over again.
    if (frameAuthSeq == 1) {
        if (mib->bssAccessPointData.stations[sta->address] == Ieee80211Mib::ASSOCIATED) {
            sendDisAssocNotification(sta->address);

            // Destroy per-STA queue bank only for APs operating in ax mode.
            try {
                cModule *macModule = getModuleFromPar<cModule>(par("macModule"), this);
                if (macModule) {
                    Ieee80211Mac *mac = check_and_cast<Ieee80211Mac *>(macModule);
                    if (mac->isApInAxMode())
                        mac->destroyStationQueueBank(sta->address);
                }
            }
            catch (const cException &e) {
                EV_DEBUG << "Could not get MAC module for queue bank destruction: " << e.what() << "\n";
            }
        }
        mib->bssAccessPointData.stations[sta->address] = Ieee80211Mib::NOT_AUTHENTICATED;
        mib->releaseAssociationId(sta->address);
        sta->authSeqExpected = 1;
    }

    // check authentication sequence number is OK
    if (frameAuthSeq != sta->authSeqExpected) {
        // wrong sequence number: send error and return
        EV << "Wrong sequence number, " << sta->authSeqExpected << " expected\n";
        const auto& body = makeShared<Ieee80211AuthenticationFrame>();
        body->setStatusCode(SC_AUTH_OUT_OF_SEQ);
        sendManagementFrame("Auth-ERROR", body, ST_AUTHENTICATION, header->getTransmitterAddress());
        delete packet;
        sta->authSeqExpected = 1; // go back to start square
        return;
    }

    // station is authenticated if it made it through the required number of steps
    bool isLast = (frameAuthSeq + 1 == numAuthSteps);

    // send OK response (we don't model the cryptography part, just assume
    // successful authentication every time)
    EV << "Sending Authentication frame, seqNum=" << (frameAuthSeq + 1) << "\n";
    const auto& body = makeShared<Ieee80211AuthenticationFrame>();
    body->setSequenceNumber(frameAuthSeq + 1);
    body->setStatusCode(SC_SUCCESSFUL);
    body->setIsLast(isLast);
    // TODO frame length could be increased to account for challenge text length etc.
    sendManagementFrame(isLast ? "Auth-OK" : "Auth", body, ST_AUTHENTICATION, header->getTransmitterAddress());

    delete packet;

    // update status
    if (isLast) {
        if (mib->bssAccessPointData.stations[sta->address] == Ieee80211Mib::ASSOCIATED) {
            sendDisAssocNotification(sta->address);

            // Destroy per-STA queue bank only for APs operating in ax mode.
            try {
                cModule *macModule = getModuleFromPar<cModule>(par("macModule"), this);
                if (macModule) {
                    Ieee80211Mac *mac = check_and_cast<Ieee80211Mac *>(macModule);
                    if (mac->isApInAxMode())
                        mac->destroyStationQueueBank(sta->address);
                }
            }
            catch (const cException &e) {
                EV_DEBUG << "Could not get MAC module for queue bank destruction: " << e.what() << "\n";
            }
        }
        mib->bssAccessPointData.stations[sta->address] = Ieee80211Mib::AUTHENTICATED; // TODO only when ACK of this frame arrives
        EV << "STA authenticated\n";
    }
    else {
        sta->authSeqExpected += 2;
        EV << "Expecting Authentication frame " << sta->authSeqExpected << "\n";
    }
}

void Ieee80211MgmtAp::handleDeauthenticationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    EV << "Processing Deauthentication frame\n";

    StaInfo *sta = lookupSenderSTA(header);
    delete packet;

    if (sta) {
        // mark STA as not authenticated; alternatively, it could also be removed from staList
        if (mib->bssAccessPointData.stations[sta->address] == Ieee80211Mib::ASSOCIATED) {
            sendDisAssocNotification(sta->address);
            
            // Destroy per-STA queue bank only for APs operating in ax mode.
            try {
                cModule *macModule = getModuleFromPar<cModule>(par("macModule"), this);
                if (macModule) {
                    Ieee80211Mac *mac = check_and_cast<Ieee80211Mac *>(macModule);
                    if (mac->isApInAxMode())
                        mac->destroyStationQueueBank(sta->address);
                }
            }
            catch (const cException &e) {
                EV_DEBUG << "Could not get MAC module for queue bank destruction: " << e.what() << "\n";
            }
        }
        mib->bssAccessPointData.stations[sta->address] = Ieee80211Mib::NOT_AUTHENTICATED;
        mib->removePeerHeCapabilities(sta->address);
        sta->authSeqExpected = 1;
    }
}

void Ieee80211MgmtAp::handleAssociationRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    EV << "Processing AssociationRequest frame\n";

    // "11.3.2 AP association procedures"
    StaInfo *sta = lookupSenderSTA(header);
    if (!sta || mib->bssAccessPointData.stations[sta->address] == Ieee80211Mib::NOT_AUTHENTICATED) {
        // STA not authenticated: send error and return
        const auto& body = makeShared<Ieee80211DeauthenticationFrame>();
        body->setReasonCode(RC_NONAUTH_ASS_REQUEST);
        sendManagementFrame("Deauth", body, ST_DEAUTHENTICATION, header->getTransmitterAddress());
        delete packet;
        return;
    }

    auto associationRequest = packet->peekData<Ieee80211AssociationRequestFrame>();
    mib->setStationTransmitPower(sta->address, associationRequest->getTransmitPowerDbm());
    if (associationRequest->getHeCapabilitiesPresent())
        mib->setPeerHeCapabilities(sta->address, makeHeCapabilities(associationRequest->getHeCapabilities()), mib->heOperation);
    else
        mib->removePeerHeCapabilities(sta->address);
    delete packet;

    // mark STA as associated
    if (mib->bssAccessPointData.stations[sta->address] != Ieee80211Mib::ASSOCIATED)
        sendAssocNotification(sta->address);
    mib->bssAccessPointData.stations[sta->address] = Ieee80211Mib::ASSOCIATED; // TODO this should only take place when MAC receives the ACK for the response

    // Create per-STA queue bank only for APs operating in ax mode.
    try {
        cModule *macModule = getModuleFromPar<cModule>(par("macModule"), this);
        if (macModule) {
            Ieee80211Mac *mac = check_and_cast<Ieee80211Mac *>(macModule);
            if (mac->isApInAxMode() && !mac->getStationQueueBank(sta->address))
                mac->createStationQueueBank(sta->address);
        }
    }
    catch (const cException &e) {
        EV_DEBUG << "Could not get MAC module for queue bank creation: " << e.what() << "\n";
    }

    // send OK response
    const auto& body = makeShared<Ieee80211AssociationResponseFrame>();
    body->setStatusCode(SC_SUCCESSFUL);
    body->setAid(mib->allocateAssociationId(sta->address));
    body->setSupportedRates(supportedRates);
    addApHeManagementElements(body, mib);
    body->setChunkLength(B(2 + 2 + 2 + body->getSupportedRates().numRates + 2) + getHeMgmtElementsLength(body));
    sendManagementFrame("AssocResp-OK", body, ST_ASSOCIATIONRESPONSE, sta->address);
}

void Ieee80211MgmtAp::handleAssociationResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtAp::handleReassociationRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    EV << "Processing ReassociationRequest frame\n";

    // "11.3.4 AP reassociation procedures" -- almost the same as AssociationRequest processing
    StaInfo *sta = lookupSenderSTA(header);
    if (!sta || mib->bssAccessPointData.stations[sta->address] == Ieee80211Mib::NOT_AUTHENTICATED) {
        // STA not authenticated: send error and return
        const auto& body = makeShared<Ieee80211DeauthenticationFrame>();
        body->setReasonCode(RC_NONAUTH_ASS_REQUEST);
        sendManagementFrame("Deauth", body, ST_DEAUTHENTICATION, header->getTransmitterAddress());
        delete packet;
        return;
    }

    auto reassociationRequest = packet->peekData<Ieee80211ReassociationRequestFrame>();
    if (reassociationRequest->getHeCapabilitiesPresent())
        mib->setPeerHeCapabilities(sta->address, makeHeCapabilities(reassociationRequest->getHeCapabilities()), mib->heOperation);
    else
        mib->removePeerHeCapabilities(sta->address);
    delete packet;

    // mark STA as associated
    mib->bssAccessPointData.stations[sta->address] = Ieee80211Mib::ASSOCIATED; // TODO this should only take place when MAC receives the ACK for the response

    // Create per-STA queue bank only for APs operating in ax mode.
    try {
        cModule *macModule = getModuleFromPar<cModule>(par("macModule"), this);
        if (macModule) {
            Ieee80211Mac *mac = check_and_cast<Ieee80211Mac *>(macModule);
            if (mac->isApInAxMode() && !mac->getStationQueueBank(sta->address))
                mac->createStationQueueBank(sta->address);
        }
    }
    catch (const cException &e) {
        EV_DEBUG << "Could not get MAC module for queue bank creation: " << e.what() << "\n";
    }

    // send OK response
    const auto& body = makeShared<Ieee80211ReassociationResponseFrame>();
    body->setStatusCode(SC_SUCCESSFUL);
    body->setAid(mib->allocateAssociationId(sta->address));
    body->setSupportedRates(supportedRates);
    addApHeManagementElements(body, mib);
    body->setChunkLength(B(2 + 2 + 2 + (2 + supportedRates.numRates)) + getHeMgmtElementsLength(body));
    sendManagementFrame("ReassocResp-OK", body, ST_REASSOCIATIONRESPONSE, sta->address);
}

void Ieee80211MgmtAp::handleReassociationResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtAp::handleDisassociationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    StaInfo *sta = lookupSenderSTA(header);
    delete packet;

    if (sta) {
        if (mib->bssAccessPointData.stations[sta->address] == Ieee80211Mib::ASSOCIATED)
            sendDisAssocNotification(sta->address);
        mib->bssAccessPointData.stations[sta->address] = Ieee80211Mib::AUTHENTICATED;
        mib->releaseAssociationId(sta->address);

        // Destroy per-STA queue bank only for APs operating in ax mode.
        try {
            cModule *macModule = getModuleFromPar<cModule>(par("macModule"), this);
            if (macModule) {
                Ieee80211Mac *mac = check_and_cast<Ieee80211Mac *>(macModule);
                if (mac->isApInAxMode())
                    mac->destroyStationQueueBank(sta->address);
            }
        }
        catch (const cException &e) {
            EV_DEBUG << "Could not get MAC module for queue bank destruction: " << e.what() << "\n";
        }
    }
}

void Ieee80211MgmtAp::handleBeaconFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtAp::handleProbeRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    EV << "Processing ProbeRequest frame\n";

    const auto& requestBody = packet->peekData<Ieee80211ProbeRequestFrame>();
    if (strcmp(requestBody->getSSID(), "") != 0 && strcmp(requestBody->getSSID(), ssid.c_str()) != 0) {
        EV << "SSID `" << requestBody->getSSID() << "' does not match, ignoring frame\n";
        dropManagementFrame(packet);
        return;
    }

    MacAddress staAddress = header->getTransmitterAddress();
    delete packet;

    EV << "Sending ProbeResponse frame\n";
    const auto& body = makeShared<Ieee80211ProbeResponseFrame>();
    body->setSSID(ssid.c_str());
    body->setSupportedRates(supportedRates);
    body->setBeaconInterval(beaconInterval);
    body->setChannelNumber(channelNumber);
    addApHeManagementElements(body, mib);
    body->setChunkLength(B(8 + 2 + 2 + (2 + ssid.length()) + (2 + supportedRates.numRates)) + getHeMgmtElementsLength(body));
    sendManagementFrame("ProbeResp", body, ST_PROBERESPONSE, staAddress);
}

void Ieee80211MgmtAp::handleProbeResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtAp::handleActionFrame(Packet *packet, const Ptr<const Ieee80211ActionFrame>& header)
{
    auto manager = dynamic_cast<ITwtManager *>(getModuleFromPar<cModule>(par("twtModule"), this));
    if (auto setup = dynamicPtrCast<const Ieee80211TwtSetupFrame>(header)) {
        if (!setup->getTwtRequest()) {
            dropManagementFrame(packet);
            return;
        }
        auto response = makeShared<Ieee80211TwtSetupFrame>();
        response->setDialogToken(setup->getDialogToken());
        response->setTwtRequest(false);
        TwtAgreement agreement = makeTwtAgreement(setup, header->getTransmitterAddress());
        bool peerSupportsTwt = false;
        if (auto it = mib->bssAccessPointData.advertisedHeCapabilities.find(agreement.peerAddress);
                it != mib->bssAccessPointData.advertisedHeCapabilities.end())
            peerSupportsTwt = it->second.twtRequester || (agreement.broadcast && it->second.broadcastTwt);

        bool accepted = manager != nullptr && manager->isEnabled() && mib->localHeCapabilities.twtResponder && peerSupportsTwt;
        if (agreement.broadcast) {
            TwtBroadcastSchedule schedule;
            accepted = accepted && manager->findBroadcastSchedule(agreement.broadcastId, schedule);
            if (accepted) {
                agreement = schedule;
                agreement.peerAddress = header->getTransmitterAddress();
                manager->addBroadcastMember(agreement.broadcastId, agreement.peerAddress);
            }
        }
        else {
            bool exactFeasible = agreement.active && agreement.wakeDuration <= agreement.wakeInterval;
            if (setup->getSetupCommand() == 0) { // Request: AP chooses its configured defaults.
                auto module = dynamic_cast<cModule *>(manager);
                agreement.wakeInterval = module->par("defaultWakeInterval");
                agreement.wakeDuration = module->par("defaultWakeDuration");
                agreement.nextWakeTime = simTime() + module->par("firstWakeOffset");
                agreement.persistence = module->par("defaultPersistence");
                agreement.implicit = true;
                agreement.active = true;
            }
            else if (!exactFeasible)
                accepted = false;
            if (accepted && !agreement.broadcast)
                manager->installAgreement(agreement);
        }
        response->setSetupCommand(accepted ? 3 : 6); // Accept / Reject
        setTwtSetupFrameFields(response, agreement);
        sendTwtActionFrame(accepted ? "TwtSetupAccept" : "TwtSetupReject", response, header->getTransmitterAddress());
        delete packet;
    }
    else if (auto teardown = dynamicPtrCast<const Ieee80211TwtTeardownFrame>(header)) {
        if (manager != nullptr) {
            if (teardown->getBroadcast())
                manager->removeBroadcastMember(teardown->getBroadcastId(), header->getTransmitterAddress());
            else
                manager->removeAgreement(header->getTransmitterAddress(), teardown->getFlowId(), false, 0);
        }
        delete packet;
    }
    else if (auto information = dynamicPtrCast<const Ieee80211TwtInformationFrame>(header)) {
        if (manager != nullptr && information->getNextWakeTimePresent())
            manager->updateNextWakeTime(header->getTransmitterAddress(), information->getFlowId(),
                    SimTime((int64_t)information->getNextWakeTime(), SIMTIME_US));
        delete packet;
    }
    else
        Ieee80211MgmtBase::handleActionFrame(packet, header);
}

void Ieee80211MgmtAp::sendAssocNotification(const MacAddress& addr)
{
    NotificationInfoSta notif;
    notif.setApAddress(mib->address);
    notif.setStaAddress(addr);
    emit(l2ApAssociatedSignal, &notif);
}

void Ieee80211MgmtAp::sendDisAssocNotification(const MacAddress& addr)
{
    NotificationInfoSta notif;
    notif.setApAddress(mib->address);
    notif.setStaAddress(addr);
    emit(l2ApDisassociatedSignal, &notif);
}

void Ieee80211MgmtAp::start()
{
    Ieee80211MgmtApBase::start();
    scheduleAfter(uniform(0, beaconInterval), beaconTimer);
}

void Ieee80211MgmtAp::stop()
{
    cancelEvent(beaconTimer);
    staList.clear();
    Ieee80211MgmtApBase::stop();
}

} // namespace ieee80211

} // namespace inet
