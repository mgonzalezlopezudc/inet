//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mib/Ieee80211PeerLinkState.h"

#include <cmath>

namespace inet {
namespace ieee80211 {

namespace {

bool equalValue(double left, double right)
{
    return left == right || (std::isnan(left) && std::isnan(right));
}

} // namespace

void Ieee80211PeerLinkState::advanceGeneration(uint64_t& generation)
{
    if (++generation == 0)
        generation = 1;
}

void Ieee80211PeerLinkState::setTransmitPower(const MacAddress& address, double transmitPowerDbm)
{
    auto [it, inserted] = records.try_emplace(address);
    auto& record = it->second;
    double pathLossDb = std::isnan(record.receivedPowerDbm) ? record.pathLossDb : transmitPowerDbm - record.receivedPowerDbm;
    bool valid = !std::isnan(record.receivedPowerDbm);
    if (inserted || !equalValue(record.transmitPowerDbm, transmitPowerDbm) ||
            !equalValue(record.pathLossDb, pathLossDb) || record.valid != valid) {
        record.transmitPowerDbm = transmitPowerDbm;
        record.pathLossDb = pathLossDb;
        record.valid = valid;
        advanceGeneration(record.generation);
    }
}

void Ieee80211PeerLinkState::updateReceivedPower(const MacAddress& address, units::values::W receivedPower, simtime_t updateTime)
{
    if (receivedPower.get() <= 0)
        return;
    auto& record = records[address];
    double receivedPowerDbm = 10 * std::log10(receivedPower.get() / 1e-3);
    double pathLossDb = record.transmitPowerDbm - receivedPowerDbm;
    if (record.receivedPowerDbm != receivedPowerDbm || record.pathLossDb != pathLossDb || record.lastUpdate != updateTime || !record.valid) {
        record.receivedPowerDbm = receivedPowerDbm;
        record.pathLossDb = pathLossDb;
        record.lastUpdate = updateTime;
        record.valid = true;
        advanceGeneration(record.generation);
    }
}

std::optional<Ieee80211PeerLinkState::Snapshot> Ieee80211PeerLinkState::getSnapshot(const MacAddress& address) const
{
    auto it = records.find(address);
    if (it == records.end())
        return std::nullopt;
    const auto& record = it->second;
    return Snapshot(address, record.transmitPowerDbm, record.receivedPowerDbm, record.pathLossDb,
            record.lastUpdate, record.valid, record.generation);
}

std::vector<Ieee80211PeerLinkState::Snapshot> Ieee80211PeerLinkState::getSnapshots() const
{
    std::vector<Snapshot> snapshots;
    for (const auto& entry : records)
        snapshots.push_back(*getSnapshot(entry.first));
    return snapshots;
}

} // namespace ieee80211
} // namespace inet
