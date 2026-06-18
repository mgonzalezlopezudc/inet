//
// Copyright (C) 2024 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_STATIONQUEUEBANK_H
#define __INET_STATIONQUEUEBANK_H

#include "inet/common/ModuleRefByPar.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/queue/OrigEnqueueTimeTag_m.h"
#include "inet/queueing/contract/IPacketQueue.h"

namespace inet {
namespace ieee80211 {

/**
 * Manages per-STA, per-AC FIFO queues for HE OFDMA scheduling.
 * 
 * Contains exactly 4 queues:
 * - AC_BK: Background (TID 1, 2)
 * - AC_BE: Best Effort (TID 0, 3)
 * - AC_VI: Video (TID 4, 5)
 * - AC_VO: Voice (TID 6, 7)
 */
class INET_API StationQueueBank : public cModule
{
  public:
    // Access category enumeration
    enum AccessCategory {
        AC_BK = 0,  // Background
        AC_BE = 1,  // Best Effort
        AC_VI = 2,  // Video
        AC_VO = 3   // Voice
    };

    // Per-queue statistics
    struct QueueStats {
        int packetCount;
        int byteCount;
        simtime_t holDelay;      // Head-of-line packet delay
        simtime_t holEnqueueTime; // Enqueue time of first packet
        int holPacketSize;        // Size of first packet in bytes
        int dropCount;
    };

  protected:
    MacAddress staAddress;  // MAC address of the associated STA
    queueing::IPacketQueue *queues[4];  // Four AC queues
    
    // Statistics tracking
    struct {
        int enqueued[4] = {0};
        int dequeued[4] = {0};
        int dropped[4] = {0};
    } stats;

  protected:
    virtual void initialize() override;
    virtual std::string str() const override;

  public:
    virtual ~StationQueueBank();

    // Queue access methods
    virtual queueing::IPacketQueue *getQueue(AccessCategory ac) const { return queues[ac]; }
    virtual queueing::IPacketQueue *getQueueByTid(int tid) const { return queues[tidToAc(tid)]; }

    // Statistics methods
    virtual QueueStats getQueueStats(AccessCategory ac) const;
    virtual int getTotalQueuedPackets() const;
    virtual int getTotalQueuedBytes() const;

    // Helper methods
    static AccessCategory tidToAc(int tid);
    static const char *acName(AccessCategory ac);

    // Management
    virtual MacAddress getStaAddress() const { return staAddress; }
    virtual void clear();
};

} /* namespace ieee80211 */
} /* namespace inet */

#endif
