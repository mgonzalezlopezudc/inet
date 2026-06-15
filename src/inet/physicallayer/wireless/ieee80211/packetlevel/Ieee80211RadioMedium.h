//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211RADIOMEDIUM_H
#define __INET_IEEE80211RADIOMEDIUM_H

#include "inet/physicallayer/wireless/common/medium/RadioMedium.h"
#include <map>
#include <vector>

namespace inet {
namespace physicallayer {

class INET_API Ieee80211RadioMedium : public RadioMedium
{
  protected:
    std::map<const ITransmission *, std::vector<const ITransmission *>> muSubTransmissions;

  protected:
    virtual void addTransmission(const IRadio *transmitter, const ITransmission *transmission) override;
    virtual void removeTransmission(const ITransmission *transmission) override;
    virtual bool isPotentialReceiver(const IRadio *receiver, const ITransmission *transmission) const override;
    virtual bool isInterferingTransmission(const ITransmission *transmission, const IListening *listening) const override;
    virtual bool isInterferingTransmission(const ITransmission *transmission, const IReception *reception) const override;

  public:
    virtual void sendToRadio(IRadio *transmitter, const IRadio *receiver, const IWirelessSignal *signal) override;
    virtual void initialize(int stage) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signal, cObject *value, cObject *details) override;
    virtual ~Ieee80211RadioMedium();
};

} // namespace physicallayer
} // namespace inet

#endif // __INET_IEEE80211RADIOMEDIUM_H
