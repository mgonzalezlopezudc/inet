//
// Copyright (C) 2006 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtStaSimplified.h"

#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigA.h"

namespace inet {

namespace ieee80211 {

Define_Module(Ieee80211MgmtStaSimplified);

void Ieee80211MgmtStaSimplified::initialize(int stage)
{
    Ieee80211MgmtBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        peerHtLdpcRxSupported = par("peerHtLdpcRxSupported").boolValue();
        peerVhtLdpcRxSupported = par("peerVhtLdpcRxSupported").boolValue();
        mib->mode = Ieee80211Mib::INFRASTRUCTURE;
        mib->bssStationData.stationType = Ieee80211Mib::STATION;
        mib->bssStationData.isAssociated = true;
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        L3AddressResolver addressResolver;
        auto accessPointAddress = addressResolver.resolve(par("accessPointAddress"), L3AddressResolver::ADDR_MAC).toMac();
        mib->bssData.bssid = accessPointAddress;
        auto host = addressResolver.findHostWithAddress(mib->bssData.bssid);
        if (host == nullptr)
            throw cRuntimeError("Access point with address %s not found", mib->bssData.bssid.str().c_str());
        auto interfaceTable = addressResolver.findInterfaceTableOf(host);
        auto networkInterface = interfaceTable->findInterfaceByAddress(mib->bssData.bssid);
        auto apMib = dynamic_cast<Ieee80211Mib *>(networkInterface->getSubmodule("mib"));
        apMib->bssAccessPointData.stations[mib->address] = Ieee80211Mib::ASSOCIATED;
        apMib->allocateAssociationId(mib->address);
        mib->bssData.ssid = apMib->bssData.ssid;
    }
}

Ieee80211PeerLdpcStatus Ieee80211MgmtStaSimplified::getPeerLdpcStatus(const MacAddress& peer) const
{
    Ieee80211PeerLdpcStatus result;
    if (mib->bssStationData.isAssociated && peer == mib->bssData.bssid) {
        result.htRxLdpc = peerHtLdpcRxSupported ? Ieee80211CapabilityStatus::SUPPORTED : Ieee80211CapabilityStatus::UNSUPPORTED;
        result.vhtRxLdpc = peerVhtLdpcRxSupported ? Ieee80211CapabilityStatus::SUPPORTED : Ieee80211CapabilityStatus::UNSUPPORTED;
    }
    return applyLatestPeerOperatingMode(peer, result);
}

Ieee80211VhtSigAParameters Ieee80211MgmtStaSimplified::getVhtSigAParameters(const MacAddress& receiverAddress) const
{
    // IEEE Std 802.11-2024, 10.19, Table 10-13.
    if (receiverAddress.isMulticast())
        return {true, 63, 0};
    if (mib->bssStationData.isAssociated && receiverAddress == mib->bssData.bssid)
        return {true, 0, static_cast<uint16_t>(physicallayer::computeVhtPartialAidForBssid(receiverAddress))};
    return {true, 63, 0};
}

void Ieee80211MgmtStaSimplified::handleTimer(cMessage *msg)
{
    ASSERT(false);
}

void Ieee80211MgmtStaSimplified::handleCommand(int msgkind, cObject *ctrl)
{
    throw cRuntimeError("handleCommand(): no commands supported");
}

void Ieee80211MgmtStaSimplified::handleAuthenticationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtStaSimplified::handleDeauthenticationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtStaSimplified::handleAssociationRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtStaSimplified::handleAssociationResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtStaSimplified::handleReassociationRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtStaSimplified::handleReassociationResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtStaSimplified::handleDisassociationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtStaSimplified::handleBeaconFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtStaSimplified::handleProbeRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtStaSimplified::handleProbeResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

} // namespace ieee80211

} // namespace inet
