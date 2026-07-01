//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/Ieee80211MldMac.h"

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"

namespace inet {
namespace ieee80211 {

Define_Module(Ieee80211MldMac);

Ieee80211MldMac::~Ieee80211MldMac()
{
    delete sequenceNumberAssignment;
}

void Ieee80211MldMac::initialize(int stage)
{
    MacProtocolBase::initialize(stage);
    
    if (stage == INITSTAGE_LOCAL) {
        numLinks = par("numLinks");
        lowerLinkInBaseGateId = gateBaseId("lowerLinkIn");
        lowerLinkOutBaseGateId = gateBaseId("lowerLinkOut");
        
        mldMacAddress = parseMacAddressParameter(par("mldMacAddress").stringValue());
        
        sequenceNumberAssignment = new ieee80211::QoSSequenceNumberAssignment();
        for (int i = 0; i < 4; i++) {
            pendingQueue[i] = check_and_cast<queueing::IPacketQueue *>(getSubmodule("pendingQueue", i));
        }
    }
}

void Ieee80211MldMac::configureNetworkInterface()
{
    if (networkInterface) {
        networkInterface->setMacAddress(mldMacAddress);
        // Note: bit rate, MTU, etc. might need to be aggregated from links or set globally
    }
}

bool Ieee80211MldMac::isUpperMessage(cMessage *message) const
{
    return message->getArrivalGate()->getId() == upperLayerInGateId;
}

bool Ieee80211MldMac::isLowerMessage(cMessage *message) const
{
    int gateId = message->getArrivalGate()->getId();
    return gateId >= lowerLinkInBaseGateId && gateId < lowerLinkInBaseGateId + numLinks;
}

void Ieee80211MldMac::handleUpperMessage(cMessage *message)
{
    // Basic stub: For now, forward everything to link 0
    if (numLinks > 0) {
        auto packet = check_and_cast<Packet *>(message);
        auto macAddressReq = packet->addTagIfAbsent<MacAddressReq>();
        if (macAddressReq->getSrcAddress().isUnspecified())
            macAddressReq->setSrcAddress(mldMacAddress);
        if (macAddressReq->getDestAddress().isUnspecified())
            macAddressReq->setDestAddress(MacAddress::BROADCAST_ADDRESS);
        send(message, lowerLinkOutBaseGateId); // Send to lowerLinkOut[0]
    } else {
        delete message;
    }
}

void Ieee80211MldMac::handleLowerMessage(cMessage *message)
{
    // Forward from lower MAC to the upper layer
    if (networkInterface != nullptr) {
        auto packet = check_and_cast<Packet *>(message);
        packet->addTagIfAbsent<InterfaceInd>()->setInterfaceId(networkInterface->getInterfaceId());
        auto macAddressInd = packet->addTagIfAbsent<MacAddressInd>();
        if (macAddressInd->getDestAddress().isUnspecified())
            macAddressInd->setDestAddress(mldMacAddress);
    }
    sendUp(message);
}

} // namespace ieee80211
} // namespace inet
