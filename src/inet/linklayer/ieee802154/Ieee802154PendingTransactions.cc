// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154PendingTransactions.h"
#include <algorithm>

namespace inet {

Ieee802154PendingTransactions::Ieee802154PendingTransactions(size_t capacity) : capacity(capacity)
{
    if (capacity == 0)
        throw cRuntimeError("IEEE 802.15.4 pending transaction capacity must be positive");
}

bool Ieee802154PendingTransactions::insert(const Entry& entry)
{
    if (!entry.frame || entry.destination.isNone())
        throw cRuntimeError("IEEE 802.15.4 pending transaction needs a frame and destination");
    for (const auto& existing : entries)
        if (existing.token == entry.token)
            throw cRuntimeError("Duplicate IEEE 802.15.4 pending transaction token");
    // IEEE Std 802.15.4-2024, 10.22.2: overflow does not replace an older transaction.
    if (entries.size() == capacity)
        return false;
    entries.push_back(entry);
    return true;
}

std::optional<Ieee802154PendingTransactions::Entry> Ieee802154PendingTransactions::first(
        uint16_t panId, const Ieee802154Address& destination, simtime_t now) const
{
    // IEEE Std 802.15.4-2024, 10.22.2: FIFO for each device, independent of other devices.
    for (const auto& entry : entries)
        if (entry.panId == panId && entry.destination == destination && now < entry.expiresAt)
            return entry;
    return {};
}

size_t Ieee802154PendingTransactions::count(uint16_t panId, const Ieee802154Address& destination, simtime_t now) const
{
    size_t result = 0;
    for (const auto& entry : entries)
        if (entry.panId == panId && entry.destination == destination && now < entry.expiresAt)
            ++result;
    return result;
}

std::vector<Ieee802154Address> Ieee802154PendingTransactions::addresses(uint16_t panId, simtime_t now) const
{
    std::vector<Ieee802154Address> result;
    for (const auto& entry : entries)
        if (entry.panId == panId && now < entry.expiresAt && !entry.destination.isBroadcast() &&
                !entry.destination.isExtendedGroup() && std::find(result.begin(),result.end(),entry.destination) == result.end())
            result.push_back(entry.destination);
    // IEEE Std 802.15.4-2024, 7.3.1.6: short addresses precede extended addresses.
    std::stable_partition(result.begin(),result.end(),[](const auto& address) { return address.isShort(); });
    if (result.size() > 7)
        result.resize(7);
    return result;
}

std::optional<Ieee802154PendingTransactions::Entry> Ieee802154PendingTransactions::remove(uint64_t token)
{
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (it->token == token) {
            auto result = *it;
            entries.erase(it);
            return result;
        }
    }
    return {};
}

std::vector<Ieee802154PendingTransactions::Entry> Ieee802154PendingTransactions::expire(simtime_t now)
{
    std::vector<Entry> result;
    for (auto it = entries.begin(); it != entries.end();) {
        if (it->expiresAt <= now) {
            result.push_back(*it);
            it = entries.erase(it);
        }
        else
            ++it;
    }
    return result;
}

std::vector<Ieee802154PendingTransactions::Entry> Ieee802154PendingTransactions::clear()
{
    std::vector<Entry> result(entries.begin(), entries.end());
    entries.clear();
    return result;
}

std::vector<Ieee802154PendingTransactions::Entry> Ieee802154PendingTransactions::removeFor(
        uint16_t panId, const Ieee802154Address& destination)
{
    std::vector<Entry> result;
    for (auto it = entries.begin(); it != entries.end();) {
        if (it->panId == panId && it->destination == destination) {
            result.push_back(*it);
            it = entries.erase(it);
        }
        else
            ++it;
    }
    return result;
}

std::optional<simtime_t> Ieee802154PendingTransactions::nextExpiry() const
{
    std::optional<simtime_t> result;
    for (const auto& entry : entries)
        if (!result || entry.expiresAt < *result)
            result = entry.expiresAt;
    return result;
}

} // namespace inet
