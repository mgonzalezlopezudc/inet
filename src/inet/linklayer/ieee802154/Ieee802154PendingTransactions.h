// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154PENDINGTRANSACTIONS_H
#define __INET_IEEE802154PENDINGTRANSACTIONS_H

#include <list>
#include <optional>
#include <vector>

#include "inet/linklayer/ieee802154/Ieee802154Frame_m.h"

namespace inet {

/** Immutable indirect frames; service packets and terminal callbacks remain with the MAC. */
class INET_API Ieee802154PendingTransactions
{
  public:
    struct Entry {
        uint64_t token;
        uint16_t panId;
        Ieee802154Address destination;
        Ptr<const Ieee802154Frame> frame;
        simtime_t expiresAt; ///< Absolute deadline; equality is expired.
    };

  private:
    size_t capacity;
    std::list<Entry> entries;

  public:
    explicit Ieee802154PendingTransactions(size_t capacity);
    bool insert(const Entry& entry);
    std::optional<Entry> first(uint16_t panId, const Ieee802154Address& destination, simtime_t now) const;
    size_t count(uint16_t panId, const Ieee802154Address& destination, simtime_t now) const;
    std::vector<Ieee802154Address> addresses(uint16_t panId, simtime_t now) const;
    std::optional<Entry> remove(uint64_t token);
    std::vector<Entry> expire(simtime_t now);
    std::vector<Entry> clear();
    std::vector<Entry> removeFor(uint16_t panId, const Ieee802154Address& destination);
    std::optional<simtime_t> nextExpiry() const;
    size_t size() const { return entries.size(); }
};

} // namespace inet
#endif
