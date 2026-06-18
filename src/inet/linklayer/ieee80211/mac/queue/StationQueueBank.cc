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

StationQueueBank::~StationQueueBank()
{
    clear();
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
    // TODO: Implement detailed queue statistics collection
    // This requires accessing the queue internals which depends on the queue implementation
    qs.dropCount = stats.dropped[ac];
    
    return qs;
}

int StationQueueBank::getTotalQueuedPackets() const
{
    // TODO: Implement by accessing queue internals
    int total = 0;
    for (int ac = AC_BK; ac <= AC_VO; ac++) {
        // For now, we can't access queue length through IPacketQueue interface
        // This will be implemented when queue iteration is available
    }
    return total;
}

int StationQueueBank::getTotalQueuedBytes() const
{
    // TODO: Implement by accessing queue internals
    int total = 0;
    for (int ac = AC_BK; ac <= AC_VO; ac++) {
        // For now, we can't access queue packets through IPacketQueue interface
        // This will be implemented when queue iteration is available
    }
    return total;
}

void StationQueueBank::clear()
{
    for (int ac = AC_BK; ac <= AC_VO; ac++) {
        while (!queues[ac]->isEmpty()) {
            Packet *pkt = check_and_cast<Packet *>(queues[ac]->dequeuePacket());
            delete pkt;
            stats.dropped[ac]++;
        }
    }
}

} /* namespace ieee80211 */
} /* namespace inet */
