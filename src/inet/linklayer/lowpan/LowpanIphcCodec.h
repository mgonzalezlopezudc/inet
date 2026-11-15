// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANIPHCCODEC_H
#define __INET_LOWPANIPHCCODEC_H

#include "inet/linklayer/ieee802154/Ieee802154Address.h"
#include "inet/linklayer/lowpan/LowpanContextTable.h"
#include "inet/linklayer/lowpan/LowpanIphcHeader_m.h"
#include "inet/networklayer/ipv6/Ipv6Header_m.h"

namespace inet { namespace lowpan {

class INET_API LowpanIphcCodec
{
  public:
    static int getInlineLength(const LowpanIphcHeader& header);
    static Ptr<const LowpanIphcHeader> encode(const Ipv6Header& ipv6,
            const Ieee802154Address& source, const Ieee802154Address& destination, bool udpNhc = false,
            const LowpanContextTable *contexts = nullptr, simtime_t now = SIMTIME_ZERO);
    static Ptr<Ipv6Header> decode(const LowpanIphcHeader& header, int originalSize,
            const Ieee802154Address& source, const Ieee802154Address& destination, bool udpNhc = false,
            const LowpanContextTable *contexts = nullptr, simtime_t now = SIMTIME_ZERO);
};

} } // namespace inet::lowpan
#endif
