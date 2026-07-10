//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/Ieee80211MldMac.h"

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/common/NetworkInterface.h"

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

    cModule *mibModule = getParentModule()->getSubmodule("mib");
    if (mibModule) {
        auto mib = check_and_cast<Ieee80211Mib *>(mibModule);
        mib->address = mldMacAddress;
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

        if (!macAddressReq->getDestAddress().isMulticast() && !macAddressReq->getDestAddress().isUnspecified()) {
            MacAddress destMldAddress = macAddressReq->getDestAddress();
            L3AddressResolver addressResolver;
            cModule *destHost = addressResolver.findHostWithAddress(destMldAddress);
            if (destHost) {
                IInterfaceTable *ift = addressResolver.findInterfaceTableOf(destHost);
                if (ift) {
                    std::string linkName = "link" + std::to_string(0); // currently choosing link 0
                    NetworkInterface *ie = ift->findInterfaceByName(linkName.c_str());
                    if (ie) {
                        macAddressReq->setDestAddress(ie->getMacAddress());
                    }
                }
            }
        }
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

        if (!macAddressInd->getSrcAddress().isMulticast() && !macAddressInd->getSrcAddress().isUnspecified()) {
            MacAddress srcLinkAddress = macAddressInd->getSrcAddress();
            L3AddressResolver addressResolver;
            NetworkInterface *srcLinkIe = addressResolver.findInterfaceWithMacAddress(srcLinkAddress);
            if (srcLinkIe) {
                cModule *mldInterfaceModule = srcLinkIe->getParentModule();
                if (mldInterfaceModule) {
                    auto mldInterface = check_and_cast<NetworkInterface *>(mldInterfaceModule);
                    macAddressInd->setSrcAddress(mldInterface->getMacAddress());
                }
            }
        }
    }
    sendUp(message);
}

} // namespace ieee80211
} // namespace inet
