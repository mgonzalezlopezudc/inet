//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "Ieee80211VhtSigAEndToEndTest.h"

#include <memory>

#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mgmt/contract/IIeee80211PeerCapabilities.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211DataEncodingPlanTag.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

namespace inet {

using namespace ieee80211;
using namespace physicallayer;

Define_Module(Ieee80211VhtSigAEndToEndTest);

static IIeee80211PeerCapabilities *resolveConfiguredPeerCapabilities(cModule *radio)
{
    auto interface = radio->getParentModule();
    auto mac = interface->getSubmodule("mac");
    auto dcf = mac == nullptr ? nullptr : mac->getSubmodule("dcf");
    auto rateSelection = dcf == nullptr ? nullptr : dcf->getSubmodule("rateSelection");
    if (rateSelection == nullptr)
        throw cRuntimeError("Cannot resolve DCF rate selection from radio %s", radio->getFullPath().c_str());
    return findModuleFromPar<IIeee80211PeerCapabilities>(rateSelection->par("peerCapabilitiesModule"), rateSelection);
}

void Ieee80211VhtSigAEndToEndTest::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        stationAddress = MacAddress(par("stationAddress").stringValue());
        bssid = MacAddress(par("bssid").stringValue());
        expectedAssociationId = par("expectedAssociationId");
        expectedStaToApPartialAid = par("expectedStaToApPartialAid");
        expectedApToStaPartialAid = par("expectedApToStaPartialAid");
        expectedLdpc = par("expectedLdpc");
        expectedAdhoc = par("expectedAdhoc");
        verifyMissingAssociationIdInvariant = par("verifyMissingAssociationIdInvariant");
        if ((!expectedAdhoc && (expectedAssociationId < 1 || expectedAssociationId > 2007)) ||
            expectedStaToApPartialAid < 0 || expectedStaToApPartialAid > 511 ||
            expectedApToStaPartialAid < 0 || expectedApToStaPartialAid > 511)
            throw cRuntimeError("Invalid VHT-SIG-A end-to-end expectations");
    }
    else if (stage == INITSTAGE_LAST) {
        stationRadio = getModuleByPath(par("stationRadioModule"));
        apRadio = getModuleByPath(par("apRadioModule"));
        uplinkSink = getModuleByPath(par("uplinkSinkModule"));
        downlinkSink = getModuleByPath(par("downlinkSinkModule"));
        apMib = dynamic_cast<Ieee80211Mib *>(getModuleByPath(par("apMibModule")));
        if (stationRadio == nullptr || apRadio == nullptr || uplinkSink == nullptr ||
            downlinkSink == nullptr || (!expectedAdhoc && apMib == nullptr))
            throw cRuntimeError("Cannot resolve a VHT-SIG-A end-to-end test module");
        stationRadio->subscribe("transmissionStarted", this);
        apRadio->subscribe("transmissionStarted", this);
        uplinkSink->subscribe("packetReceived", this);
        downlinkSink->subscribe("packetReceived", this);
        stationPeerCapabilities = resolveConfiguredPeerCapabilities(stationRadio);
        apPeerCapabilities = resolveConfiguredPeerCapabilities(apRadio);
        if (stationPeerCapabilities == nullptr || apPeerCapabilities == nullptr)
            throw cRuntimeError("VHT-SIG-A test requires management-owned peer capabilities at both rate selectors");

        if (verifyMissingAssociationIdInvariant) {
            if (expectedAdhoc)
                throw cRuntimeError("Missing-association-ID invariant is only valid for an AP management context");
            auto aid = apMib->bssAccessPointData.associationIds.find(stationAddress);
            if (aid == apMib->bssAccessPointData.associationIds.end())
                throw cRuntimeError("VHT-SIG-A invariant test requires an AP-owned association ID");
            auto savedAid = aid->second;
            apMib->bssAccessPointData.associationIds.erase(aid);
            bool rejected = false;
            try {
                apPeerCapabilities->getVhtSigAParameters(stationAddress);
            }
            catch (const cRuntimeError&) {
                rejected = true;
            }
            catch (...) {
                apMib->bssAccessPointData.associationIds[stationAddress] = savedAid;
                throw;
            }
            apMib->bssAccessPointData.associationIds[stationAddress] = savedAid;
            if (!rejected)
                throw cRuntimeError("Associated station without an AID was accepted by AP VHT-SIG-A parameters");
        }
    }
}

void Ieee80211VhtSigAEndToEndTest::handleMessage(cMessage *message)
{
    throw cRuntimeError("Ieee80211VhtSigAEndToEndTest does not process messages");
}

void Ieee80211VhtSigAEndToEndTest::receiveSignal(cComponent *source, simsignal_t signalID,
        cObject *object, cObject *details)
{
    Enter_Method("receiveSignal");
    if ((source == stationRadio || source == apRadio) && !strcmp(getSignalName(signalID), "transmissionStarted")) {
        auto transmission = dynamic_cast<const Ieee80211Transmission *>(object);
        if (transmission == nullptr)
            return;

        auto packet = std::unique_ptr<Packet>(transmission->getPacket()->dup());
        auto phyHeader = Ieee80211Radio::popIeee80211PhyHeaderAtFront(packet.get());
        const auto *dataMode = transmission->getMode()->getDataMode();
        if (dataMode->getPhyFormat() == Ieee80211PhyFormat::VHT_SU &&
            dataMode->getFecType() == Ieee80211FecType::LDPC)
            packet->popAtFront<Ieee80211MpduSubframeHeader>();
        auto macHeader = packet->peekAtFront<Ieee80211MacHeader>();
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(macHeader);
        if (dataHeader == nullptr)
            return;

        auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader);
        if (vhtHeader == nullptr)
            throw cRuntimeError("Application Data frame was not transmitted as a VHT-SU PPDU");
        if (transmission->getPacket()->findTag<Ieee80211VhtSigAReq>() != nullptr)
            throw cRuntimeError("MAC-to-PHY VHT-SIG-A request crossed the immutable transmission boundary");
        if (dataMode->getPhyFormat() != Ieee80211PhyFormat::VHT_SU ||
            dataMode->getFecType() != (expectedLdpc ? Ieee80211FecType::LDPC : Ieee80211FecType::BCC) ||
            dataMode->getBandwidth() != MHz(20) || dataMode->getNumberOfSpatialStreams() != 1 ||
            dataMode->getMcsIndex() != 0)
            throw cRuntimeError("Application Data frame did not use the fixed VHT MCS 0, 20 MHz, one-stream expected FEC mode");
        if (transmission->getPacket()->findTag<Ieee80211DataEncodingPlanTag>() != nullptr)
            throw cRuntimeError("Sender encoding-plan tag crossed the immutable transmission boundary");
        if (vhtHeader->getBandwidth() != 0 || !vhtHeader->getReserved1() || vhtHeader->getStbc() ||
            vhtHeader->getNumberOfSpaceTimeStreams() != 0 || !vhtHeader->getTxopPsNotAllowed() ||
            !vhtHeader->getReserved2() || vhtHeader->getShortGi() ||
            vhtHeader->getShortGiNsymDisambiguation() || vhtHeader->getCoding() != expectedLdpc ||
            vhtHeader->getMcs() != 0 ||
            vhtHeader->getBeamformed() || !vhtHeader->getReserved3())
            throw cRuntimeError("Application Data frame has unexpected fixed VHT-SIG-A fields");

        if (expectedAdhoc) {
            auto peerCapabilities = source == stationRadio ? stationPeerCapabilities : apPeerCapabilities;
            auto parameters = peerCapabilities->getVhtSigAParameters(source == stationRadio ? bssid : stationAddress);
            if (!parameters.known || parameters.groupId != 63 || parameters.partialAid != 0)
                throw cRuntimeError("Ad-hoc management returned unexpected VHT-SIG-A parameters: known=%d GID=%u PAID=%u",
                        parameters.known, parameters.groupId, parameters.partialAid);
            auto expectedReceiver = source == stationRadio ? bssid : stationAddress;
            auto expectedTransmitter = source == stationRadio ? stationAddress : bssid;
            if (dataHeader->getReceiverAddress() != expectedReceiver ||
                dataHeader->getTransmitterAddress() != expectedTransmitter ||
                dataHeader->getToDS() || dataHeader->getFromDS())
                throw cRuntimeError("Unexpected ad-hoc VHT data MAC addressing");
            if (vhtHeader->getGroupId() != 63 || vhtHeader->getPartialAid() != 0)
                throw cRuntimeError("Unexpected ad-hoc VHT-SIG-A GID/PAID: header=%u/%u",
                        vhtHeader->getGroupId(), vhtHeader->getPartialAid());
            if (source == stationRadio)
                staToApTransmissionCount++;
            else
                apToStaTransmissionCount++;
        }
        else if (source == stationRadio) {
            auto parameters = stationPeerCapabilities->getVhtSigAParameters(bssid);
            if (!parameters.known || parameters.groupId != 0 || parameters.partialAid != expectedStaToApPartialAid)
                throw cRuntimeError("Station management returned unexpected VHT-SIG-A parameters: known=%d GID=%u PAID=%u",
                        parameters.known, parameters.groupId, parameters.partialAid);
            if (dataHeader->getReceiverAddress() != bssid || dataHeader->getTransmitterAddress() != stationAddress ||
                !dataHeader->getToDS() || dataHeader->getFromDS())
                throw cRuntimeError("Unexpected infrastructure STA-to-AP MAC addressing");
            if (vhtHeader->getGroupId() != 0 || vhtHeader->getPartialAid() != expectedStaToApPartialAid)
                throw cRuntimeError("Unexpected STA-to-AP VHT-SIG-A: GID=%u PAID=%u expected GID=0 PAID=%d",
                        vhtHeader->getGroupId(), vhtHeader->getPartialAid(), expectedStaToApPartialAid);
            staToApTransmissionCount++;
        }
        else {
            auto parameters = apPeerCapabilities->getVhtSigAParameters(stationAddress);
            if (!parameters.known || parameters.groupId != 63 || parameters.partialAid != expectedApToStaPartialAid)
                throw cRuntimeError("AP management returned unexpected VHT-SIG-A parameters: known=%d GID=%u PAID=%u",
                        parameters.known, parameters.groupId, parameters.partialAid);
            if (dataHeader->getReceiverAddress() != stationAddress || dataHeader->getTransmitterAddress() != bssid ||
                dataHeader->getToDS() || !dataHeader->getFromDS())
                throw cRuntimeError("Unexpected infrastructure AP-to-STA MAC addressing");
            if (vhtHeader->getGroupId() != 63 || vhtHeader->getPartialAid() != expectedApToStaPartialAid)
                throw cRuntimeError("Unexpected AP-to-STA VHT-SIG-A: GID=%u PAID=%u expected GID=63 PAID=%d",
                        vhtHeader->getGroupId(), vhtHeader->getPartialAid(), expectedApToStaPartialAid);
            apToStaTransmissionCount++;
        }
    }
    else if (source == uplinkSink && !strcmp(getSignalName(signalID), "packetReceived"))
        uplinkDeliveryCount++;
    else if (source == downlinkSink && !strcmp(getSignalName(signalID), "packetReceived"))
        downlinkDeliveryCount++;
}

void Ieee80211VhtSigAEndToEndTest::finish()
{
    int aidValue = 0;
    if (!expectedAdhoc) {
        auto status = apMib->bssAccessPointData.stations.find(stationAddress);
        auto aid = apMib->bssAccessPointData.associationIds.find(stationAddress);
        if (status == apMib->bssAccessPointData.stations.end() || status->second != Ieee80211Mib::ASSOCIATED ||
            aid == apMib->bssAccessPointData.associationIds.end() || aid->second != expectedAssociationId)
            throw cRuntimeError("Station was not associated with the expected AID at the AP");
        if (apMib->bssData.bssid != bssid)
            throw cRuntimeError("AP MIB BSSID does not match the VHT-SIG-A test BSSID");
        aidValue = aid->second;
    }
    if (staToApTransmissionCount != 1 || apToStaTransmissionCount != 1 ||
        uplinkDeliveryCount != 1 || downlinkDeliveryCount != 1)
        throw cRuntimeError("Unexpected VHT-SIG-A end-to-end counts: STA-to-AP=%d AP-to-STA=%d uplink=%d downlink=%d",
                staToApTransmissionCount, apToStaTransmissionCount, uplinkDeliveryCount, downlinkDeliveryCount);

    EV_INFO << "VHT-SIG-A end-to-end verdict: staToAp=" << staToApTransmissionCount
            << " staToApGid=" << (expectedAdhoc ? 63 : 0) << " staToApPaid=" << expectedStaToApPartialAid
            << " apToSta=" << apToStaTransmissionCount
            << " apToStaGid=63 apToStaPaid=" << expectedApToStaPartialAid
            << " aid=" << aidValue
            << " uplinkDeliveries=" << uplinkDeliveryCount
            << " downlinkDeliveries=" << downlinkDeliveryCount << EV_ENDL;
    recordScalar("staToApTransmissionCount", staToApTransmissionCount);
    recordScalar("apToStaTransmissionCount", apToStaTransmissionCount);
    recordScalar("uplinkDeliveryCount", uplinkDeliveryCount);
    recordScalar("downlinkDeliveryCount", downlinkDeliveryCount);
    SimpleModule::finish();
}

} // namespace inet
