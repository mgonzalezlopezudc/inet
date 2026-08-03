// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef __INET_IIEEE80211VHTPACKETRADIO_H
#define __INET_IIEEE80211VHTPACKETRADIO_H

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"

namespace inet {
namespace physicallayer {

struct Ieee80211VhtMuRxSelection
{
    bool active = false;
    uint8_t groupId = 0;
    uint8_t userPosition = 0;
    units::values::Hz channelWidth = units::values::Hz(0);
};

class INET_API IIeee80211VhtPacketRadio
{
  public:
    virtual ~IIeee80211VhtPacketRadio() = default;
    virtual int getVhtAntennaCount() const = 0;
    virtual units::values::Hz getVhtChannelWidth() const = 0;
    virtual Ieee80211VhtMuRxSelection getVhtMuRxSelection() const = 0;
    virtual void setVhtMuRxSelection(const Ieee80211VhtMuRxSelection& selection) = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
