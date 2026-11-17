// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanContextTable.h"

namespace inet { namespace lowpan {

bool LowpanContextTable::install(int id, const Ipv6Address& prefix, int prefixLength,
        bool allowCompression, simtime_t validUntil)
{
    return install(id, prefix, prefixLength, allowCompression, validUntil, validUntil);
}

bool LowpanContextTable::install(int id, const Ipv6Address& prefix, int prefixLength,
        bool allowCompression, simtime_t compressionUntil, simtime_t validUntil)
{
    if (id < 0 || id >= 16 || prefixLength < 0 || prefixLength > 128 ||
        prefix != prefix.getPrefix(prefixLength) || prefix.isMulticast() || compressionUntil < SIMTIME_ZERO ||
        compressionUntil > validUntil)
        throw cRuntimeError("Invalid LoWPAN compression context");
    const auto& old = contexts[id];
    bool replaced = !old || old->prefix != prefix || old->prefixLength != prefixLength;
    contexts[id] = Context{prefix, prefixLength, allowCompression, compressionUntil, validUntil};
    return replaced;
}

const LowpanContextTable::Context *LowpanContextTable::findForCompression(int id, simtime_t now) const
{
    auto context = find(id, now);
    return context && context->allowCompression && context->compressionUntil > now ? context : nullptr;
}

const LowpanContextTable::Context *LowpanContextTable::get(int id) const
{
    return id >= 0 && id < 16 && contexts[id] ? &*contexts[id] : nullptr;
}

const LowpanContextTable::Context *LowpanContextTable::find(int id, simtime_t now) const
{
    if (id < 0 || id >= 16 || !contexts[id] || contexts[id]->validUntil <= now)
        return nullptr;
    return &*contexts[id];
}

} } // namespace inet::lowpan
