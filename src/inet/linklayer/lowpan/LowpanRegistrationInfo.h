// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANREGISTRATIONINFO_H
#define __INET_LOWPANREGISTRATIONINFO_H

#include "inet/networklayer/icmpv6/Ipv6NdMessage_m.h"

namespace inet { namespace lowpan {

// Per-address ownership and transaction metadata, independent of link identity.
struct INET_API LowpanRegistrationInfo
{
    enum Recency { EQUAL, NEWER, OLDER, INCOMPARABLE };
    enum ParseResult { VALID, MALFORMED, UNSUPPORTED };
    std::vector<uint8_t> rovr;
    bool tidKnown = false;
    uint8_t tid = 0;
    bool reachability = true;
    uint8_t opaque = 0;

    static Recency compare(uint8_t incoming, uint8_t incumbent);
    static ParseResult parse(const Ipv6NdOption& option, bool updated, LowpanRegistrationInfo& result);
    Ipv6NdOption *makeOption(int status, int lifetimeMinutes, bool updatedReply) const;
    bool sameOwner(const LowpanRegistrationInfo& other) const { return rovr == other.rovr; }
    bool sameTransaction(const LowpanRegistrationInfo& other) const;
    bool sameParameters(const LowpanRegistrationInfo& other) const;
};

} }
#endif
