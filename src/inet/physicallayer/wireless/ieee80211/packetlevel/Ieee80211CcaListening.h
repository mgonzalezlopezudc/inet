//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211CCALISTENING_H
#define __INET_IEEE80211CCALISTENING_H

#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"
#include "inet/physicallayer/wireless/ieee80211/contract/Ieee80211CcaSnapshot.h"

namespace inet {
namespace physicallayer {

/**
 * Marker listening mask used only by the IEEE 802.11 radio's CCA sampler.
 * Keeping the marker typed prevents a normal receiver listening mask from
 * being classified with CCA thresholds.
 */
class INET_API Ieee80211CcaListening : public BandListening
{
  protected:
    const Ieee80211CcaGroup group;
    const bool legacyHt40;

  public:
    Ieee80211CcaListening(const IRadio *radio, simtime_t startTime, simtime_t endTime,
            Coord startPosition, Coord endPosition, Hz centerFrequency, Hz bandwidth,
            Ieee80211CcaGroup group, bool legacyHt40 = false) :
        BandListening(radio, startTime, endTime, startPosition, endPosition, centerFrequency, bandwidth),
        group(group), legacyHt40(legacyHt40) {}

    Ieee80211CcaGroup getGroup() const { return group; }
    bool isLegacyHt40() const { return legacyHt40; }
};

} // namespace physicallayer
} // namespace inet

#endif
