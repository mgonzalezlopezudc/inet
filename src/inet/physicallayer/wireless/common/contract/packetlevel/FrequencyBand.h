//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_FREQUENCYBAND_H
#define __INET_FREQUENCYBAND_H

#include <algorithm>
#include <cmath>
#include <vector>

#include "inet/common/Units.h"

namespace inet {
namespace physicallayer {

using namespace inet::units::values;

/**
 * An immutable occupied RF band.  Bands are represented by their center and
 * positive bandwidth; the lower and upper edges are derived from those two
 * values.  This type deliberately has no dependency on a radio technology.
 */
struct INET_API FrequencyBand
{
    const Hz centerFrequency;
    const Hz bandwidth;

    FrequencyBand(Hz centerFrequency, Hz bandwidth) :
        centerFrequency(centerFrequency),
        bandwidth(bandwidth)
    {
        if (!std::isfinite(centerFrequency.get()) || !std::isfinite(bandwidth.get()) || bandwidth <= Hz(0))
            throw cRuntimeError("A frequency band requires a finite center frequency and positive finite bandwidth");
    }

    Hz getLowerFrequency() const { return centerFrequency - bandwidth / 2; }
    Hz getUpperFrequency() const { return centerFrequency + bandwidth / 2; }

    bool contains(const FrequencyBand& other) const {
        return getLowerFrequency() <= other.getLowerFrequency() && other.getUpperFrequency() <= getUpperFrequency();
    }
};

/**
 * Returns a low-to-high copy of a band set and rejects overlapping bands.
 * Touching bands are allowed; they do not leave a spectral gap.
 */
inline std::vector<FrequencyBand> normalizeFrequencyBands(const std::vector<FrequencyBand>& bands)
{
    if (bands.empty())
        throw cRuntimeError("At least one occupied frequency band is required");

    std::vector<size_t> order(bands.size());
    for (size_t i = 0; i < bands.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&] (size_t i, size_t j) {
        if (bands[i].getLowerFrequency() != bands[j].getLowerFrequency())
            return bands[i].getLowerFrequency() < bands[j].getLowerFrequency();
        return bands[i].getUpperFrequency() < bands[j].getUpperFrequency();
    });

    std::vector<FrequencyBand> result;
    result.reserve(bands.size());
    for (size_t index : order) {
        const auto& band = bands[index];
        if (!result.empty() && result.back().getUpperFrequency() > band.getLowerFrequency())
            throw cRuntimeError("Occupied frequency bands must not overlap");
        result.emplace_back(band.centerFrequency, band.bandwidth);
    }
    return result;
}

inline Hz getFrequencyBandLower(const std::vector<FrequencyBand>& bands)
{
    return normalizeFrequencyBands(bands).front().getLowerFrequency();
}

inline Hz getFrequencyBandUpper(const std::vector<FrequencyBand>& bands)
{
    auto normalized = normalizeFrequencyBands(bands);
    return normalized.back().getUpperFrequency();
}

inline Hz getFrequencyBandEnvelopeCenter(const std::vector<FrequencyBand>& bands)
{
    auto normalized = normalizeFrequencyBands(bands);
    return (normalized.front().getLowerFrequency() + normalized.back().getUpperFrequency()) / 2;
}

inline Hz getFrequencyBandEnvelopeBandwidth(const std::vector<FrequencyBand>& bands)
{
    auto normalized = normalizeFrequencyBands(bands);
    return normalized.back().getUpperFrequency() - normalized.front().getLowerFrequency();
}

inline Hz getFrequencyBandTotalBandwidth(const std::vector<FrequencyBand>& bands)
{
    auto normalized = normalizeFrequencyBands(bands);
    Hz result = Hz(0);
    for (const auto& band : normalized)
        result += band.bandwidth;
    return result;
}

inline bool containsFrequencyBandSet(const std::vector<FrequencyBand>& container, const std::vector<FrequencyBand>& requested)
{
    auto normalizedContainer = normalizeFrequencyBands(container);
    auto normalizedRequested = normalizeFrequencyBands(requested);
    for (const auto& requestedBand : normalizedRequested) {
        bool contained = false;
        for (const auto& containerBand : normalizedContainer) {
            if (containerBand.contains(requestedBand)) {
                contained = true;
                break;
            }
        }
        if (!contained)
            return false;
    }
    return true;
}

} // namespace physicallayer
} // namespace inet

#endif
