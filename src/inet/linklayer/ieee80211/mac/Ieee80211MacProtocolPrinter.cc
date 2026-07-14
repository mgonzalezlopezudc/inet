//
// Copyright (C) 2018 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/Ieee80211MacProtocolPrinter.h"

#include <algorithm>
#include <sstream>

#include "inet/common/packet/printer/PacketPrinter.h"
#include "inet/common/packet/printer/ProtocolPrinterRegistry.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {
namespace ieee80211 {

Register_Protocol_Printer(&Protocol::ieee80211Mac, Ieee80211MacProtocolPrinter);

namespace {

const char *frameTypeName(Ieee80211FrameType type)
{
    switch (type) {
        case ST_ASSOCIATIONREQUEST: return "AssocReq";
        case ST_ASSOCIATIONRESPONSE: return "AssocResp";
        case ST_REASSOCIATIONREQUEST: return "ReassocReq";
        case ST_REASSOCIATIONRESPONSE: return "ReassocResp";
        case ST_PROBEREQUEST: return "ProbeRequest";
        case ST_PROBERESPONSE: return "ProbeResponse";
        case ST_BEACON: return "Beacon";
        case ST_ATIM: return "Atim";
        case ST_DISASSOCIATION: return "Disassoc";
        case ST_AUTHENTICATION: return "Auth";
        case ST_DEAUTHENTICATION: return "Deauth";
        case ST_ACTION: return "Action";
        case ST_NOACKACTION: return "Noackaction";
        case ST_PSPOLL: return "PsPoll";
        case ST_RTS: return "RTS";
        case ST_CTS: return "CTS";
        case ST_ACK: return "ACK";
        case ST_BLOCKACK_REQ: return "BlockAckReq";
        case ST_BLOCKACK: return "BlockAck";
        case ST_TRIGGER: return "Trigger";
        case ST_DATA: return "DATA";
        case ST_DATA_WITH_QOS: return "QoS DATA";
        case ST_QOS_NULL: return "QoS Null";
        case ST_LBMS_REQUEST: return "LbmsReq";
        case ST_LBMS_REPORT: return "LbmsReport";
        default: return nullptr;
    }
}

const char *ackPolicyName(AckPolicy ackPolicy)
{
    switch (ackPolicy) {
        case NORMAL_ACK: return "normal";
        case NO_ACK: return "no-ack";
        case NO_EXPLICIT_ACK: return "no-explicit-ack";
        case BLOCK_ACK: return "block-ack";
        default: return "unknown";
    }
}

const char *triggerTypeName(uint8_t triggerType)
{
    switch (triggerType) {
        case 0: return "Basic";
        case 1: return "BFRP";
        case 2: return "MU-BAR";
        case 4: return "BSRP";
        default: return "Trigger";
    }
}

void printSequenceNumber(std::ostream& stream, const SequenceNumberCyclic& sequenceNumber)
{
    if (sequenceNumber.isValid())
        stream << sequenceNumber.get();
    else
        stream << "?";
}

void printDataOrMgmtInfo(std::ostream& stream, const Ieee80211DataOrMgmtHeader& header)
{
    stream << " address3=" << header.getAddress3()
           << " seq=";
    printSequenceNumber(stream, header.getSequenceNumber());
    stream << " frag=" << header.getFragmentNumber();
}

void printFrameFlags(std::ostream& stream, const Ieee80211MacHeader& header)
{
    if (header.getToDS())
        stream << " ToDS";
    if (header.getFromDS())
        stream << " FromDS";
    if (header.getRetry())
        stream << " retry";
    if (header.getMoreFragments())
        stream << " more-fragments";
    if (header.getMoreData())
        stream << " more-data";
    if (header.getProtectedFrame())
        stream << " protected";
    if (header.getOrder())
        stream << " order";
    if (header.getDurationField() >= 0)
        stream << " duration=" << header.getDurationField();
    if (header.getAID() >= 0)
        stream << " AID=" << header.getAID();
}

void printTriggerInfo(std::ostream& stream, const Ieee80211TriggerFrame& trigger)
{
    constexpr size_t maxPrintedUsers = 6;
    auto usersCount = trigger.getUsersArraySize();
    stream << " triggerType=" << triggerTypeName(trigger.getTriggerType())
           << " id=" << trigger.getTriggerId()
           << " users=" << usersCount;
    if (usersCount > 0)
        stream << " [";
    auto shownUsers = std::min(maxPrintedUsers, usersCount);
    for (size_t i = 0; i < shownUsers; i++) {
        if (i != 0)
            stream << "; ";
        const auto& user = trigger.getUsers(i);
        if (user.randomAccess)
            stream << "RA";
        else
            stream << "AID" << user.aid;
        stream << " RU" << user.ruIndex;
        if (trigger.getTriggerType() == 0)
            stream << " limit" << static_cast<int>(user.tidAggregationLimit)
                   << " preferredACI" << static_cast<int>(user.preferredAc);
        else if (trigger.getTriggerType() == 2)
            stream << " TID" << static_cast<int>(user.tid);
        stream << " MCS" << static_cast<int>(user.mcs);
    }
    if (usersCount > maxPrintedUsers)
        stream << "; +" << (usersCount - maxPrintedUsers) << " more";
    if (usersCount > 0)
        stream << "]";
}

void printBlockAckReqInfo(std::ostream& stream, const Ieee80211BlockAckReq& blockAckReq)
{
    stream << " policy=" << (blockAckReq.getBarAckPolicy() ? "no-ack" : "normal");
    if (auto basicBlockAckReq = dynamic_cast<const Ieee80211BasicBlockAckReq *>(&blockAckReq)) {
        stream << " basic tid=" << basicBlockAckReq->getTidInfo() << " start=";
        printSequenceNumber(stream, basicBlockAckReq->getStartingSequenceNumber());
    }
    else if (auto compressedBlockAckReq = dynamic_cast<const Ieee80211CompressedBlockAckReq *>(&blockAckReq)) {
        stream << " compressed tid=" << compressedBlockAckReq->getTidInfo() << " start=";
        printSequenceNumber(stream, compressedBlockAckReq->getStartingSequenceNumber());
    }
    else if (auto multiTidBlockAckReq = dynamic_cast<const Ieee80211MultiTidBlockAckReq *>(&blockAckReq)) {
        stream << " multi-tid records=" << multiTidBlockAckReq->getRecordsArraySize();
    }
}

void printBlockAckInfo(std::ostream& stream, const Ieee80211BlockAck& blockAck)
{
    stream << " policy=" << (blockAck.getBlockAckPolicy() ? "no-ack" : "normal");
    if (auto basicBlockAck = dynamic_cast<const Ieee80211BasicBlockAck *>(&blockAck)) {
        stream << " basic tid=" << basicBlockAck->getTidInfo() << " start=";
        printSequenceNumber(stream, basicBlockAck->getStartingSequenceNumber());
    }
    else if (auto compressedBlockAck = dynamic_cast<const Ieee80211CompressedBlockAck *>(&blockAck)) {
        stream << " compressed tid=" << compressedBlockAck->getTidInfo() << " start=";
        printSequenceNumber(stream, compressedBlockAck->getStartingSequenceNumber());
    }
    else if (auto multiTidBlockAck = dynamic_cast<const Ieee80211MultiTidBlockAck *>(&blockAck)) {
        stream << " multi-tid records=" << multiTidBlockAck->getRecordsArraySize();
    }
    else if (auto multiStaBlockAck = dynamic_cast<const Ieee80211MultiStaBlockAck *>(&blockAck)) {
        stream << " multi-sta records=" << multiStaBlockAck->getRecordsArraySize();
    }
}

void printActionInfo(std::ostream& stream, const Ieee80211ActionFrame& action)
{
    if (auto addbaRequest = dynamic_cast<const Ieee80211AddbaRequest *>(&action)) {
        stream << " ADDBA request dialog=" << static_cast<int>(addbaRequest->getDialogToken())
               << " tid=" << static_cast<int>(addbaRequest->getTid())
               << " buffer=" << addbaRequest->getBufferSize();
    }
    else if (auto addbaResponse = dynamic_cast<const Ieee80211AddbaResponse *>(&action)) {
        stream << " ADDBA response dialog=" << static_cast<int>(addbaResponse->getDialogToken())
               << " status=" << addbaResponse->getStatusCode()
               << " tid=" << static_cast<int>(addbaResponse->getTid())
               << " buffer=" << addbaResponse->getBufferSize();
    }
    else if (auto delba = dynamic_cast<const Ieee80211Delba *>(&action)) {
        stream << " DELBA tid=" << static_cast<int>(delba->getTid())
               << (delba->getInitiator() ? " initiator" : " recipient")
               << " reason=" << delba->getReasonCode();
    }
    else if (auto twtSetup = dynamic_cast<const Ieee80211TwtSetupFrame *>(&action)) {
        stream << " TWT setup flow=" << static_cast<int>(twtSetup->getFlowId())
               << " dialog=" << static_cast<int>(twtSetup->getDialogToken())
               << (twtSetup->getTwtRequest() ? " request" : " response")
               << (twtSetup->getTrigger() ? " trigger" : "");
    }
    else if (auto twtTeardown = dynamic_cast<const Ieee80211TwtTeardownFrame *>(&action)) {
        stream << " TWT teardown flow=" << static_cast<int>(twtTeardown->getFlowId())
               << (twtTeardown->getTeardownAll() ? " all" : "");
    }
    else if (auto twtInformation = dynamic_cast<const Ieee80211TwtInformationFrame *>(&action)) {
        stream << " TWT information flow=" << static_cast<int>(twtInformation->getFlowId())
               << (twtInformation->getNextWakeTimePresent() ? " next-wake" : "");
    }
    else {
        stream << " category=" << action.getCategory()
               << " blockAckAction=" << static_cast<int>(action.getBlockAckAction())
               << " s1gAction=" << static_cast<int>(action.getS1gAction());
    }
}

} // namespace

void Ieee80211MacProtocolPrinter::print(const Ptr<const Chunk>& chunk, const Protocol *protocol, const cMessagePrinter::Options *options, Context& context) const
{
    if (auto mpduSubframeHeader = dynamicPtrCast<const Ieee80211MpduSubframeHeader>(chunk)) {
        context.infoColumn << "A-MPDU length=" << mpduSubframeHeader->getLength();
        if (mpduSubframeHeader->getEof())
            context.infoColumn << " EOF";
        context.typeColumn << "MPDU delimiter";
    }
    else if (auto msduSubframeHeader = dynamicPtrCast<const Ieee80211MsduSubframeHeader>(chunk)) {
        context.sourceColumn << msduSubframeHeader->getSa();
        context.destinationColumn << msduSubframeHeader->getDa();
        context.typeColumn << "MSDU subframe";
        context.infoColumn << "A-MSDU length=" << msduSubframeHeader->getLength();
    }
    else if (auto macTrailer = dynamicPtrCast<const Ieee80211MacTrailer>(chunk)) {
        context.typeColumn << "FCS";
        context.infoColumn << "fcs=" << macTrailer->getFcs();
    }
    else if (auto macHeader = dynamicPtrCast<const Ieee80211MacHeader>(chunk)) {
        std::ostringstream stream;
        stream << "WLAN";
        if (auto oneAddressHeader = dynamicPtrCast<const Ieee80211OneAddressHeader>(chunk))
            context.destinationColumn << oneAddressHeader->getReceiverAddress();
        if (auto twoAddressHeader = dynamicPtrCast<const Ieee80211TwoAddressHeader>(chunk))
            context.sourceColumn << twoAddressHeader->getTransmitterAddress();
        if (auto typeName = frameTypeName(macHeader->getType()))
            context.typeColumn << typeName;
        else {
            context.typeColumn << "Unknown";
            stream << " type=" << macHeader->getType();
        }
        printFrameFlags(stream, *macHeader);
        if (auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(chunk))
            printDataOrMgmtInfo(stream, *dataOrMgmtHeader);
        if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(chunk)) {
            if (dataHeader->getType() == ST_DATA_WITH_QOS || dataHeader->getType() == ST_QOS_NULL || dataHeader->getAckPolicy() != NORMAL_ACK) {
                stream << " tid=" << static_cast<int>(dataHeader->getTid())
                       << " ack=" << ackPolicyName(dataHeader->getAckPolicy());
            }
            if (dataHeader->getToDS() && dataHeader->getFromDS())
                stream << " address4=" << dataHeader->getAddress4();
            if (dataHeader->getAMsduPresent())
                stream << " A-MSDU";
            if (dataHeader->getBufferStatusPresent())
                stream << " BSR tid=" << static_cast<int>(dataHeader->getBufferStatusTid())
                       << " ac=" << static_cast<int>(dataHeader->getBufferStatusAc())
                       << " queue=" << dataHeader->getBufferStatusQueueSize();
        }
        if (auto trigger = dynamicPtrCast<const Ieee80211TriggerFrame>(chunk))
            printTriggerInfo(stream, *trigger);
        if (auto blockAckReq = dynamicPtrCast<const Ieee80211BlockAckReq>(chunk))
            printBlockAckReqInfo(stream, *blockAckReq);
        if (auto blockAck = dynamicPtrCast<const Ieee80211BlockAck>(chunk))
            printBlockAckInfo(stream, *blockAck);
        if (auto action = dynamicPtrCast<const Ieee80211ActionFrame>(chunk))
            printActionInfo(stream, *action);
        context.infoColumn << stream.str();
    }
    else
        context.infoColumn << "(IEEE 802.11 Mac) " << chunk;
}

} // namespace ieee80211
} // namespace inet
