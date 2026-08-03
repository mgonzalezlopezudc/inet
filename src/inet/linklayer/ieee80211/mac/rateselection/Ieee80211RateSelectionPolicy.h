//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211RATESELECTIONPOLICY_H
#define __INET_IEEE80211RATESELECTIONPOLICY_H

#include <vector>

#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HtCapabilities.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211VhtCapabilities.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

class INET_API Ieee80211RateSelectionPolicy
{
  public:
    enum CandidateOrigin { EXPLICIT_CONFIGURATION, RATE_CONTROL, DEFAULT_SELECTION };

    struct Context {
        const physicallayer::Ieee80211ModeSet *modeSet;
        const std::vector<Ieee80211LegacyRate> *localOperationalRates;
        const std::vector<Ieee80211LegacyRate> *bssBasicRates;
        const std::vector<Ieee80211LegacyRate> *peerRates;
        const Ieee80211HtOperation *htOperation = nullptr;
        const Ieee80211VhtOperation *vhtOperation = nullptr;
    };

  protected:
    static bool containsRate(const std::vector<Ieee80211LegacyRate> *rates, int rate);
    static bool isLegacyMode(const Context& context, const physicallayer::IIeee80211Mode *mode);
    static bool hasCompatibleResponseModulationClass(const Context& context,
            const physicallayer::IIeee80211Mode *precedingMode,
            const physicallayer::IIeee80211Mode *responseMode);
    static const physicallayer::IIeee80211Mode *findFastestLegacy(const Context& context,
            const std::vector<Ieee80211LegacyRate> *requiredRates, units::values::bps maximumRate,
            const physicallayer::IIeee80211Mode *responseClassReferenceMode = nullptr);
    static const physicallayer::IIeee80211Mode *findFastestMandatory(const Context& context,
            units::values::bps maximumRate,
            const physicallayer::IIeee80211Mode *responseClassReferenceMode = nullptr);
    static const physicallayer::IIeee80211Mode *findFastestBasicAdvanced(const Context& context);

  public:
    static const physicallayer::IIeee80211Mode *selectUnicast(const Context& context,
            const physicallayer::IIeee80211Mode *candidate, CandidateOrigin origin);
    static const physicallayer::IIeee80211Mode *selectGroupOrControl(const Context& context,
            const physicallayer::IIeee80211Mode *candidate = nullptr,
            CandidateOrigin origin = DEFAULT_SELECTION);
    static const physicallayer::IIeee80211Mode *selectResponse(const Context& context,
            const physicallayer::IIeee80211Mode *precedingMode,
            const physicallayer::IIeee80211Mode *configuredMode = nullptr);
    static const physicallayer::IIeee80211Mode *selectBlockAck(const Context& context);
};

} // namespace ieee80211
} // namespace inet

#endif
