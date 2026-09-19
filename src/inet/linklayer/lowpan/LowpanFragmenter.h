// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANFRAGMENTER_H
#define __INET_LOWPANFRAGMENTER_H
#include <vector>
#include "inet/common/INETDefs.h"

namespace inet { namespace lowpan {

class INET_API LowpanFragmenter
{
  public:
    struct Range {
        int offset;
        int length;
    };
    // Ranges include reconstructed header coverage, never dispatch octets.
    static std::vector<Range> plan(int size, int firstBudget, int subsequentBudget,
            int encodedHeaderLength, int originalHeaderLength);
};

} } // namespace inet::lowpan
#endif
