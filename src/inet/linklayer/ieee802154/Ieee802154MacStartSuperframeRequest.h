// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACSTARTSUPERFRAMEREQUEST_H
#define __INET_IEEE802154MACSTARTSUPERFRAMEREQUEST_H

#include "inet/linklayer/ieee802154/Ieee802154MacBeaconRequest.h"

namespace inet {

/** Selected legacy START-SUPERFRAME parameters (IEEE 802.15.4-2024, Table 10-1). */
struct Ieee802154MacStartSuperframeRequest
{
    uint16_t panId;
    physicallayer::Ieee802154PhyChannel channel;
    uint32_t startTime = 0;
    uint8_t beaconOrder = 15;
    uint8_t superframeOrder = 15;
    bool panCoordinator = true;
    bool batteryLifeExtension = false;
    bool coordinatorRealignment = false;
    Ieee802154MacSecurityParameters beaconSecurity;
    Ieee802154MacSecurityParameters realignmentSecurity;
};

struct Ieee802154MacStartSuperframeConfirm { Ieee802154MacServiceResult result; };

} // namespace inet
#endif
