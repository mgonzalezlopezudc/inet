//
// Copyright (C) 2006 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtBase.h"

#include "inet/common/INETUtils.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/lifecycle/LifecycleOperation.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/common/packet/chunk/SliceChunk.h"

namespace inet {

namespace ieee80211 {

using namespace inet::physicallayer;

void Ieee80211MgmtBase::initialize(int stage)
{
    OperationalBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        mib.reference(this, "mibModule", true);
        interfaceTable.reference(this, "interfaceTableModule", true);
        myIface = getContainingNicModule(this);
        numMgmtFramesReceived = 0;
        numMgmtFramesDropped = 0;
        getContainingNicModule(this)->subscribe(modesetChangedSignal, this);
        WATCH(numMgmtFramesReceived);
        WATCH(numMgmtFramesDropped);
    }
}

void Ieee80211MgmtBase::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (signalID == modesetChangedSignal) {
        modeSet = check_and_cast<physicallayer::Ieee80211ModeSet *>(obj);
        supportedRates.numRates = std::min(8, modeSet->getNumModes());
        int rateIndex = 0;
        for (int i = 0; i < supportedRates.numRates; i++)
            if (modeSet->isMandatory(i))
                supportedRates.rate[rateIndex++] = modeSet->getMode(i)->getDataMode()->getNetBitrate().get<Mbps>();
    }
}

void Ieee80211MgmtBase::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        // process timers
        EV << "Timer expired: " << msg << "\n";
        handleTimer(msg);
    }
    else if (msg->arrivedOn("macIn")) {
        // process incoming frame
        EV << "Frame arrived from MAC: " << msg << "\n";
        auto packet = check_and_cast<Packet *>(msg);
        Ptr<const Ieee80211DataOrMgmtHeader> header = nullptr;
        auto content = packet->peekAll();
        b targetEnd = packet->getFrontOffset();
        if (auto seq = dynamicPtrCast<const SequenceChunk>(content)) {
            b offset = b(0);
            for (const auto& chunk : seq->getChunks()) {
                b length = chunk->getChunkLength();
                if (offset + length == targetEnd) {
                    header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(chunk);
                    if (!header) {
                        if (auto slice = dynamicPtrCast<const SliceChunk>(chunk))
                            header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(slice->getChunk());
                    }
                    break;
                }
                offset += length;
            }
        }
        else {
            header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(content);
            if (!header) {
                if (auto slice = dynamicPtrCast<const SliceChunk>(content))
                    header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(slice->getChunk());
            }
        }
        if (!header) {
            header = packet->peekAt<Ieee80211DataOrMgmtHeader>(packet->getFrontOffset() - B(24));
        }
        processFrame(packet, header);
    }
    else if (msg->arrivedOn("agentIn")) {
        // process command from agent
        EV << "Command arrived from agent: " << msg << "\n";
        int msgkind = msg->getKind();
        cObject *ctrl = msg->removeControlInfo();
        delete msg;

        handleCommand(msgkind, ctrl);
    }
    else
        throw cRuntimeError("Unknown message");
}

void Ieee80211MgmtBase::sendDown(Packet *frame)
{
    ASSERT(isUp());
    frame->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211Mgmt);
    send(frame, "macOut");
}

void Ieee80211MgmtBase::dropManagementFrame(Packet *frame)
{
    EV << "ignoring management frame: " << (cMessage *)frame << "\n";
    delete frame;
    numMgmtFramesDropped++;
}

void Ieee80211MgmtBase::processFrame(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& header)
{
    switch (header->getType()) {
        case ST_AUTHENTICATION:
            numMgmtFramesReceived++;
            handleAuthenticationFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_DEAUTHENTICATION:
            numMgmtFramesReceived++;
            handleDeauthenticationFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_ASSOCIATIONREQUEST:
            numMgmtFramesReceived++;
            handleAssociationRequestFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_ASSOCIATIONRESPONSE:
            numMgmtFramesReceived++;
            handleAssociationResponseFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_REASSOCIATIONREQUEST:
            numMgmtFramesReceived++;
            handleReassociationRequestFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_REASSOCIATIONRESPONSE:
            numMgmtFramesReceived++;
            handleReassociationResponseFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_DISASSOCIATION:
            numMgmtFramesReceived++;
            handleDisassociationFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_BEACON:
            numMgmtFramesReceived++;
            handleBeaconFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_PROBEREQUEST:
            numMgmtFramesReceived++;
            handleProbeRequestFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_PROBERESPONSE:
            numMgmtFramesReceived++;
            handleProbeResponseFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_ACTION:
            numMgmtFramesReceived++;
            handleActionFrame(packet, dynamicPtrCast<const Ieee80211ActionFrame>(header));
            break;

        default:
            throw cRuntimeError("Unexpected frame type (%s)%s", packet->getClassName(), packet->getName());
    }
}

void Ieee80211MgmtBase::handleActionFrame(Packet *packet, const Ptr<const Ieee80211ActionFrame>& header)
{
    if (dynamicPtrCast<const Ieee80211TwtSetupFrame>(header) ||
            dynamicPtrCast<const Ieee80211TwtTeardownFrame>(header) ||
            dynamicPtrCast<const Ieee80211TwtInformationFrame>(header))
        EV_WARN << "Received TWT action without a TWT-capable management implementation\n";
    dropManagementFrame(packet);
}

void Ieee80211MgmtBase::start()
{
}

void Ieee80211MgmtBase::stop()
{
}

} // namespace ieee80211

} // namespace inet
