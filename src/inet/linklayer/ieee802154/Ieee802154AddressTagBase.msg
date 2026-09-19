// SPDX-License-Identifier: LGPL-3.0-or-later

import inet.common.INETDefs;
import inet.common.TagBase;
import inet.linklayer.ieee802154.Ieee802154Address;

namespace inet;

// Native identities; never compatibility MacAddress aliases or surrogate wire fields.
class Ieee802154AddressTagBase extends TagBase
{
    Ieee802154Address srcAddress;
    Ieee802154Address destAddress;
    uint16_t srcPanId = 0xffff;
    uint16_t destPanId = 0xffff;
}

class Ieee802154AddressReq extends Ieee802154AddressTagBase
{
}

class Ieee802154AddressInd extends Ieee802154AddressTagBase
{
}
