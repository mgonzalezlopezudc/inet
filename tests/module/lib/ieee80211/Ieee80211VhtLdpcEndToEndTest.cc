//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "Ieee80211VhtLdpcEndToEndTest.h"

#include <memory>

#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DataEncodingPlan.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211DataEncodingPlanTag.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"

namespace inet {

using namespace ieee80211;
using namespace physicallayer;

Define_Module(RecordingIeee80211LdpcPerSuccessModel);
Define_Module(Ieee80211VhtLdpcEndToEndTest);

double RecordingIeee80211LdpcPerSuccessModel::computeDataSuccessRate(const IIeee80211DataMode& mode,
        const Ieee80211DataEncodingPlan& plan, double snrDb) const
{
    if (mode.getPhyFormat() == Ieee80211PhyFormat::VHT_SU &&
        mode.getFecType() == Ieee80211FecType::LDPC &&
        plan.getPhyFormat() == Ieee80211PhyFormat::VHT_SU &&
        plan.getFecType() == Ieee80211FecType::LDPC && mode.getMcsIndex() == 0) {
        dataEvaluationCount++;
        return Ieee80211LdpcPerSuccessModel::computeDataSuccessRate(mode, plan, snrDb);
    }
    // Capability and management exchanges can use other VHT LDPC modes in
    // this packet-level setup; the explicit test curve intentionally covers
    // only the application-data signature above.
    return 1.0;
}

void Ieee80211VhtLdpcEndToEndTest::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        stationAddress = MacAddress(par("stationAddress").stringValue());
        bssid = MacAddress(par("bssid").stringValue());
        expectedAssociationId = par("expectedAssociationId");
        expectedPartialAid = par("expectedPartialAid");
        if (expectedAssociationId < 1 || expectedAssociationId > 2007 ||
            expectedPartialAid < 0 || expectedPartialAid > 511)
            throw cRuntimeError("Invalid VHT LDPC end-to-end expectations");
    }
    else if (stage == INITSTAGE_LAST) {
        stationRadio = getModuleByPath(par("stationRadioModule"));
        stationDcf = getModuleByPath(par("stationDcfModule"));
        serverApp = getModuleByPath(par("serverAppModule"));
        fecSuccessModel = dynamic_cast<RecordingIeee80211LdpcPerSuccessModel *>(
                getModuleByPath(par("fecSuccessModelModule")));
        apMib = dynamic_cast<Ieee80211Mib *>(getModuleByPath(par("apMibModule")));
        if (stationRadio == nullptr || stationDcf == nullptr || serverApp == nullptr ||
            fecSuccessModel == nullptr || apMib == nullptr)
            throw cRuntimeError("Cannot resolve the VHT LDPC end-to-end test modules");
        stationRadio->subscribe("transmissionStarted", this);
        stationDcf->subscribe("packetSentToPeer", this);
        stationDcf->subscribe("packetReceivedFromPeer", this);
        serverApp->subscribe("packetReceived", this);
    }
}

void Ieee80211VhtLdpcEndToEndTest::handleMessage(cMessage *message)
{
    throw cRuntimeError("Ieee80211VhtLdpcEndToEndTest does not process messages");
}

void Ieee80211VhtLdpcEndToEndTest::receiveSignal(cComponent *source, simsignal_t signalID,
        cObject *object, cObject *details)
{
    Enter_Method("receiveSignal");
    if (source == stationRadio && !strcmp(getSignalName(signalID), "transmissionStarted")) {
        auto transmission = dynamic_cast<const Ieee80211Transmission *>(object);
        if (transmission == nullptr)
            return;
        const auto *mode = transmission->getMode();
        const auto *dataMode = mode->getDataMode();
        if (dataMode->getPhyFormat() != Ieee80211PhyFormat::VHT_SU ||
            dataMode->getFecType() != Ieee80211FecType::LDPC)
            return;
        auto packet = std::unique_ptr<Packet>(transmission->getPacket()->dup());
        Ieee80211Radio::popIeee80211PhyHeaderAtFront(packet.get());
        packet->popAtFront<Ieee80211MpduSubframeHeader>();
        auto macHeader = packet->peekAtFront<Ieee80211MacHeader>();
        if (dynamicPtrCast<const Ieee80211DataHeader>(macHeader) == nullptr)
            return;

        auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(transmission->getPacket());
        auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader);
        if (vhtHeader == nullptr || !vhtHeader->getCoding())
            throw cRuntimeError("VHT LDPC data transmission did not set the VHT-SIG-A Coding bit");
        auto symbolTicks = dataMode->getSymbolInterval().raw();
        auto dataTicks = transmission->getDataDuration().raw();
        if (symbolTicks <= 0 || dataTicks <= 0 || dataTicks % symbolTicks != 0)
            throw cRuntimeError("VHT LDPC data duration is not an integral number of OFDM symbols");
        int numberOfSymbols = dataTicks / symbolTicks;
        int initialNumberOfSymbols = numberOfSymbols - (vhtHeader->getLdpcExtraOfdmSymbol() ? 1 : 0);
        auto plan = Ieee80211LdpcPlanner::computeVhtSuFromReceivedSymbols(initialNumberOfSymbols,
                dataMode->getNumberOfCodedBitsPerSymbol(), dataMode->getNumberOfDataBitsPerSymbol(),
                dataMode->getCodeRate());
        if (plan.getNumberOfSymbols() != numberOfSymbols ||
            plan.getAdditionalCapacityApplied() != vhtHeader->getLdpcExtraOfdmSymbol())
            throw cRuntimeError("VHT LDPC plan cannot be reconstructed from VHT-SIG and duration");
        if (transmission->getPacket()->findTag<Ieee80211DataEncodingPlanTag>() != nullptr ||
            transmission->getPacket()->findTag<Ieee80211VhtApepReq>() != nullptr ||
            transmission->getPacket()->findTag<Ieee80211VhtSigAReq>() != nullptr)
            throw cRuntimeError("Local TX plan/APEP/SIG-A context crossed the immutable transmission boundary");
        if (vhtHeader->getBandwidth() != 0 || vhtHeader->getMcs() != 0 ||
            vhtHeader->getNumberOfSpaceTimeStreams() != 0 || vhtHeader->getGroupId() != 0 ||
            vhtHeader->getPartialAid() != expectedPartialAid ||
            vhtHeader->getLdpcExtraOfdmSymbol() != plan.getAdditionalCapacityApplied())
            throw cRuntimeError("VHT LDPC data transmission has unexpected VHT-SIG-A fields");
        onAirLdpcDataCount++;
    }
    else if (source == stationDcf && !strcmp(getSignalName(signalID), "packetSentToPeer")) {
        auto packet = check_and_cast<const Packet *>(object);
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        if (dynamicPtrCast<const Ieee80211DataHeader>(header) == nullptr)
            return;
        auto modeReq = const_cast<Packet *>(packet)->findTag<Ieee80211ModeReq>();
        if (modeReq == nullptr)
            throw cRuntimeError("VHT LDPC data packet reached DCF without a mode request");
        const auto *mode = modeReq->getMode();
        if (mode->getDataMode()->getPhyFormat() != Ieee80211PhyFormat::VHT_SU ||
            mode->getDataMode()->getFecType() != Ieee80211FecType::LDPC)
            throw cRuntimeError("Target VHT data packet was sent with a non-LDPC mode");
        macLdpcDataCount++;
        if (header->getRetry())
            retriedLdpcDataCount++;
        awaitingLdpcAck = true;
    }
    else if (source == stationDcf && !strcmp(getSignalName(signalID), "packetReceivedFromPeer")) {
        auto packet = check_and_cast<const Packet *>(object);
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        if (header->getType() == ST_ACK && awaitingLdpcAck) {
            ldpcAckCount++;
            awaitingLdpcAck = false;
        }
    }
    else if (source == serverApp && !strcmp(getSignalName(signalID), "packetReceived")) {
        auto packet = check_and_cast<const Packet *>(object);
        if (packet->findTag<Ieee80211DataEncodingPlanTag>() != nullptr ||
            packet->findTag<Ieee80211VhtApepReq>() != nullptr ||
            packet->findTag<Ieee80211VhtApepInd>() != nullptr ||
            packet->findTag<Ieee80211VhtSigAReq>() != nullptr)
            throw cRuntimeError("Internal LDPC PHY context leaked above the receiver MAC");
        deliveryCount++;
    }
}

void Ieee80211VhtLdpcEndToEndTest::finish()
{
    auto status = apMib->bssAccessPointData.stations.find(stationAddress);
    auto aid = apMib->bssAccessPointData.associationIds.find(stationAddress);
    if (status == apMib->bssAccessPointData.stations.end() || status->second != Ieee80211Mib::ASSOCIATED ||
        aid == apMib->bssAccessPointData.associationIds.end() || aid->second != expectedAssociationId)
        throw cRuntimeError("Station was not associated with the expected AID at the AP");
    if (apMib->bssData.bssid != bssid)
        throw cRuntimeError("AP MIB BSSID does not match the VHT LDPC test BSSID");
    if (onAirLdpcDataCount != 1 || macLdpcDataCount != 1 || retriedLdpcDataCount != 0 ||
        ldpcAckCount != 1 || deliveryCount != 1 || fecSuccessModel->getDataEvaluationCount() < 1)
        throw cRuntimeError("Unexpected VHT LDPC end-to-end counts: air=%d mac=%d retries=%d evaluations=%d ACK=%d deliveries=%d",
                onAirLdpcDataCount, macLdpcDataCount, retriedLdpcDataCount,
                fecSuccessModel->getDataEvaluationCount(), ldpcAckCount, deliveryCount);

    EV_INFO << "VHT LDPC end-to-end verdict: airData=" << onAirLdpcDataCount
            << " macData=" << macLdpcDataCount
            << " retries=" << retriedLdpcDataCount
            << " acks=" << ldpcAckCount
            << " deliveries=" << deliveryCount
            << " modelEvaluations=" << fecSuccessModel->getDataEvaluationCount() << EV_ENDL;
    recordScalar("onAirLdpcDataCount", onAirLdpcDataCount);
    recordScalar("macLdpcDataCount", macLdpcDataCount);
    recordScalar("retriedLdpcDataCount", retriedLdpcDataCount);
    recordScalar("ldpcAckCount", ldpcAckCount);
    recordScalar("deliveryCount", deliveryCount);
    recordScalar("ldpcDataModelEvaluationCount", fecSuccessModel->getDataEvaluationCount());
    SimpleModule::finish();
}

} // namespace inet
