// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANSELECTIVERECOVERY_H
#define __INET_LOWPANSELECTIVERECOVERY_H

#include <array>
#include <deque>
#include <map>
#include "inet/common/ModuleRefByPar.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/lowpan/LowpanRecoveryTagAllocator.h"
#include "inet/linklayer/lowpan/LowpanRfragHeader_m.h"
#include "inet/linklayer/ieee802154/Ieee802154AddressTagBase_m.h"
#include "inet/networklayer/ipv6/Ipv6Header_m.h"

namespace inet {
class Ipv6RoutingTable;
class Icmpv6;
namespace lowpan {
class LowpanLayer;
class LowpanIpv6;
class LowpanRecoverySource;

// RFC 8931 compressed-coordinate state. Each instance belongs to one native Layer.
class INET_API LowpanSelectiveRecovery : public cListener
{
    friend class LowpanRecoverySource;
  protected:
    struct Key {
        Ieee802154Address source, destination;
        uint16_t pan;
        uint8_t tag;
        bool operator<(const Key& other) const;
    };
    struct Path {
        int layerId = -1, interfaceId = -1;
        Ipv6Address nextHop;
        Ieee802154AddressReq peer;
        bool operator==(const Path& other) const;
    };
    enum Phase { SOURCE, RECEIVER, RELAY, FULL, ABORT };
    struct Shape { int offset = -1, size = 0; };
    struct Frame {
        Ptr<LowpanRfragHeader> header;
        Ptr<const Chunk> body;
        simtime_t deadline;
        SharingRegionTagSet regions;
    };
    struct Record {
        Phase phase = ABORT;
        int64_t token = -1, inFlight = -1;
        simtime_t activeEnd, historyEnd, fullEnd, nextSend;
        int size = 0, headerLength = 0, delta = 0, inFlightBytes = 0;
        uint8_t outgoingTag = 0;
        Path path;
        Ptr<Ipv6Header> ipv6;
        Ptr<const Chunk> outputHeader;
        std::vector<uint8_t> firstBytes, bytes;
        std::vector<bool> covered;
        SharingRegionTagSet regions;
        std::array<Shape, 32> shapes{};
        std::array<Shape, 32> submittedShapes{};
        uint32_t bitmap = 0;
        bool marked = false;
        std::deque<Frame> pending;
    };
    struct Reverse {
        int ownerId;
        int64_t token;
        Ieee802154Address peer;
        uint16_t pan;
        bool nullOnly = false;
    };
    struct Control {
        Ieee802154AddressReq peer;
        Ptr<const Chunk> bytes;
        simtime_t deadline;
    };
    LowpanLayer& owner;
    ModuleRefByPar<Ipv6RoutingTable> routes;
    ModuleRefByPar<LowpanIpv6> ipv6;
    ModuleRefByPar<Icmpv6> icmp;
    opp_component_ptr<cModule> notifications;
    std::unique_ptr<LowpanRecoverySource> sources;
    LowpanRecoveryTagAllocator tags;
    std::map<Key, Record> records;
    std::map<uint8_t, Reverse> reverse;
    std::deque<Control> controls;
    std::map<int64_t, int> controlFrames;
    cMessage *timer = nullptr;
    int64_t nextToken = 0;
    int maxRecords, maxBytes, maxControls, maxPending, maxPendingBytes, payloadLimit, pathPayloadLimit;
    simtime_t residence, gap, incompleteLifetime, fullLifetime, historyLifetime, tagProtection;
    static simtime_t quantizeDuration(double seconds);
    static simtime_t addTime(simtime_t first, simtime_t second);
    std::optional<Key> findToken(int64_t token) const;
    int computeRetainedBytes() const;
    bool hasCapacity(int bytes, bool newRecord) const;
    bool findPath(const Ipv6Address& source, const Ipv6Address& destination, Path& path, int originalSize = 0) const;
    bool isPathValid(const Record& record) const;
    bool inspectTransit(const Packet& packet) const;
    bool decodeFirst(const Packet& packet, int size, const Key& key, Ptr<Ipv6Header>& header, int& length) const;
    void sendError(const Packet& quote, int type, int code, int mtu = 0) const;
    void sendMtuError(const Record& record) const;
    std::optional<uint8_t> allocateTag(int ownerId, int64_t token, const Ieee802154AddressReq& peer);
    void removeReverse(int layerId, uint8_t tag, int64_t token);
    void queueAck(const Key& key, uint32_t bitmap, bool marked = false);
    void queueControl(const Ieee802154AddressReq& peer, const Ptr<const Chunk>& bytes);
    void sendControls();
    void processAck(Packet *packet);
    void processReset(Packet *packet, const Ptr<const LowpanRfragHeader>& header, const Key& key);
    void acceptReceiver(const Key& key, const Ptr<const LowpanRfragHeader>& header, const Packet& packet);
    void acceptRelay(const Key& key, const Ptr<const LowpanRfragHeader>& header, const Packet& packet);
    void sendNext(const Key& key);
    void close(const Key& key, bool full, bool sendReset = false);
    void protect(const Key& key);
    void expire();
    void schedule();
    void handleAck(int64_t token, uint32_t bitmap, bool marked);
    void finishSource(int64_t token, bool successful);
    virtual void receiveSignal(cComponent *, simsignal_t signal, cObject *object, cObject *) override;
  public:
    explicit LowpanSelectiveRecovery(LowpanLayer& owner);
    ~LowpanSelectiveRecovery();
    bool isTimer(const cMessage *message) const { return message == timer; }
    void handleTimer();
    void admit(Packet *packet, const Ieee802154AddressReq& peer, int nativePayloadLimit);
    void process(Packet *packet, bool acknowledgement);
    void handleTransmissionCompleted(int64_t token, int64_t frameId, bool successful);
    void handleInterfaceUnavailable(int moduleId);
    int getNumRecords() const { return records.size(); }
    int getRetainedBytes() const { return computeRetainedBytes(); }
};
} }
#endif
