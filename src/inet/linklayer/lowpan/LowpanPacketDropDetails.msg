// SPDX-License-Identifier: LGPL-3.0-or-later
import inet.common.Simsignals;

namespace inet::lowpan;

enum LowpanDropReason
{
    LOWPAN_MALFORMED = 0;
    LOWPAN_UNSUPPORTED_DISPATCH = 1;
    LOWPAN_FRAGMENTATION_UNAVAILABLE = 2;
    LOWPAN_ENVELOPE_INVALIDATED = 3;
    LOWPAN_RESOURCE_LIMIT = 4;
    LOWPAN_ND_UNSUPPORTED = 5;
    LOWPAN_LOWER_LIFETIME_EXPIRED = 6;
    LOWPAN_REASSEMBLY_REJECTED = 7;
    LOWPAN_TAG_EXHAUSTED = 8;
}

class LowpanPacketDropDetails extends inet::PacketDropDetails
{
    LowpanDropReason lowpanReason;
}
