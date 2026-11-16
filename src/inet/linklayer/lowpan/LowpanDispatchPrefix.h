// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANDISPATCHPREFIX_H
#define __INET_LOWPANDISPATCHPREFIX_H
#include "inet/common/packet/Packet.h"
namespace inet { namespace lowpan {

// Packet-local outer grammar; it never scans NHC or extension-specific payload.
class INET_API LowpanDispatchPrefix
{
  public:
    enum Kind { MALFORMED, UNSUPPORTED_PAGE, UNKNOWN, IPV6, IPHC, MESH, BROADCAST, FRAG1, FRAGN, ESC };
    struct State {
        uint8_t page;
        bool seenPageOne;
    };
    struct Result {
        Kind kind;
        int pagingLength;
        State state;
    };
    static Result parse(const Packet& packet, int offset = 0, State state = {0, false});
};
} }
#endif
