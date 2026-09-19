// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanReassemblyTable.h"

namespace inet { namespace lowpan {

bool LowpanReassemblyTable::Key::operator<(const Key& other) const
{
    return std::tie(interfaceId, sourcePan, destinationPan, source, destination, size, tag) <
           std::tie(other.interfaceId, other.sourcePan, other.destinationPan, other.source, other.destination, other.size, other.tag);
}

LowpanReassemblyTable::LowpanReassemblyTable(int maxContexts, int maxBytes, simtime_t timeout) :
    maxContexts(maxContexts), maxBytes(maxBytes), timeout(timeout)
{
    if (maxContexts < 1 || maxBytes < 40 || timeout <= SIMTIME_ZERO || timeout > SimTime(60))
        throw cRuntimeError("Invalid LoWPAN reassembly limits (timeout must be in (0,60] seconds)");
}

LowpanReassemblyTable::Result LowpanReassemblyTable::accept(const Key& key, int offset, bool first,
        const Packet& rangePacket, simtime_t now)
{
    expire(now);
    auto bitLength = rangePacket.getDataLength().get();
    if (key.interfaceId < 0 || key.sourcePan == 0xffff || key.destinationPan == 0xffff ||
        key.source.isUnspecified() || key.source.isBroadcast() || key.destination.isUnspecified() ||
        key.size < 40 || key.size > 2047 || offset < 0 || offset >= key.size || offset % 8 != 0 ||
        first != (offset == 0) || bitLength <= 0 || bitLength % 8 != 0 || bitLength > (key.size - offset) * 8)
        return {INVALID, nullptr};
    int length = bitLength / 8;
    if (offset + length < key.size && length % 8 != 0)
        return {INVALID, nullptr};
    auto found = contexts.find(key);
    if (found == contexts.end()) {
        if ((int)contexts.size() >= maxContexts || key.size > maxBytes - reservedBytes)
            return {RESOURCE_LIMIT, nullptr};
        found = contexts.emplace(key, Context()).first;
        found->second.expiry = now + timeout;
        reservedBytes += key.size;
    }
    auto& context = found->second;
    if (context.quarantined)
        return {QUARANTINED, nullptr};
    bool restart = false;
    for (const auto& item : context.ranges) {
        int previousOffset = item.first;
        int previousLength = item.second->getDataLength().get<B>();
        if (previousOffset == offset && previousLength == length) {
            if (item.second->peekDataAsBytes()->getBytes() == rangePacket.peekDataAsBytes()->getBytes())
                return {DUPLICATE, nullptr};
            // Local hardening: conflicting same-range data quarantines this key.
            context.ranges.clear();
            context.covered = 0;
            context.quarantined = true;
            reservedBytes -= key.size;
            return {CONFLICT, nullptr};
        }
        if (offset < previousOffset + previousLength && previousOffset < offset + length) {
            restart = true;
            break;
        }
    }
    if (restart) {
        // RFC 4944 section 5.3 permits restarting with the overlapping fragment.
        context.ranges.clear();
        context.covered = 0;
        context.firstSeen = false;
        context.expiry = now + timeout;
    }
    auto stored = std::make_unique<Packet>(rangePacket.getName());
    stored->insertAtBack(rangePacket.peekData());
    stored->copyRegionTags(rangePacket, rangePacket.getFrontOffset(), b(0), rangePacket.getDataLength());
    context.ranges.emplace(offset, std::move(stored));
    context.covered += length;
    context.firstSeen |= first;
    if (context.firstSeen && context.covered == key.size) {
        auto packet = std::make_unique<Packet>(rangePacket.getName());
        for (const auto& item : context.ranges) {
            auto destinationOffset = packet->getDataLength();
            packet->insertAtBack(item.second->peekData());
            packet->copyRegionTags(*item.second, b(0), destinationOffset, item.second->getDataLength());
        }
        reservedBytes -= key.size;
        contexts.erase(found);
        return {COMPLETED, std::move(packet)};
    }
    return {restart ? RESTARTED : STORED, nullptr};
}

int LowpanReassemblyTable::expire(simtime_t now)
{
    int expired = 0;
    for (auto i = contexts.begin(); i != contexts.end();) {
        if (i->second.expiry <= now) {
            if (!i->second.quarantined)
                reservedBytes -= i->first.size;
            i = contexts.erase(i);
            expired++;
        }
        else
            ++i;
    }
    return expired;
}

void LowpanReassemblyTable::clear()
{
    contexts.clear();
    reservedBytes = 0;
}

simtime_t LowpanReassemblyTable::getNextExpiry() const
{
    simtime_t next = SimTime::getMaxTime();
    for (const auto& item : contexts)
        next = std::min(next, item.second.expiry);
    return next;
}

} } // namespace inet::lowpan
