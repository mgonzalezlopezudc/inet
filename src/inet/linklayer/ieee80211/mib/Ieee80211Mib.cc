//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"

#include <cmath>

namespace inet {

namespace ieee80211 {

Define_Module(Ieee80211Mib);

void Ieee80211Mib::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        WATCH(address);
        WATCH(mode);
        WATCH(qos);
        WATCH(bssData.bssid);
        WATCH(bssStationData.stationType);
        WATCH(bssStationData.isAssociated);
        WATCH(bssStationData.associationId);
        WATCH(bssAccessPointData.stations);
        WATCH(bssAccessPointData.associationIds);
        WATCH_EXPR("modeStr", getModeStr(mode));
        WATCH_EXPR("stationTypeStr", getStationTypeStr(bssStationData.stationType));
        WATCH_EXPR("qosStr", qos ? ", QoS" : ", Non-QoS");
        WATCH_EXPR("ssidStr", getSsidStr());
        WATCH_EXPR("ssid", bssData.ssid.empty() ? std::string("-") : bssData.ssid); // associated SSID ("-" if none), for node display strings
        WATCH_EXPR("associatedStr", bssStationData.stationType == STATION ? (bssStationData.isAssociated ? "\nAssociated" : "\nNot associated") : "");
    }
}

std::string Ieee80211Mib::getSsidStr() const
{
    if (mode == INFRASTRUCTURE)
        return "\nSSID: " + bssData.ssid + ", " + bssData.bssid.str();
    return "";
}

void Ieee80211Mib::setStationTransmitPower(const MacAddress& address, double transmitPowerDbm)
{
    auto& link = bssAccessPointData.links[address];
    link.transmitPowerDbm = transmitPowerDbm;
    if (!std::isnan(link.receivedPowerDbm)) {
        link.pathLossDb = link.transmitPowerDbm - link.receivedPowerDbm;
        link.valid = true;
    }
}

void Ieee80211Mib::updateStationReceivedPower(const MacAddress& address, units::values::W receivedPower)
{
    if (receivedPower.get() <= 0)
        return;
    auto& link = bssAccessPointData.links[address];
    link.receivedPowerDbm = 10 * std::log10(receivedPower.get() / 1e-3);
    link.pathLossDb = link.transmitPowerDbm - link.receivedPowerDbm;
    link.lastUpdate = simTime();
    link.valid = true;
}

const Ieee80211Mib::BssAccessPointData::LinkData *Ieee80211Mib::findStationLink(const MacAddress& address) const
{
    auto it = bssAccessPointData.links.find(address);
    return it == bssAccessPointData.links.end() ? nullptr : &it->second;
}

short Ieee80211Mib::allocateAssociationId(const MacAddress& address)
{
    auto existing = bssAccessPointData.associationIds.find(address);
    if (existing != bssAccessPointData.associationIds.end())
        return existing->second;
    for (short aid = 1; aid <= 2007; aid++) {
        bool used = false;
        for (const auto& entry : bssAccessPointData.associationIds)
            if (entry.second == aid) {
                used = true;
                break;
            }
        if (!used) {
            bssAccessPointData.associationIds[address] = aid;
            return aid;
        }
    }
    throw cRuntimeError("No IEEE 802.11 association ID is available");
}

void Ieee80211Mib::releaseAssociationId(const MacAddress& address)
{
    bssAccessPointData.associationIds.erase(address);
}

short Ieee80211Mib::getAssociationId(const MacAddress& address) const
{
    auto it = bssAccessPointData.associationIds.find(address);
    return it == bssAccessPointData.associationIds.end() ? -1 : it->second;
}

MacAddress Ieee80211Mib::getStationAddress(short associationId) const
{
    for (const auto& entry : bssAccessPointData.associationIds)
        if (entry.second == associationId)
            return entry.first;
    return MacAddress::UNSPECIFIED_ADDRESS;
}

const char *Ieee80211Mib::getModeStr(Ieee80211Mib::Mode mode)
{
    switch (mode) {
        case INFRASTRUCTURE: return "Infrastructure";
        case INDEPENDENT: return "Ad-hoc";
        case MESH: return "Mesh";
        default: return "?";
    }
}

const char *Ieee80211Mib::getStationTypeStr(Ieee80211Mib::BssStationType stationType)
{
    switch (stationType) {
        case ACCESS_POINT: return ", AP";
        case STATION: return ", STA";
        default: return "";
    }
}

} // namespace ieee80211

} // namespace inet
