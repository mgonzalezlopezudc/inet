// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee802154/Ieee802154Address.h"

namespace inet {

static cNEDValue nedf_ieee802154LocalAddress(cComponent *context, cNEDValue[], int)
{
    return Ieee802154Address::fromExtended(0x0200000000000000ULL | static_cast<uint64_t>(context->getId())).str().c_str();
}

Define_NED_Function2(nedf_ieee802154LocalAddress, "string ieee802154LocalAddress()", "network",
        "Returns a locally administered IEEE 802.15.4 extended address unique to the calling component.");

} // namespace inet
