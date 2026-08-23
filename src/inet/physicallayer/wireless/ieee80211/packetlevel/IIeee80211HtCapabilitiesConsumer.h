//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEEE80211HTCAPABILITIESCONSUMER_H
#define __INET_IIEEE80211HTCAPABILITIESCONSUMER_H

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

class Ieee80211HtCapabilities;

/**
 * Initialization contract for radio submodules that consume the immutable
 * radio-owned HT capability set.
 */
class INET_API IIeee80211HtCapabilitiesConsumer
{
  public:
    virtual ~IIeee80211HtCapabilitiesConsumer() {}

    virtual void setHtCapabilities(const Ieee80211HtCapabilities *capabilities) = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
