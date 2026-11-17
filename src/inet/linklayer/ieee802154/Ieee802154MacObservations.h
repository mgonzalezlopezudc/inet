// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef INET_IEEE802154_MACOBSERVATIONS_H
#define INET_IEEE802154_MACOBSERVATIONS_H

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee802154/Ieee802154GtsValues.h"

namespace inet {

/** Borrowed signal details; bounds are physical simulation instants. */
class INET_API Ieee802154MacWindowObservation : public cObject
{
  public:
    simtime_t start;
    simtime_t end;
    bool outgoing = false;
    bool receive = false; // Applicable to CFP windows.
    uint16_t shortAddress = 0xffff; // Applicable to CFP windows.
    Ieee802154GtsDirection direction = Ieee802154GtsDirection::DEVICE_TRANSMIT;
};

/** Borrowed coordinator GTS ledger/activity signal details. */
class INET_API Ieee802154MacGtsObservation : public cObject
{
  public:
    uint16_t shortAddress = 0xffff;
    uint8_t length = 0;
    Ieee802154GtsDirection direction = Ieee802154GtsDirection::DEVICE_TRANSMIT;
};

} // namespace inet
#endif
