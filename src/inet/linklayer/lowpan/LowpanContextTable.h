// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANCONTEXTTABLE_H
#define __INET_LOWPANCONTEXTTABLE_H

#include <array>
#include <optional>

#include "inet/common/INETDefs.h"
#include "inet/networklayer/contract/ipv6/Ipv6Address.h"

namespace inet { namespace lowpan {

// RFC 6282 context identifiers are four bits. Validity and permission to
// compress are distinct: a retiring context can still decompress packets.
class INET_API LowpanContextTable
{
  public:
    struct Context {
        Ipv6Address prefix;
        int prefixLength = 0; // Leading IPv6 prefix bits, 0..128.
        bool allowCompression = false;
        simtime_t validUntil; // Absolute simulation time; maximum means no expiry.
    };

  protected:
    std::array<std::optional<Context>, 16> contexts;

  public:
    // Returns true when a new prefix interpretation replaces the old one.
    bool install(int id, const Ipv6Address& prefix, int prefixLength,
            bool allowCompression, simtime_t validUntil);
    const Context *find(int id, simtime_t now) const;
};

} } // namespace inet::lowpan
#endif
