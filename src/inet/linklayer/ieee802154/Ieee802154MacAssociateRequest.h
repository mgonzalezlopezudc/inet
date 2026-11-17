// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACASSOCIATEREQUEST_H
#define __INET_IEEE802154MACASSOCIATEREQUEST_H

#include "inet/linklayer/ieee802154/Ieee802154MacAssociateResponse.h"
#include "inet/linklayer/ieee802154/Ieee802154MacStartRequest.h"

namespace inet {

struct Ieee802154MacAssociateRequest
{
    physicallayer::Ieee802154PhyChannel channel;
    uint16_t coordinatorPanId;
    Ieee802154Address coordinatorAddress;
    uint8_t capabilityInformation = 0x80; ///< Selected ordinary association; request short-address allocation.
    Ieee802154MacSecurityParameters security;
    bool coordinatorIsPanCoordinator = true; ///< Caller declaration; discovery supplies the advertised role.
};

struct Ieee802154MacAssociateIndication
{
    Ieee802154Address deviceAddress;
    uint8_t capabilityInformation;
};

struct Ieee802154MacAssociateConfirm
{
    uint16_t shortAddress = 0xffff;
    std::optional<uint8_t> associationStatus; ///< Present only after an actual Association Response.
    Ieee802154MacServiceResult result = Ieee802154MacStatus::SUCCESS;
};

} // namespace inet
#endif
