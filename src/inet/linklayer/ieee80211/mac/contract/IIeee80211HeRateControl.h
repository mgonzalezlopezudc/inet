//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEEE80211HERATECONTROL_H
#define __INET_IIEEE80211HERATECONTROL_H

#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

/**
 * HE-specific rate adaptation contract shared by SU rate selection and HE MU
 * schedulers. Implementations may ignore fields that are not relevant to the
 * active PPDU type, but must always return a standard-valid MCS/NSS pair.
 */
class INET_API IIeee80211HeRateControl
{
  public:
    struct Constraints {
        int minMcs = 0;
        int maxMcs = 11;
        bool ldpc = false;
        bool dcm = false;
        bool extendedRangeSu = false;
    };

    struct Selection {
        const physicallayer::IIeee80211Mode *mode = nullptr;
        int mcs = 0;
        int numberOfSpatialStreams = 1;
        bool dcm = false;
        bool probing = false;
    };

  public:
    virtual ~IIeee80211HeRateControl() {}

    virtual Selection selectHeMode(const MacAddress& peer, Hz bandwidth, int ruToneSize,
            uint8_t ppduFormat, int maxNss, const Constraints& constraints) = 0;
    virtual void reportHeTxResult(const MacAddress& peer, int mcs, int numberOfSpatialStreams,
            int ruToneSize, int retryCount, bool success, int64_t ackedBytes) = 0;
    virtual void reportHeRxSnir(const MacAddress& peer, double snirDb) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif

