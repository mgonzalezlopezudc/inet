//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211RADIOMEDIUM_H
#define __INET_IEEE80211RADIOMEDIUM_H

#include "inet/physicallayer/wireless/common/medium/RadioMedium.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuTag.h"

namespace inet {
namespace physicallayer {

class INET_API Ieee80211RadioMedium : public RadioMedium
{
  protected:
    virtual const Ieee80211HeMuRuAllocation *findHeMuAllocationForReceiver(const IRadio *receiver, const ITransmission *transmission) const;
    virtual const IReception *computeHeMuRuReception(const IRadio *receiver, const ITransmission *transmission, const Ieee80211HeMuRuAllocation& allocation) const;

  protected:
    virtual void addTransmission(const IRadio *transmitter, const ITransmission *transmission) override;
    virtual const IReception *computeReception(const IRadio *receiver, const ITransmission *transmission) const override;

  public:
};

} // namespace physicallayer
} // namespace inet

#endif // __INET_IEEE80211RADIOMEDIUM_H
