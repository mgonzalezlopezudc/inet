//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/AmpduTransmissionLedger.h"

namespace inet {
namespace ieee80211 {

void AmpduTransmissionLedger::record(Packet *packet,
        const std::vector<Packet *>& subframes, bool implicitBlockAck)
{
    entries[packet] = {subframes, implicitBlockAck};
}

bool AmpduTransmissionLedger::hasImplicitBlockAck(Packet *packet) const
{
    auto entry = entries.find(packet);
    return entry != entries.end() && entry->second.implicitBlockAck;
}

std::optional<AmpduTransmissionLedger::Entry> AmpduTransmissionLedger::take(Packet *packet)
{
    auto entry = entries.find(packet);
    if (entry == entries.end())
        return std::nullopt;
    auto value = std::move(entry->second);
    entries.erase(entry);
    return value;
}

bool AmpduTransmissionLedger::discard(Packet *packet)
{
    return entries.erase(packet) != 0;
}

} // namespace ieee80211
} // namespace inet
