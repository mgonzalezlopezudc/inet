//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HTCSICACHE_H
#define __INET_HTCSICACHE_H

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <map>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

enum class Ieee80211HtFeedbackKind : uint8_t {
    CSI = 1,
    NONCOMPRESSED_BEAMFORMING = 2,
    COMPRESSED_BEAMFORMING = 3,
};

/** Deterministic packet-level result of an HT-LTF measurement. */
struct Ieee80211HtCsiMeasurement
{
    bool valid = false;
    double minimumSnir = NaN;
    double meanSnir = NaN;
    double beamformingGainDb = 0;
    uint8_t recommendedMcs = 127;
    uint8_t numberOfRows = 1;
    uint8_t numberOfColumns = 1;
    uint8_t soundingNsts = 1;
    uint8_t numberOfHtLtfSymbols = 1;
    Ieee80211HtFeedbackKind feedbackKind = Ieee80211HtFeedbackKind::CSI;
    std::vector<uint8_t> reportBytes;
};

/**
 * Owner-neutral cache for packet-level HT sounding results.
 *
 * INET's packet-level channel has no per-subcarrier MIMO tensor. Consequently
 * this class deterministically quantizes received SNIR and authoritative HT
 * mode metadata into bounded report bytes; it does not claim waveform-level
 * channel-matrix fidelity (IEEE Std 802.11-2024, 19.3.13 and 10.33.3).
 */
class INET_API HtCsiCache
{
  protected:
    struct Key {
        MacAddress peer;
        Hz channelWidth = Hz(0);
        uint64_t associationGeneration = 0;
        uint8_t soundingNsts = 1;

        bool operator<(const Key& other) const
        {
            if (peer != other.peer) return peer < other.peer;
            if (channelWidth != other.channelWidth) return channelWidth < other.channelWidth;
            if (associationGeneration != other.associationGeneration)
                return associationGeneration < other.associationGeneration;
            return soundingNsts < other.soundingNsts;
        }
    };

    struct Entry {
        simtime_t acquisitionTime;
        simtime_t expiryTime;
        Ieee80211HtCsiMeasurement measurement;
        uint8_t requestToken = 0;
    };

    simtime_t validityDuration = SimTime(0.1);
    std::map<Key, Entry> entries;
    std::function<simtime_t()> timeProvider = []() { return simTime(); };

  public:
    void configure(simtime_t duration)
    {
        if (duration < SIMTIME_ZERO)
            throw cRuntimeError("HT CSI validity duration must not be negative");
        validityDuration = duration;
    }

    void setTimeProvider(std::function<simtime_t()> provider)
    {
        if (!provider)
            throw cRuntimeError("HT CSI time provider must be defined");
        timeProvider = std::move(provider);
    }

    static Ieee80211HtCsiMeasurement deriveMeasurement(double minimumSnir,
            double meanSnir, uint8_t recommendedMcs, uint8_t soundingNsts,
            uint8_t numberOfHtLtfSymbols, Ieee80211HtFeedbackKind kind)
    {
        if (!std::isfinite(minimumSnir) || !std::isfinite(meanSnir) ||
                minimumSnir <= 0 || meanSnir <= 0 || recommendedMcs > 76 ||
                soundingNsts < 1 || soundingNsts > 4 ||
                numberOfHtLtfSymbols < soundingNsts || numberOfHtLtfSymbols > 5)
            throw cRuntimeError("Invalid HT-LTF measurement inputs");
        Ieee80211HtCsiMeasurement result;
        result.valid = true;
        result.minimumSnir = minimumSnir;
        result.meanSnir = meanSnir;
        result.recommendedMcs = recommendedMcs;
        result.soundingNsts = soundingNsts;
        result.numberOfHtLtfSymbols = numberOfHtLtfSymbols;
        result.numberOfRows = soundingNsts;
        result.numberOfColumns = std::max<uint8_t>(1, soundingNsts - (kind == Ieee80211HtFeedbackKind::COMPRESSED_BEAMFORMING));
        result.feedbackKind = kind;
        const double snirDb = 10 * std::log10(std::max(minimumSnir, 1e-12));
        result.beamformingGainDb = std::clamp((10 * std::log10(meanSnir) - snirDb) +
                0.75 * (soundingNsts - 1), 0.0, 12.0);
        const size_t bytesPerStream = kind == Ieee80211HtFeedbackKind::CSI ? 4 :
                kind == Ieee80211HtFeedbackKind::NONCOMPRESSED_BEAMFORMING ? 3 : 2;
        result.reportBytes.resize(std::min<size_t>(64, bytesPerStream * soundingNsts));
        uint32_t state = static_cast<uint32_t>(std::llround((snirDb + 40) * 256)) ^
                (static_cast<uint32_t>(recommendedMcs) << 16) ^
                (static_cast<uint32_t>(soundingNsts) << 24) ^
                static_cast<uint8_t>(kind);
        for (auto& byte : result.reportBytes) {
            state = state * 1664525u + 1013904223u;
            byte = state >> 24;
        }
        return result;
    }

    void update(const MacAddress& peer, Hz width, uint64_t generation,
            uint8_t soundingNsts, uint8_t requestToken,
            const Ieee80211HtCsiMeasurement& measurement)
    {
        if (peer.isUnspecified() || width <= Hz(0) || generation == 0 ||
                requestToken > 6 || !measurement.valid ||
                measurement.soundingNsts != soundingNsts)
            throw cRuntimeError("Invalid HT CSI cache entry");
        auto now = timeProvider();
        entries[{peer, width, generation, soundingNsts}] =
                {now, now + validityDuration, measurement, requestToken};
    }

    const Ieee80211HtCsiMeasurement *findFresh(const MacAddress& peer, Hz width,
            uint64_t generation, uint8_t soundingNsts, int requestToken = -1) const
    {
        auto it = entries.find({peer, width, generation, soundingNsts});
        return it != entries.end() && timeProvider() <= it->second.expiryTime &&
                (requestToken < 0 || requestToken == it->second.requestToken) ?
                &it->second.measurement : nullptr;
    }

    /** Finds the newest valid measurement when the requester does not carry
     *  the association generation in the HT Control field. */
    const Ieee80211HtCsiMeasurement *findFresh(const MacAddress& peer, Hz width,
            uint8_t soundingNsts) const
    {
        const Entry *best = nullptr;
        for (const auto& [key, entry] : entries) {
            if (key.peer == peer && key.channelWidth == width &&
                    key.soundingNsts == soundingNsts &&
                    timeProvider() <= entry.expiryTime &&
                    (best == nullptr || entry.acquisitionTime > best->acquisitionTime))
                best = &entry;
        }
        return best == nullptr ? nullptr : &best->measurement;
    }

    void invalidatePeer(const MacAddress& peer)
    {
        for (auto it = entries.begin(); it != entries.end(); )
            it = it->first.peer == peer ? entries.erase(it) : std::next(it);
    }

    void clear() { entries.clear(); }
};

} // namespace ieee80211
} // namespace inet

#endif
