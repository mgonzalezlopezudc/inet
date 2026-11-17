// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANRECOVERYSOURCE_H
#define __INET_LOWPANRECOVERYSOURCE_H

#include <array>
#include <map>
#include <optional>
#include "inet/linklayer/ieee802154/Ieee802154AddressTagBase_m.h"
#include "inet/common/packet/Packet.h"

namespace inet { namespace lowpan {
class LowpanSelectiveRecovery;
class INET_API LowpanRecoverySource
{
    friend class LowpanSelectiveRecovery;
  public:
    struct Range { int offset, size; };
    // Bounds apply to every source fragment, including the last. Empty means infeasible.
    static std::vector<Range> partition(int size, int header, int firstMaximum, int minimum, int optimum, int maximum);
  protected:
    struct Operation {
        std::unique_ptr<Packet> original;
        Ptr<const Chunk> header;
        Ieee802154AddressReq peer;
        std::vector<Range> ranges;
        std::array<uint8_t, 32> attempts{};
        uint32_t credit = 0;
        int batchBegin = 0, batchEnd = 0, cursor = 0, xSequence = -1;
        int64_t inFlight = -1;
        uint8_t tag = 0;
        int size = 0;
        bool waiting = false, waitFull = false, initialPass = true, probe = false;
        simtime_t deadline, nextSend, responseEnd;
    };
    LowpanSelectiveRecovery& recovery;
    std::map<int64_t, Operation> operations;
    int window, minimum, optimum, maximum;
    simtime_t operationLifetime, initialRto, minimumRto, maximumRto;
    void sendNext(int64_t token);
    void retire(int64_t token, bool successful);
    void advanceBatch(Operation& operation);
    bool isOperationValid(const Operation& operation) const;
    simtime_t computeResponseInterval(int attempts) const;
  public:
    explicit LowpanRecoverySource(LowpanSelectiveRecovery& recovery);
    void admit(Packet *packet, const Ieee802154AddressReq& peer, int nativePayloadLimit);
    void handleAck(int64_t token, uint32_t bitmap);
    void handleTransmissionCompleted(int64_t token, int64_t frameId, bool successful);
    void handleTimer();
    void clear();
    std::optional<simtime_t> getNextEvent() const;
    int getRetainedBytes() const;
};
} }
#endif
