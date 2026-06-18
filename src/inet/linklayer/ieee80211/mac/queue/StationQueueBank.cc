//
// Copyright (C) 2024 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/queue/StationQueueBank.h"
#include "inet/linklayer/ieee80211/mac/queue/OrigEnqueueTimeTag_m.h"

#include "inet/common/ProtocolUtils.h"

namespace inet {
namespace ieee80211 {

Define_Module(StationQueueBank);

StationQueueBank::~StationQueueBank()
{
}

void StationQueueBank::initialize()
{
    // Parse the MAC address from the parameter
    const char *staAddrStr = par("staAddress").stringValue();
    staAddress.setAddress(staAddrStr);
    
    // Get references to the four queue submodules
    queues[AC_BK] = check_and_cast<queueing::IPacketQueue *>(getSubmodule("queueBK"));
    queues[AC_BE] = check_and_cast<queueing::IPacketQueue *>(getSubmodule("queueBE"));
    queues[AC_VI] = check_and_cast<queueing::IPacketQueue *>(getSubmodule("queueVI"));
    queues[AC_VO] = check_and_cast<queueing::IPacketQueue *>(getSubmodule("queueVO"));
    
    ASSERT(queues[AC_BK] && queues[AC_BE] && queues[AC_VI] && queues[AC_VO]);
}

std::string StationQueueBank::str() const
{
    std::ostringstream oss;
    oss << "StationQueueBank(" << staAddress << ")";
    return oss.str();
}

StationQueueBank::AccessCategory StationQueueBank::tidToAc(int tid)
{
    // IEEE 802.11e TID to AC mapping
    switch (tid) {
        case 1:
        case 2:
            return AC_BK;
        case 0:
        case 3:
            return AC_BE;
        case 4:
        case 5:
            return AC_VI;
        case 6:
        case 7:
            return AC_VO;
        default:
            throw cRuntimeError("Invalid TID: %d", tid);
    }
}

const char *StationQueueBank::acName(AccessCategory ac)
{
    switch (ac) {
        case AC_BK:
            return "AC_BK";
        case AC_BE:
            return "AC_BE";
        case AC_VI:
            return "AC_VI";
        case AC_VO:
            return "AC_VO";
        default:
            return "Unknown";
    }
}

StationQueueBank::QueueStats StationQueueBank::getQueueStats(AccessCategory ac) const
{
    ASSERT(ac >= AC_BK && ac <= AC_VO);
    
    QueueStats qs = {};
    auto queue = queues[ac];
    qs.packetCount = queue->getNumPackets();
    qs.byteCount = queue->getTotalLength().get<B>();
    if (!queue->isEmpty()) {
        auto packet = queue->getPacket(0);
        auto enqueueTimeTag = packet->findTag<OrigEnqueueTimeTag>();
        qs.holEnqueueTime = enqueueTimeTag == nullptr ? packet->getArrivalTime() : enqueueTimeTag->getEnqueueTime();
        qs.holDelay = simTime() - qs.holEnqueueTime;
        qs.holPacketSize = packet->getByteLength();
    }
    qs.dropCount = stats.dropped[ac];
    
    return qs;
}

int StationQueueBank::getTotalQueuedPackets() const
{
    int total = 0;
    for (int ac = AC_BK; ac <= AC_VO; ac++)
        total += queues[ac]->getNumPackets();
    return total;
}

int StationQueueBank::getTotalQueuedBytes() const
{
    int total = 0;
    for (int ac = AC_BK; ac <= AC_VO; ac++)
        total += queues[ac]->getTotalLength().get<B>();
    return total;
}

void StationQueueBank::clear()
{
    for (int ac = AC_BK; ac <= AC_VO; ac++) {
        if (queues[ac] == nullptr)
            continue;
        while (!queues[ac]->isEmpty()) {
            Packet *pkt = check_and_cast<Packet *>(queues[ac]->dequeuePacket());
            delete pkt;
            stats.dropped[ac]++;
        }
    }
}

} /* namespace ieee80211 */
} /* namespace inet */
