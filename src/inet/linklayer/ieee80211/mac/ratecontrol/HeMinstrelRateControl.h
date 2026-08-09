//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEMINSTRELRATECONTROL_H
#define __INET_HEMINSTRELRATECONTROL_H

#include <map>
#include <string>
#include <vector>

#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HeRateControl.h"
#include "inet/linklayer/ieee80211/mac/ratecontrol/MinstrelRateControlBase.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"

namespace inet {
namespace ieee80211 {

/** Minstrel-style HE rate controller for SU and HE scheduler paths. */
class INET_API HeMinstrelRateControl : public MinstrelRateControlBase, public IIeee80211HeRateControl
{
  protected:
    bool enableExtendedRangeSu = false;
    bool preferDcm = false;
    std::string selectionPolicy;
    struct HeSelectionContext {
        const physicallayer::IIeee80211Mode *mode = nullptr;
        int ruToneSize = 0;
    };
    std::map<MacAddress, HeSelectionContext> lastSelections;

  protected:
    virtual void initialize(int stage) override;
    virtual bool isRateCandidate(const physicallayer::IIeee80211Mode *mode) const override;
    virtual int getModeMcs(const physicallayer::IIeee80211Mode *mode) const override;
    virtual int getModeNss(const physicallayer::IIeee80211Mode *mode) const override;

    const physicallayer::Ieee80211HeMode *findHeMode(int mcs, int nss, Hz bandwidth,
            bool extendedRangeSu, bool ldpc) const;
    int clampMcsForConstraints(int mcs, int ruToneSize, uint8_t ppduFormat,
            int maxNss, const Constraints& constraints) const;

  public:
    virtual const physicallayer::IIeee80211Mode *getRate() override;

    virtual Selection selectHeMode(const MacAddress& peer, Hz bandwidth, int ruToneSize,
            uint8_t ppduFormat, int maxNss, const Constraints& constraints) override;
    virtual void reportHeTxResult(const MacAddress& peer, int mcs, int numberOfSpatialStreams,
            int ruToneSize, int retryCount, bool success, int64_t ackedBytes) override;
    virtual void reportHeRxSnir(const MacAddress& peer, double snirDb) override;
    virtual void invalidatePeer(const MacAddress& peer) override
    {
        lastSelections.erase(peer);
        MinstrelRateControlBase::invalidatePeer(peer);
    }
};

} // namespace ieee80211
} // namespace inet

#endif
