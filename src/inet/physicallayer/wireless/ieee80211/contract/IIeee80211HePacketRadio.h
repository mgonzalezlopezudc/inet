// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef __INET_IIEEE80211HEPACKETRADIO_H
#define __INET_IIEEE80211HEPACKETRADIO_H

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

class INET_API IIeee80211HePacketRadio
{
  public:
    virtual ~IIeee80211HePacketRadio() = default;
    virtual uint8_t getHeBssColor() const = 0;
    virtual void setHeBssColor(uint8_t bssColor) = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
