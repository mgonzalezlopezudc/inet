//
// Copyright (C) 2006 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtApSimplified.h"

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigA.h"

namespace inet {

namespace ieee80211 {

Define_Module(Ieee80211MgmtApSimplified);

// FIXME add sequence number handling

void Ieee80211MgmtApSimplified::initialize(int stage)
{
    Ieee80211MgmtApBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        peerHtLdpcRxSupported = par("peerHtLdpcRxSupported").boolValue();
        peerVhtLdpcRxSupported = par("peerVhtLdpcRxSupported").boolValue();
    }
}

Ieee80211PeerLdpcStatus Ieee80211MgmtApSimplified::getPeerLdpcStatus(const MacAddress& peer) const
{
    Ieee80211PeerLdpcStatus result;
    auto status = mib->bssAccessPointData.stations.find(peer);
    if (status != mib->bssAccessPointData.stations.end() && status->second == Ieee80211Mib::ASSOCIATED) {
        result.htRxLdpc = peerHtLdpcRxSupported ? Ieee80211CapabilityStatus::SUPPORTED : Ieee80211CapabilityStatus::UNSUPPORTED;
        result.vhtRxLdpc = peerVhtLdpcRxSupported ? Ieee80211CapabilityStatus::SUPPORTED : Ieee80211CapabilityStatus::UNSUPPORTED;
    }
    return applyLatestPeerOperatingMode(peer, result);
}

Ieee80211VhtSigAParameters Ieee80211MgmtApSimplified::getVhtSigAParameters(const MacAddress& receiverAddress) const
{
    // IEEE Std 802.11-2024, 10.19, Table 10-13 and Equation (10-13).
    if (receiverAddress.isMulticast())
        return {true, 63, 0};
    auto status = mib->bssAccessPointData.stations.find(receiverAddress);
    if (status != mib->bssAccessPointData.stations.end() && status->second == Ieee80211Mib::ASSOCIATED) {
        auto aid = mib->bssAccessPointData.associationIds.find(receiverAddress);
        if (aid == mib->bssAccessPointData.associationIds.end())
            throw cRuntimeError("Associated IEEE 802.11 station %s has no association ID", receiverAddress.str().c_str());
        return {true, 63, static_cast<uint16_t>(physicallayer::computeVhtPartialAidForAssociatedSta(
                aid->second, mib->bssData.bssid))};
    }
    return {true, 63, 0};
}

void Ieee80211MgmtApSimplified::handleTimer(cMessage *msg)
{
    ASSERT(false);
}

void Ieee80211MgmtApSimplified::handleCommand(int msgkind, cObject *ctrl)
{
    throw cRuntimeError("handleCommand(): no commands supported");
}

void Ieee80211MgmtApSimplified::handleAuthenticationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtApSimplified::handleDeauthenticationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtApSimplified::handleAssociationRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtApSimplified::handleAssociationResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtApSimplified::handleReassociationRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtApSimplified::handleReassociationResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtApSimplified::handleDisassociationFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtApSimplified::handleBeaconFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtApSimplified::handleProbeRequestFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

void Ieee80211MgmtApSimplified::handleProbeResponseFrame(Packet *packet, const Ptr<const Ieee80211MgmtHeader>& header)
{
    dropManagementFrame(packet);
}

} // namespace ieee80211

} // namespace inet
