//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCPERSUCCESSMODEL_H
#define __INET_IEEE80211LDPCPERSUCCESSMODEL_H

#include "inet/common/Module.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211LdpcPerTable.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/contract/IIeee80211FecSuccessModel.h"

namespace inet {
namespace physicallayer {

class INET_API Ieee80211LdpcPerSuccessModel : public Module, public IIeee80211FecSuccessModel
{
  protected:
    Ieee80211LdpcPerTable table;

  protected:
    virtual void initialize(int stage) override;

  public:
    virtual double computeDataSuccessRate(const IIeee80211DataMode& mode,
            const Ieee80211DataEncodingPlan& plan, double snrDb) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
