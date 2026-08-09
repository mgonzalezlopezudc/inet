//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HTTXVECTOR_H
#define __INET_IEEE80211HTTXVECTOR_H

#include <memory>

#include "inet/common/Units.h"

namespace inet {
namespace physicallayer {

/**
 * Immutable packet-level HT sounding metadata.
 *
 * This is not a sampled RF channel model. It records the normative HT
 * TXVECTOR dimensions needed by packet-level sounding (IEEE Std 802.11-2024,
 * 19.3.13) while the selected Ieee80211HtMode remains authoritative for rate
 * and PPDU timing.
 */
class INET_API Ieee80211HtTxVector final
{
  protected:
    const units::values::Hz channelWidth;
    const bool sounding;
    const bool ndp;
    const uint8_t numberOfSpaceTimeStreams;
    const uint8_t numberOfHtLtfSymbols;

  public:
    Ieee80211HtTxVector(units::values::Hz channelWidth, bool sounding, bool ndp,
            uint8_t numberOfSpaceTimeStreams, uint8_t numberOfHtLtfSymbols) :
        channelWidth(channelWidth), sounding(sounding), ndp(ndp),
        numberOfSpaceTimeStreams(numberOfSpaceTimeStreams),
        numberOfHtLtfSymbols(numberOfHtLtfSymbols)
    {
        const int canonicalHtDltfCount = numberOfSpaceTimeStreams == 3 ? 4 : numberOfSpaceTimeStreams;
        if (numberOfSpaceTimeStreams < 1 || numberOfSpaceTimeStreams > 4 ||
                numberOfHtLtfSymbols != canonicalHtDltfCount)
            throw cRuntimeError("Packet-level HT sounding requires 1..4 NSTS and the canonical HT-DLTF count");
        if (ndp && !sounding)
            throw cRuntimeError("An HT NDP must be marked as a sounding PPDU");
    }

    units::values::Hz getChannelWidth() const { return channelWidth; }
    bool isSounding() const { return sounding; }
    bool isNdp() const { return ndp; }
    uint8_t getNumberOfSpaceTimeStreams() const { return numberOfSpaceTimeStreams; }
    uint8_t getNumberOfHtLtfSymbols() const { return numberOfHtLtfSymbols; }
};

} // namespace physicallayer
} // namespace inet

#endif
