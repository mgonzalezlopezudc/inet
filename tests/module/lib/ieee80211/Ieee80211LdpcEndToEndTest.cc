//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "Ieee80211LdpcEndToEndTest.h"

#include <cmath>

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
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

Define_Module(DeterministicIeee80211FecSuccessModel);
Define_Module(Ieee80211LdpcEndToEndTest);

void DeterministicIeee80211FecSuccessModel::initialize(int stage)
{
    Module::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        targetDataSuccessRate = par("targetDataSuccessRate");
        minimumTargetUncodedBits = par("minimumTargetUncodedBits");
        auto expectedPhyFormat = par("expectedPhyFormat").stdstringValue();
        if (expectedPhyFormat == "HT")
            expectedVht = false;
        else if (expectedPhyFormat == "VHT_SU")
            expectedVht = true;
        else
            throw cRuntimeError("Unsupported deterministic IEEE 802.11 FEC PHY format: %s", expectedPhyFormat.c_str());
        if ((targetDataSuccessRate != 0 && targetDataSuccessRate != 1) || minimumTargetUncodedBits <= 0)
            throw cRuntimeError("Deterministic IEEE 802.11 FEC success model requires a binary endpoint and a positive target size");
    }
}

double DeterministicIeee80211FecSuccessModel::computeDataSuccessRate(const IIeee80211DataMode& mode,
        const Ieee80211DataEncodingPlan& plan, double snrDb) const
{
    if (mode.getFecType() != Ieee80211FecType::LDPC || plan.getFecType() != Ieee80211FecType::LDPC)
        throw cRuntimeError("Deterministic IEEE 802.11 LDPC endpoint was called for non-LDPC data");
    auto expectedPhyFormat = expectedVht ? Ieee80211PhyFormat::VHT_SU : Ieee80211PhyFormat::HT;
    if (plan.getPhyFormat() != expectedPhyFormat)
        throw cRuntimeError("Deterministic IEEE 802.11 FEC end-to-end test received an unexpected PHY format");
    if (!std::isfinite(snrDb))
        throw cRuntimeError("Deterministic IEEE 802.11 LDPC endpoint received a non-finite SNR");

    if (plan.getUncodedDataBits() >= minimumTargetUncodedBits) {
        targetEvaluationCount++;
        EV_INFO << "LDPC target endpoint evaluated: successRate=" << targetDataSuccessRate
                << " Npld=" << plan.getUncodedDataBits()
                << " NCW=" << plan.getNumberOfCodewords() << EV_ENDL;
        return targetDataSuccessRate;
    }

    // Association and other smaller management PPDUs must succeed so both
    // endpoint cases reach the fixed application Data frame.
    return 1;
}

void DeterministicIeee80211FecSuccessModel::finish()
{
    recordScalar("targetEvaluationCount", targetEvaluationCount);
    Module::finish();
}

void Ieee80211LdpcEndToEndTest::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        expectedSuccess = par("expectedSuccess");
        expectedAttempts = par("expectedAttempts");
        minimumTargetUncodedBits = par("minimumTargetUncodedBits");
        if (expectedAttempts <= 0 || minimumTargetUncodedBits <= 0)
            throw cRuntimeError("Invalid IEEE 802.11 LDPC end-to-end test expectations");
    }
    else if (stage == INITSTAGE_LAST) {
        stationRadio = getModuleByPath(par("stationRadioModule"));
        stationDcf = getModuleByPath(par("stationDcfModule"));
        stationRecovery = getModuleByPath(par("stationRecoveryModule"));
        serverApp = getModuleByPath(par("serverAppModule"));
        fecSuccessModel = dynamic_cast<DeterministicIeee80211FecSuccessModel *>(
                getModuleByPath(par("fecSuccessModelModule")));
        if (stationRadio == nullptr || stationDcf == nullptr || stationRecovery == nullptr ||
            serverApp == nullptr || fecSuccessModel == nullptr)
            throw cRuntimeError("Cannot resolve an IEEE 802.11 LDPC end-to-end test module");
        stationRadio->subscribe("transmissionStarted", this);
        stationDcf->subscribe("packetSentToPeer", this);
        stationDcf->subscribe("packetReceivedFromPeer", this);
        stationRecovery->subscribe("retryLimitReached", this);
        serverApp->subscribe("packetReceived", this);
    }
}

void Ieee80211LdpcEndToEndTest::handleMessage(cMessage *message)
{
    throw cRuntimeError("Ieee80211LdpcEndToEndTest does not process messages");
}

void Ieee80211LdpcEndToEndTest::receiveSignal(cComponent *source, simsignal_t signalID,
        cObject *object, cObject *details)
{
    Enter_Method("receiveSignal");
    if (source == stationRadio && !strcmp(getSignalName(signalID), "transmissionStarted")) {
        auto transmission = dynamic_cast<const Ieee80211Transmission *>(object);
        if (transmission != nullptr) {
            const auto *mode = transmission->getMode();
            auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(transmission->getPacket());
            auto htHeader = dynamicPtrCast<const Ieee80211HtPhyHeader>(phyHeader);
            if (htHeader != nullptr && htHeader->getLengthField().get<B>() * 8 + 16 >= minimumTargetUncodedBits) {
                auto plan = mode->getDataMode()->computeEncodingPlan(B(htHeader->getLengthField()));
                if (mode->getDataMode()->getFecType() != Ieee80211FecType::LDPC ||
                    plan.getFecType() != Ieee80211FecType::LDPC || plan.getPhyFormat() != Ieee80211PhyFormat::HT ||
                    !htHeader->getFecCoding() ||
                    transmission->getDataDuration() != plan.getNumberOfSymbols() * mode->getDataMode()->getSymbolInterval())
                    throw cRuntimeError("Target IEEE 802.11 HT transmission cannot be reconstructed as LDPC from HT-SIG and duration");
                if (transmission->getPacket()->findTag<Ieee80211DataEncodingPlanTag>() != nullptr)
                    throw cRuntimeError("Sender encoding-plan tag crossed the immutable transmission boundary");
                onAirLdpcDataCount++;
            }
        }
    }
    else if (source == stationDcf && !strcmp(getSignalName(signalID), "packetSentToPeer")) {
        auto packet = check_and_cast<const Packet *>(object);
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        if (dynamicPtrCast<const Ieee80211DataHeader>(header) != nullptr) {
            auto modeReq = const_cast<Packet *>(packet)->findTag<Ieee80211ModeReq>();
            if (modeReq != nullptr && modeReq->getMode()->getDataMode()->getFecType() == Ieee80211FecType::LDPC) {
                macLdpcDataCount++;
                if (header->getRetry())
                    retriedLdpcDataCount++;
                awaitingLdpcAck = true;
            }
        }
    }
    else if (source == stationDcf && !strcmp(getSignalName(signalID), "packetReceivedFromPeer")) {
        auto packet = check_and_cast<const Packet *>(object);
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        if (header->getType() == ST_ACK && awaitingLdpcAck) {
            ldpcAckCount++;
            awaitingLdpcAck = false;
        }
    }
    else if (source == stationRecovery && !strcmp(getSignalName(signalID), "retryLimitReached")) {
        auto packet = check_and_cast<const Packet *>(object);
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        if (dynamicPtrCast<const Ieee80211DataHeader>(header) != nullptr)
            retryLimitDropCount++;
    }
    else if (source == serverApp && !strcmp(getSignalName(signalID), "packetReceived"))
        deliveryCount++;
}

void Ieee80211LdpcEndToEndTest::finish()
{
    if (onAirLdpcDataCount != expectedAttempts || macLdpcDataCount != expectedAttempts ||
        retriedLdpcDataCount != expectedAttempts - 1)
        throw cRuntimeError("Unexpected IEEE 802.11 LDPC attempt counters: air=%d mac=%d retries=%d expected=%d",
                onAirLdpcDataCount, macLdpcDataCount, retriedLdpcDataCount, expectedAttempts);
    if (fecSuccessModel->getTargetEvaluationCount() < expectedAttempts)
        throw cRuntimeError("IEEE 802.11 LDPC endpoint was not evaluated for every target attempt");
    if (expectedSuccess) {
        if (ldpcAckCount != 1 || retryLimitDropCount != 0 || deliveryCount != 1)
            throw cRuntimeError("Unexpected successful IEEE 802.11 LDPC outcome: ACK=%d drops=%d deliveries=%d",
                    ldpcAckCount, retryLimitDropCount, deliveryCount);
    }
    else if (ldpcAckCount != 0 || retryLimitDropCount != 1 || deliveryCount != 0)
        throw cRuntimeError("Unexpected failed IEEE 802.11 LDPC outcome: ACK=%d drops=%d deliveries=%d",
                ldpcAckCount, retryLimitDropCount, deliveryCount);

    EV_INFO << "LDPC end-to-end verdict: success=" << expectedSuccess
            << " airData=" << onAirLdpcDataCount
            << " macData=" << macLdpcDataCount
            << " retries=" << retriedLdpcDataCount
            << " acks=" << ldpcAckCount
            << " retryDrops=" << retryLimitDropCount
            << " deliveries=" << deliveryCount
            << " targetEvaluations=" << fecSuccessModel->getTargetEvaluationCount() << EV_ENDL;
    recordScalar("onAirLdpcDataCount", onAirLdpcDataCount);
    recordScalar("macLdpcDataCount", macLdpcDataCount);
    recordScalar("retriedLdpcDataCount", retriedLdpcDataCount);
    recordScalar("ldpcAckCount", ldpcAckCount);
    recordScalar("retryLimitDropCount", retryLimitDropCount);
    recordScalar("deliveryCount", deliveryCount);
    SimpleModule::finish();
}

} // namespace inet
