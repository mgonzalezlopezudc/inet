// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANFRAGMENTFORWARDING_H
#define __INET_LOWPANFRAGMENTFORWARDING_H
#include <deque>
#include <map>
#include <set>
#include "inet/common/ModuleRefByPar.h"
#include "inet/linklayer/lowpan/LowpanReassemblyTable.h"
#include "inet/linklayer/lowpan/LowpanPacketDropDetails_m.h"
#include "inet/linklayer/lowpan/LowpanDispatchPrefix.h"
#include "inet/linklayer/ieee802154/Ieee802154AddressTagBase_m.h"
#include "inet/networklayer/ipv6/Ipv6Header_m.h"

namespace inet {
class Icmpv6;
class Ipv6RoutingTable;
namespace lowpan {
class LowpanLayer;
class LowpanIpv6;

class INET_API LowpanFragmentForwarding : public cListener
{
  protected:
    using Key = LowpanReassemblyTable::Key;
    enum Mode { ORDINARY, LOCAL, FORWARD, CLOSED };
    struct Path {
        int interfaceModuleId = -1, layerModuleId = -1;
        Ipv6Address nextHop;
        Ieee802154AddressReq peer;
        bool operator==(const Path& other) const;
    };
    struct Range {
        int offset;
        std::unique_ptr<Packet> data;
        simtime_t deadline;
        bool carry = false;
    };
    struct Record {
        Mode mode = ORDINARY;
        int64_t token = -1, inFlight = -1;
        simtime_t activeEnd, historyEnd, nextSend;
        Path path;
        uint16_t outgoingTag = 0;
        int inputEnd = 0, outputEnd = 0, stagedCount = 0;
        Ptr<Ipv6Header> header;
        Ptr<const Chunk> prefix;
        std::vector<uint8_t> firstWire;
        std::map<int, int> shapes;
        std::deque<Range> pending;
    };
    LowpanLayer& owner;
    ModuleRefByPar<Ipv6RoutingTable> routes;
    ModuleRefByPar<Icmpv6> icmp;
    ModuleRefByPar<LowpanIpv6> ipv6;
    opp_component_ptr<cModule> notifications;
    std::set<Ieee802154Address> peers;
    std::map<Key, Record> records;
    cMessage *timer = nullptr;
    int maxRecords, maxRanges, maxPendingBytes, maxCarry, maxBytes, emissionCap, maxHops;
    simtime_t residence, gap, activeLifetime, historyLifetime;
    bool findPath(const Ipv6Address& source, const Ipv6Address& destination, Path& path) const;
    bool isPathValid(const Record& record) const;
    bool inspect(const Packet& packet, bool& control) const;
    bool decodeFirst(const Packet& packet, int size, const Ieee802154Address& source,
            const Ieee802154Address& destination, LowpanDispatchPrefix::State state,
            std::unique_ptr<Packet>& original) const;
    void sendError(const Packet& quote, int type, int code) const;
    bool append(const Key& key, int offset, std::unique_ptr<Packet> data);
    void sendNext(const Key& key);
    void close(const Key& key, std::optional<LowpanDropReason> failure = std::nullopt);
    void expire();
    void schedule();
    int computeBufferedBytes() const;
    std::optional<Key> findToken(int64_t token) const;
    std::optional<Key> findIdentity(const Key& key) const;
    virtual void receiveSignal(cComponent *source, simsignal_t signal, cObject *object, cObject *details) override;
  public:
    explicit LowpanFragmentForwarding(LowpanLayer& owner);
    ~LowpanFragmentForwarding();
    void validatePeers();
    bool isTimer(const cMessage *message) const { return message == timer; }
    void handleTimer();
    // True consumes input. False preserves the input for ordinary reassembly.
    bool process(Packet *packet, bool first, const Ieee802154Address& source,
            const Ieee802154Address& destination, LowpanDispatchPrefix::State state);
    void handleReassemblyChanged(const Key& key);
    void handleTransmissionCompleted(int64_t token, int64_t frameId, bool successful);
    void handleInterfaceUnavailable(int moduleId);
    int getNumRecords() const { return records.size(); }
    int getBufferedBytes() const { return computeBufferedBytes(); }
};
} }
#endif
