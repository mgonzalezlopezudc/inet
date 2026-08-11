//
// Copyright (C) 2006 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtStaSimplified.h"

#include "inet/networklayer/common/L3AddressResolver.h"

namespace inet {

namespace ieee80211 {

Define_Module(Ieee80211MgmtStaSimplified);

void Ieee80211MgmtStaSimplified::initialize(int stage)
{
    Ieee80211MgmtBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        mib->mode = Ieee80211Mib::INFRASTRUCTURE;
        mib->setBssStationType(Ieee80211Mib::STATION);
    }
    else if (stage == INITSTAGE_LINK_LAYER + 1) {
        L3AddressResolver addressResolver;
        auto accessPointAddress = addressResolver.resolve(par("accessPointAddress"), L3AddressResolver::ADDR_MAC).toMac();
        mib->setBssIdentity(mib->getSsid(), accessPointAddress);
        auto host = addressResolver.findHostWithAddress(mib->getBssid());
        if (host == nullptr)
            throw cRuntimeError("Access point with address %s not found", mib->getBssid().str().c_str());
        auto interfaceTable = addressResolver.findInterfaceTableOf(host);
        auto networkInterface = interfaceTable->findInterfaceByAddress(mib->getBssid());
        auto apMib = dynamic_cast<Ieee80211Mib *>(networkInterface->getSubmodule("mib"));
        if (!mib->localOperationalRates.empty() && !apMib->localOperationalRates.empty()) {
            apMib->setPeerLegacyRates(mib->address,
                    mib->getSupportedRatesElement(),
                    mib->getExtendedSupportedRatesElement());
            mib->setPeerLegacyRates(apMib->address,
                    apMib->getSupportedRatesElement(),
                    apMib->getExtendedSupportedRatesElement());
            mib->installCurrentBssBasicLegacyRates(apMib->getSupportedRatesElement(),
                    apMib->getExtendedSupportedRatesElement());
        }
        else {
            apMib->removePeerCapabilities(mib->address);
            mib->removePeerCapabilities(apMib->address);
            mib->clearCurrentBssBasicLegacyRates();
        }
        if (isHtManagementSupported()) {
            apMib->setPeerHtCapabilities(mib->address, mib->localHtCapabilities, apMib->htOperation);
            mib->setPeerHtCapabilities(apMib->address, apMib->localHtCapabilities, apMib->htOperation);
        }
        else {
            apMib->removePeerHtCapabilities(mib->address);
            mib->removePeerHtCapabilities(apMib->address);
        }
        if (isVhtManagementSupported()) {
            apMib->setPeerVhtCapabilities(mib->address, mib->localVhtCapabilities, apMib->vhtOperation);
            mib->setPeerVhtCapabilities(apMib->address, apMib->localVhtCapabilities, apMib->vhtOperation);
        }
        else {
            apMib->removePeerVhtCapabilities(mib->address);
            mib->removePeerVhtCapabilities(apMib->address);
        }
        if (isHeManagementSupported()) {
            apMib->setPeerHeCapabilities(mib->address, mib->localHeCapabilities, apMib->heOperation);
            mib->setPeerHeCapabilities(apMib->address, apMib->localHeCapabilities, apMib->heOperation);
            mib->heOperation.bssColor = apMib->heOperation.bssColor;
            EV_INFO << "Peer HE capabilities set for AP address=" << apMib->address << ", BSS color=" << (int)mib->heOperation.bssColor << "\n";
        }
        else {
            apMib->removePeerHeCapabilities(mib->address);
            mib->removePeerHeCapabilities(apMib->address);
            mib->heOperation.bssColor = 0;
        }
        if (isEhtManagementSupported()) {
            apMib->setPeerEhtCapabilities(mib->address, mib->localEhtCapabilities, apMib->ehtOperation);
            mib->setPeerEhtCapabilities(apMib->address, apMib->localEhtCapabilities, apMib->ehtOperation);
            mib->ehtOperation = apMib->ehtOperation;
            EV_INFO << "Peer EHT capabilities set for AP address=" << apMib->address << "\n";
        }
        else {
            apMib->removePeerEhtCapabilities(mib->address);
            mib->removePeerEhtCapabilities(apMib->address);
        }
        auto association = apMib->commitPeerAssociation(mib->address);
        mib->installLocalAssociation(apMib->getSsid(), accessPointAddress, association.getAssociationId());
    }
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
