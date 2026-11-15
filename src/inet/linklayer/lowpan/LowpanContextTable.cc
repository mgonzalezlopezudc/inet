// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanContextTable.h"

namespace inet { namespace lowpan {

bool LowpanContextTable::install(int id, const Ipv6Address& prefix, int prefixLength,
        bool allowCompression, simtime_t validUntil)
{
    if (id < 0 || id >= 16 || prefixLength < 0 || prefixLength > 128 ||
        prefix != prefix.getPrefix(prefixLength) || prefix.isMulticast() || validUntil < SIMTIME_ZERO)
        throw cRuntimeError("Invalid LoWPAN compression context");
    const auto& old = contexts[id];
    bool replaced = !old || old->prefix != prefix || old->prefixLength != prefixLength;
    contexts[id] = Context{prefix, prefixLength, allowCompression, validUntil};
    return replaced;
}

const LowpanContextTable::Context *LowpanContextTable::find(int id, simtime_t now) const
{
    if (id < 0 || id >= 16 || !contexts[id] || contexts[id]->validUntil <= now)
        return nullptr;
    return &*contexts[id];
}

} } // namespace inet::lowpan
