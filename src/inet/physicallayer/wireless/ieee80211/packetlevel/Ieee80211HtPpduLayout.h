//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HTPPDULAYOUT_H
#define __INET_IEEE80211HTPPDULAYOUT_H

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtPpduDescription.h"

namespace inet {
namespace physicallayer {

/**
 * Canonical timing authority for the bounded HT-mixed PPDU implementation.
 *
 * IEEE Std 802.11-2024 19.3.9.2, 19.3.9.3.3--19.3.9.3.5,
 * 19.3.9.4.3, 19.3.9.4.5, and 19.3.9.4.6 define the field order and
 * durations. 19.3.11.11.6 defines the short-GI data symbol duration.
 */
class INET_API Ieee80211HtPpduLayout final
{
  private:
    int numberOfDataLongTrainingFields;
    simtime_t dataStart;
    simtime_t dataEnd;
    simtime_t dataSymbolDuration;
    int numberOfDataSymbols;

  public:
    Ieee80211HtPpduLayout(const Ieee80211HtPpduDescription& description,
        simtime_t ppduDuration);

    static simtime_t getLegacyShortTrainingEnd() { return SimTime(8, SIMTIME_US); }
    static simtime_t getLegacyLongTrainingEnd() { return SimTime(16, SIMTIME_US); }
    static simtime_t getLegacySignalEnd() { return SimTime(20, SIMTIME_US); }
    static simtime_t getFirstHtSignalSymbolEnd() { return SimTime(24, SIMTIME_US); }
    static simtime_t getRobustMixedPreambleEnd() { return SimTime(28, SIMTIME_US); }
    static simtime_t getHtShortTrainingEnd() { return SimTime(32, SIMTIME_US); }
    static simtime_t getHtLongTrainingFieldDuration() { return SimTime(4, SIMTIME_US); }

    int getNumberOfDataLongTrainingFields() const { return numberOfDataLongTrainingFields; }
    simtime_t getHtLongTrainingFieldStart(int index) const;
    simtime_t getHtLongTrainingFieldEnd(int index) const;
    simtime_t getDataStart() const { return dataStart; }
    simtime_t getDataEnd() const { return dataEnd; }
    simtime_t getDataSymbolDuration() const { return dataSymbolDuration; }
    int getNumberOfDataSymbols() const { return numberOfDataSymbols; }
};

} // namespace physicallayer
} // namespace inet

#endif
