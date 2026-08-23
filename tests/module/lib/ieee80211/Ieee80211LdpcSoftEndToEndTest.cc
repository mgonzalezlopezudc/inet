//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "Ieee80211LdpcSoftEndToEndTest.h"

#include <cstring>

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211DataEncodingPlanTag.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211LdpcSoftTransmissionModel.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigB.h"

namespace inet {

using namespace ieee80211;
using namespace physicallayer;

Define_Module(Ieee80211LdpcSoftEndToEndTest);

Ieee80211PeerLdpcStatus Ieee80211LdpcSoftEndToEndTest::getPeerLdpcStatus(
        const MacAddress& peer) const
{
    Ieee80211PeerLdpcStatus status;
    status.htRxLdpc = Ieee80211CapabilityStatus::SUPPORTED;
    status.vhtRxLdpc = Ieee80211CapabilityStatus::SUPPORTED;
    status.htRxMcsSetKnown = true;
    status.htRxMcsSet.fill(0xff);
    status.maximumHtRxBandwidthMhz = 20;
    status.vhtRxMcsMapKnown = true;
    status.vhtRxMcsMap = 0xfffe; // NSS1 supports MCS0-9; all other NSS unsupported.
    status.vhtTxMcsMapKnown = true;
    status.vhtTxMcsMap = 0xfffe;
    status.maximumVhtRxBandwidthMhz = 20;
    return status;
}

Ieee80211IntendedReceiverSet Ieee80211LdpcSoftEndToEndTest::resolveIntendedReceivers(
        const MacAddress& receiverAddress) const
{
    return receiverAddress.isMulticast() ? Ieee80211IntendedReceiverSet{false, {}} :
            Ieee80211IntendedReceiverSet{true, {receiverAddress}};
}

Ieee80211VhtSigAParameters Ieee80211LdpcSoftEndToEndTest::getVhtSigAParameters(
        const MacAddress& receiverAddress) const
{
    return {true, 63, 0};
}

void Ieee80211LdpcSoftEndToEndTest::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        expectedExact = par("expectedExact");
        expectedSuccess = par("expectedSuccess");
        expectedAttempts = par("expectedAttempts");
        minimumTargetPsduOctets = par("minimumTargetPsduOctets");
        auto expectedPhyFormat = par("expectedPhyFormat").stdstringValue();
        if (expectedPhyFormat == "HT")
            expectedVht = false;
        else if (expectedPhyFormat == "VHT_SU")
            expectedVht = true;
        else
            throw cRuntimeError("Unknown expected IEEE 802.11 soft-boundary PHY format: %s",
                    expectedPhyFormat.c_str());
        if (expectedAttempts <= 0 || minimumTargetPsduOctets <= 0)
            throw cRuntimeError("Invalid IEEE 802.11 soft-boundary test expectations");
    }
    else if (stage == INITSTAGE_LAST) {
        stationRadio = getModuleByPath(par("stationRadioModule"));
        stationDcf = getModuleByPath(par("stationDcfModule"));
        stationRecovery = getModuleByPath(par("stationRecoveryModule"));
        serverApp = getModuleByPath(par("serverAppModule"));
        stationTransmitter = getModuleByPath(par("stationTransmitterModule"));
        apReceiver = getModuleByPath(par("apReceiverModule"));
        if (stationRadio == nullptr || stationDcf == nullptr || stationRecovery == nullptr ||
            serverApp == nullptr || stationTransmitter == nullptr || apReceiver == nullptr)
            throw cRuntimeError("Cannot resolve an IEEE 802.11 soft-boundary test module");
        stationRadio->subscribe("transmissionStarted", this);
        stationDcf->subscribe("packetSentToPeer", this);
        stationDcf->subscribe("packetReceivedFromPeer", this);
        stationRecovery->subscribe("retryLimitReached", this);
        serverApp->subscribe("packetReceived", this);
        stationTransmitter->subscribe("ldpcDataEncoded", this);
        apReceiver->subscribe("ldpcDataDecodeAttempted", this);
        apReceiver->subscribe("ldpcDataDecodeSucceeded", this);
        apReceiver->subscribe("ldpcDataDecodeIterations", this);
    }
}

void Ieee80211LdpcSoftEndToEndTest::receiveSignal(cComponent *source,
        simsignal_t signalID, long value, cObject *details)
{
    Enter_Method("receiveSignal");
    const char *signalName = getSignalName(signalID);
    if (source == stationTransmitter && !strcmp(signalName, "ldpcDataEncoded"))
        encodeInvocations++;
    else if (source == apReceiver && !strcmp(signalName, "ldpcDataDecodeAttempted"))
        decodeInvocations++;
    else if (source == apReceiver && !strcmp(signalName, "ldpcDataDecodeSucceeded"))
        decodeConvergences++;
    else if (source == apReceiver && !strcmp(signalName, "ldpcDataDecodeIterations"))
        decodeIterations += value;
}

void Ieee80211LdpcSoftEndToEndTest::handleMessage(cMessage *message)
{
    throw cRuntimeError("Ieee80211LdpcSoftEndToEndTest does not process messages");
}

void Ieee80211LdpcSoftEndToEndTest::receiveSignal(cComponent *source, simsignal_t signalID,
        cObject *object, cObject *details)
{
    Enter_Method("receiveSignal");
    if (source == stationRadio && !strcmp(getSignalName(signalID), "transmissionStarted")) {
        auto transmission = dynamic_cast<const Ieee80211Transmission *>(object);
        if (transmission == nullptr || transmission->getPacket() == nullptr)
            return;
        auto phyHeader = Ieee80211Radio::peekIeee80211PhyHeaderAtFront(transmission->getPacket());
        int psduOctets = 0;
        bool isLdpc = false;
        bool isVht = false;
        if (auto htHeader = dynamicPtrCast<const Ieee80211HtPhyHeader>(phyHeader)) {
            psduOctets = htHeader->getLengthField().get<B>();
            isLdpc = htHeader->getFecCoding();
        }
        else if (auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader)) {
            psduOctets = decodeVhtSuSigBLength(vhtHeader->getVhtSigBLength()).get<B>();
            if (vhtHeader->getLengthField() != B(psduOctets))
                throw cRuntimeError("VHT transmission retained an exact sender APEP length outside VHT-SIG-B");
            isLdpc = vhtHeader->getCoding();
            isVht = true;
        }
        if (psduOctets < minimumTargetPsduOctets)
            return;
        if (isVht != expectedVht || isLdpc != expectedExact)
            throw cRuntimeError("Target IEEE 802.11 transmission has unexpected PHY format or FEC signaling");

        auto signalModel = dynamic_cast<const Ieee80211LdpcSoftTransmissionModel *>(transmission->getBitModel());
        if (expectedExact) {
            if (signalModel == nullptr)
                throw cRuntimeError("Exact IEEE 802.11 LDPC transmission did not use the sender-plan-free soft model");
            if (transmission->getPacket()->findTag<Ieee80211VhtApepReq>() != nullptr)
                throw cRuntimeError("Local VHT APEP_LENGTH TXVECTOR request crossed the transmission boundary");
            if (transmission->getPacket()->findTag<Ieee80211DataEncodingPlanTag>() != nullptr)
                throw cRuntimeError("Sender LDPC encoding-plan tag crossed the transmission boundary");
            if (signalModel->getAllBits() == nullptr || signalModel->getAllBits()->getSize() == 0 ||
                signalModel->getMappedData().blocks.empty() || signalModel->getSymbols().empty() ||
                signalModel->getMappedData().blocks.size() != signalModel->getSymbols().size())
                throw cRuntimeError("Exact IEEE 802.11 LDPC transmission has an empty or inconsistent symbol representation");

            if (isVht) {
                // Poison the already-rounded nonserialized base property
                // after the soft symbols have been constructed. VHT-SIG-A/B,
                // duration, and observations stay unchanged. Successful
                // delivery proves the receiver does not use this property.
                auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader);
                auto poisonedHeader = makeShared<Ieee80211VhtPhyHeader>(*vhtHeader);
                poisonedHeader->setLengthField(B(1));
                auto mutablePacket = const_cast<Packet *>(transmission->getPacket());
                mutablePacket->replaceAt(poisonedHeader, b(0), poisonedHeader->getChunkLength(),
                        Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE |
                        Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
            }
        }
        else if (signalModel != nullptr)
            throw cRuntimeError("BCC transmission unexpectedly entered the exact IEEE 802.11 LDPC path");
        onAirDataCount++;
    }
    else if (source == stationDcf && !strcmp(getSignalName(signalID), "packetSentToPeer")) {
        auto packet = check_and_cast<const Packet *>(object);
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        if (dynamicPtrCast<const Ieee80211DataHeader>(header) != nullptr) {
            auto modeReq = const_cast<Packet *>(packet)->findTag<Ieee80211ModeReq>();
            if (modeReq == nullptr)
                throw cRuntimeError("Target IEEE 802.11 Data frame has no selected mode");
            bool isLdpc = modeReq->getMode()->getDataMode()->getFecType() == Ieee80211FecType::LDPC;
            if (isLdpc != expectedExact)
                throw cRuntimeError("Target IEEE 802.11 Data frame selected the wrong FEC type");
            macDataCount++;
            if (header->getRetry())
                retriedDataCount++;
            awaitingAck = true;
        }
    }
    else if (source == stationDcf && !strcmp(getSignalName(signalID), "packetReceivedFromPeer")) {
        auto packet = check_and_cast<const Packet *>(object);
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        if (header->getType() == ST_ACK && awaitingAck) {
            ackCount++;
            awaitingAck = false;
        }
    }
    else if (source == stationRecovery && !strcmp(getSignalName(signalID), "retryLimitReached")) {
        auto packet = check_and_cast<const Packet *>(object);
        if (dynamicPtrCast<const Ieee80211DataHeader>(packet->peekAtFront<Ieee80211MacHeader>()) != nullptr)
            retryLimitDropCount++;
    }
    else if (source == serverApp && !strcmp(getSignalName(signalID), "packetReceived")) {
        auto packet = check_and_cast<const Packet *>(object);
        if (packet->findTag<Ieee80211DataEncodingPlanTag>() != nullptr ||
            packet->findTag<Ieee80211VhtApepInd>() != nullptr ||
            packet->findTag<Ieee80211VhtApepReq>() != nullptr ||
            packet->findTag<Ieee80211VhtSigAReq>() != nullptr)
            throw cRuntimeError("Internal IEEE 802.11 PHY/MAC handoff tag leaked to the application");
        deliveryCount++;
    }
}

void Ieee80211LdpcSoftEndToEndTest::finish()
{
    if (onAirDataCount != expectedAttempts || macDataCount != expectedAttempts ||
        retriedDataCount != expectedAttempts - 1)
        throw cRuntimeError("Unexpected IEEE 802.11 soft-boundary attempts: air=%d mac=%d retries=%d expected=%d",
                onAirDataCount, macDataCount, retriedDataCount, expectedAttempts);
    if (expectedSuccess) {
        if (ackCount != 1 || retryLimitDropCount != 0 || deliveryCount != 1)
            throw cRuntimeError("Unexpected successful IEEE 802.11 soft-boundary outcome: ACK=%d drops=%d deliveries=%d",
                    ackCount, retryLimitDropCount, deliveryCount);
    }
    else if (ackCount != 0 || retryLimitDropCount != 1 || deliveryCount != 0)
        throw cRuntimeError("Unexpected failed IEEE 802.11 soft-boundary outcome: ACK=%d drops=%d deliveries=%d",
                ackCount, retryLimitDropCount, deliveryCount);

    if (expectedExact) {
        if (encodeInvocations != expectedAttempts || decodeInvocations != expectedAttempts ||
            decodeConvergences != (expectedSuccess ? 1 : 0))
            throw cRuntimeError("Unexpected exact LDPC boundary counters: encode=%d decode=%d converged=%d",
                    encodeInvocations, decodeInvocations, decodeConvergences);
    }
    else if (encodeInvocations != 0 || decodeInvocations != 0 || decodeConvergences != 0)
        throw cRuntimeError("BCC fallback invoked the exact LDPC boundary");

    EV_INFO << "LDPC soft-boundary verdict: exact=" << expectedExact
            << " success=" << expectedSuccess
            << " vht=" << expectedVht
            << " airData=" << onAirDataCount
            << " macData=" << macDataCount
            << " retries=" << retriedDataCount
            << " acks=" << ackCount
            << " retryDrops=" << retryLimitDropCount
            << " deliveries=" << deliveryCount
            << " encodes=" << encodeInvocations
            << " decodes=" << decodeInvocations
            << " convergences=" << decodeConvergences
            << " iterations=" << decodeIterations << EV_ENDL;
    SimpleModule::finish();
}

} // namespace inet
