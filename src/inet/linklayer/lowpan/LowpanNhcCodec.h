// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANNHCCODEC_H
#define __INET_LOWPANNHCCODEC_H

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/lowpan/LowpanIphcCodec.h"

namespace inet { namespace lowpan {

// Header-only transformation: payload views and their region tags stay with the caller.
class INET_API LowpanNhcCodec
{
  public:
    struct Result {
        Ptr<const Chunk> header;
        int originalLength = 0;
        int encodedLength = 0;
    };
    static Result encode(const Packet& packet, const Ieee802154Address& source,
            const Ieee802154Address& destination, const LowpanContextTable *contexts, simtime_t now);
    static Result decode(const Packet& packet, int originalSize, const Ieee802154Address& source,
            const Ieee802154Address& destination, const LowpanContextTable *contexts, simtime_t now);
};

} }
#endif
