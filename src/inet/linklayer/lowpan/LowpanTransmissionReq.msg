// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.linklayer.ieee802154.Ieee802154AddressTagBase;

namespace inet::lowpan;

// Prepared native envelope. Payload limit is in octets, excluding MAC overhead.
class LowpanTransmissionReq extends inet::Ieee802154AddressReq
{
    int payloadLimit = -1;
    int profile = 0;
    simtime_t deliveryDeadline = -1; // Absolute latest arrival under the prepared lower profile.
}
