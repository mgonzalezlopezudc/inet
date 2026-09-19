// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANREASSEMBLYTABLE_H
#define __INET_LOWPANREASSEMBLYTABLE_H

#include <map>
#include <memory>
#include <tuple>
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee802154/Ieee802154Address.h"

namespace inet { namespace lowpan {

class INET_API LowpanReassemblyTable
{
  public:
    struct Key {
        int interfaceId;
        uint16_t sourcePan;
        uint16_t destinationPan;
        Ieee802154Address source;
        Ieee802154Address destination;
        uint16_t size;
        uint16_t tag;
        bool operator<(const Key& other) const;
    };
    enum Status { STORED, COMPLETED, DUPLICATE, RESTARTED, CONFLICT, QUARANTINED, INVALID, RESOURCE_LIMIT };
    struct Result {
        Status status;
        std::unique_ptr<Packet> packet;
    };

  protected:
    struct Context {
        simtime_t expiry;
        bool firstSeen = false;
        bool quarantined = false;
        int covered = 0;
        std::map<int, std::unique_ptr<Packet>> ranges;
    };
    std::map<Key, Context> contexts;
    int maxContexts;
    int maxBytes;
    int reservedBytes = 0;
    simtime_t timeout;

  public:
    LowpanReassemblyTable(int maxContexts, int maxBytes, simtime_t timeout);
    // Original-coordinate data only. The caller retains rangePacket ownership.
    Result accept(const Key& key, int offset, bool first, const Packet& rangePacket, simtime_t now);
    int expire(simtime_t now);
    void clear();
    simtime_t getNextExpiry() const;
    int getNumContexts() const { return contexts.size(); }
    int getReservedBytes() const { return reservedBytes; }
};

} } // namespace inet::lowpan
#endif
