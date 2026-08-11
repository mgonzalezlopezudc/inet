//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211TIMINGPROFILE_H
#define __INET_IEEE80211TIMINGPROFILE_H

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

/** Immutable OFDM timing constants shared by IEEE 802.11 PHY mode families. */
class INET_API Ieee80211TimingProfile
{
  protected:
    const simtime_t dftPeriod;
    const simtime_t longGuardInterval;
    const simtime_t mediumGuardInterval;
    const simtime_t shortGuardInterval;

  private:
    Ieee80211TimingProfile(simtime_t dftPeriod, simtime_t longGuardInterval,
            simtime_t mediumGuardInterval, simtime_t shortGuardInterval) :
        dftPeriod(dftPeriod),
        longGuardInterval(longGuardInterval),
        mediumGuardInterval(mediumGuardInterval),
        shortGuardInterval(shortGuardInterval)
    {
    }

  public:
    static const Ieee80211TimingProfile& getHtProfile()
    {
        // IEEE Std 802.11-2024 Table 19-6.
        static const Ieee80211TimingProfile profile(3.2E-6, 0.8E-6, SIMTIME_ZERO, 0.4E-6);
        return profile;
    }

    static const Ieee80211TimingProfile& getVhtProfile() { return getHtProfile(); }

    static const Ieee80211TimingProfile& getHeProfile()
    {
        // IEEE Std 802.11-2024 Table 27-13.
        static const Ieee80211TimingProfile profile(12.8E-6, 3.2E-6, 1.6E-6, 0.8E-6);
        return profile;
    }

    // IEEE Std 802.11be-2024 Table 36-18 specifies the same DFT and guard interval values for EHT.
    static const Ieee80211TimingProfile& getEhtProfile() { return getHeProfile(); }

    simtime_t getDftPeriod() const { return dftPeriod; }
    simtime_t getLongGuardInterval() const { return longGuardInterval; }
    simtime_t getMediumGuardInterval() const { return mediumGuardInterval; }
    simtime_t getShortGuardInterval() const { return shortGuardInterval; }
    simtime_t getLongGuardIntervalSymbolInterval() const { return dftPeriod + longGuardInterval; }
    simtime_t getMediumGuardIntervalSymbolInterval() const { return dftPeriod + mediumGuardInterval; }
    simtime_t getShortGuardIntervalSymbolInterval() const { return dftPeriod + shortGuardInterval; }

    bool operator==(const Ieee80211TimingProfile& other) const
    {
        return dftPeriod == other.dftPeriod &&
                longGuardInterval == other.longGuardInterval &&
                mediumGuardInterval == other.mediumGuardInterval &&
                shortGuardInterval == other.shortGuardInterval;
    }

    bool operator!=(const Ieee80211TimingProfile& other) const { return !(*this == other); }
};

} // namespace physicallayer
} // namespace inet

#endif
