//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211EESM_H
#define __INET_IEEE80211EESM_H

#include <map>
#include <string>
#include <vector>

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

/** Numerically stable EESM and Patidar BCC beta calibration helper. */
class INET_API Ieee80211Eesm
{
  protected:
    struct Key {
        std::string calibrationSet;
        int bandwidthMHz;
        int mcs;

        bool operator<(const Key& other) const {
            if (calibrationSet != other.calibrationSet)
                return calibrationSet < other.calibrationSet;
            if (bandwidthMHz != other.bandwidthMHz)
                return bandwidthMHz < other.bandwidthMHz;
            return mcs < other.mcs;
        }
    };

    struct Beta {
        int dataCarrierCount;
        double value;
    };

    std::map<Key, Beta> betas;

  public:
    static double computeEffectiveSnr(const std::vector<double>& carrierSnr, double beta);
    static double computeEffectiveSnrDb(const std::vector<double>& carrierSnr, double beta);
    static bool hasSignificantInteriorVariation(double minimum, double maximum);

    void clear() { betas.clear(); }
    void loadBetaCsv(const std::string& filename, const std::string& calibrationSet);
    void requireSha256(const std::string& filename, const std::string& expectedSha256) const;
    double getBeta(const std::string& calibrationSet, int bandwidthMHz, int mcs, int dataCarrierCount) const;
    bool empty() const { return betas.empty(); }
};

} // namespace physicallayer
} // namespace inet

#endif
