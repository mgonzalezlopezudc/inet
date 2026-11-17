// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_ILOWPANMAC_H
#define __INET_ILOWPANMAC_H

#include "inet/linklayer/lowpan/LowpanTransmissionReq_m.h"

namespace inet { namespace lowpan {

/** Native adaptation-facing preparation; no compatibility identities escape here. */
class INET_API ILowpanMac
{
  public:
    virtual ~ILowpanMac() {}
    virtual Ptr<const LowpanTransmissionReq> prepareTransmission(const Ieee802154AddressReq& request) const = 0;
};

} } // namespace inet::lowpan
#endif
