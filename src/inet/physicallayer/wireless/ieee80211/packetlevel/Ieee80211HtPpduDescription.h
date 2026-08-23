//
// Copyright (C) 2026
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HTPPDUDESCRIPTION_H
#define __INET_IEEE80211HTPPDUDESCRIPTION_H

#include <cstdint>

#include "inet/common/Units.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtSignalField.h"

namespace inet {
namespace physicallayer {

using namespace inet::units::values;

/**
 * Receiver-independent context needed to interpret an HT PPDU.
 *
 * Only copied values are retained by Ieee80211HtPpduDescription; no band,
 * channel, mode, or receiver object is owned by the canonical description.
 */
struct INET_API Ieee80211HtPpduContext
{
    Ieee80211HtMode::BandMode bandMode = Ieee80211HtMode::BAND_2_4GHZ;
    Ieee80211HtPreambleMode::HighTroughputPreambleFormat preambleFormat = Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED;
    int channelNumber = -1;
    Hz centerFrequency = Hz(0);
    Hz bandwidth = MHz(20);
};

/**
 * Immutable canonical interpretation of a valid HT-SIG and its HT-LTF
 * consequences. Local receiver capabilities are deliberately absent.
 *
 * The HT-SIG field is retained as a value so every received field, including
 * reserved-for-local-support values such as LDPC, NESS, MCS 32, and UEQM, is
 * available to later capability gates after successful canonicalization.
 */
class INET_API Ieee80211HtPpduDescription final
{
  private:
    const Ieee80211HtPpduContext context;
    const Ieee80211HtSignalField signalField;
    const int numberOfSpatialStreams;
    const int numberOfSpaceTimeStreams;
    const int numberOfDataLongTrainingFields;
    const int numberOfExtensionLongTrainingFields;
    const int numberOfLongTrainingFields;

    Ieee80211HtPpduDescription(const Ieee80211HtPpduContext& context,
        const Ieee80211HtSignalField& signalField, int numberOfSpatialStreams,
        int numberOfSpaceTimeStreams, int numberOfDataLongTrainingFields,
        int numberOfExtensionLongTrainingFields, int numberOfLongTrainingFields);

    friend class Ieee80211PhyModeResolver;

  public:
    const Ieee80211HtPpduContext& getContext() const { return context; }
    const Ieee80211HtSignalField& getSignalField() const { return signalField; }

    Ieee80211HtMode::BandMode getBandMode() const { return context.bandMode; }
    Ieee80211HtPreambleMode::HighTroughputPreambleFormat getPreambleFormat() const { return context.preambleFormat; }
    int getChannelNumber() const { return context.channelNumber; }
    Hz getCenterFrequency() const { return context.centerFrequency; }
    Hz getBandwidth() const { return context.bandwidth; }

    uint8_t getMcs() const { return signalField.mcs; }
    bool getCbw() const { return signalField.cbw; }
    B getLengthField() const { return B(signalField.length); }
    bool getSmoothing() const { return signalField.smoothing; }
    bool getNotSounding() const { return signalField.notSounding; }
    bool getReserved() const { return signalField.reserved; }
    bool getAggregation() const { return signalField.aggregation; }
    uint8_t getStbc() const { return signalField.stbc; }
    bool getFecCoding() const { return signalField.fecCoding; }
    bool getShortGi() const { return signalField.shortGi; }
    uint8_t getNumberOfExtensionSpatialStreams() const { return signalField.numberOfExtensionSpatialStreams; }
    uint8_t getCrc() const { return signalField.crc; }
    uint8_t getTail() const { return signalField.tail; }

    int getNumberOfSpatialStreams() const { return numberOfSpatialStreams; }
    int getNumberOfSpaceTimeStreams() const { return numberOfSpaceTimeStreams; }
    int getNumberOfDataLongTrainingFields() const { return numberOfDataLongTrainingFields; }
    int getNumberOfExtensionLongTrainingFields() const { return numberOfExtensionLongTrainingFields; }
    int getNumberOfLongTrainingFields() const { return numberOfLongTrainingFields; }

    // Standard abbreviations are useful at PHY call sites and are aliases of
    // the descriptive accessors above, not independent stored state.
    int getNss() const { return getNumberOfSpatialStreams(); }
    int getNsts() const { return getNumberOfSpaceTimeStreams(); }
    int getDltf() const { return getNumberOfDataLongTrainingFields(); }
    int getEltf() const { return getNumberOfExtensionLongTrainingFields(); }
    int getLtf() const { return getNumberOfLongTrainingFields(); }
};

} // namespace physicallayer
} // namespace inet

#endif
