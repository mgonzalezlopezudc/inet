// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACORPHANRESPONSE_H
#define __INET_IEEE802154MACORPHANRESPONSE_H

#include "inet/linklayer/ieee802154/Ieee802154MacDataRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacStartRequest.h"

namespace inet {

struct Ieee802154MacOrphanResponse
{
    Ieee802154Address orphanAddress;
    uint16_t shortAddress = 0xfffe;
    bool associatedMember = false;
    Ieee802154MacSecurityParameters security;
};

struct Ieee802154MacOrphanIndication { Ieee802154Address orphanAddress; };

/** Native correlated COMM-STATUS carrier; local IGNORED denotes AssociatedMember=false without transmission. */
struct Ieee802154MacOrphanResponseStatus
{
    Ieee802154Address orphanAddress;
    Ieee802154MacServiceResult result;
};

enum class Ieee802154SyncLossReason { PAN_ID_CONFLICT = 0, REALIGNMENT = 1, BEACON_LOST = 2 };

struct Ieee802154MacSyncLossIndication
{
    Ieee802154SyncLossReason reason;
    uint16_t panId;
    physicallayer::Ieee802154PhyChannel channel;
    Ieee802154Address coordinatorAddress;
    std::optional<uint16_t> shortAddress;
    std::optional<uint16_t> coordinatorShortAddress;
};

} // namespace inet
#endif
