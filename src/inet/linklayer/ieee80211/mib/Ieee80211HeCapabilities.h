//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HECAPABILITIES_H
#define __INET_IEEE80211HECAPABILITIES_H

#include <algorithm>
#include <array>
#include <set>

#include "inet/common/Units.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

/** Per-spatial-stream maximum HE MCS map; -1 denotes an unsupported stream. */
struct Ieee80211HeMcsNssMap
{
    std::array<int, 8> maxMcsPerNss;

    Ieee80211HeMcsNssMap()
    {
        maxMcsPerNss.fill(-1);
        maxMcsPerNss[0] = 11;
    }
};

/**
 * HE capability set used by management negotiation and MU scheduling.
 *
 * It covers the subset of IEEE 802.11ax capabilities that affects the INET
 * packet-level model: channel widths, MCS/NSS maps, OFDMA, coding, A-MPDU,
 * Block Ack, puncturing, and supported RU sizes.
 */
struct Ieee80211HeCapabilities
{
    std::set<Hz> supportedChannelWidths = {Hz(20e6), Hz(40e6), Hz(80e6), Hz(160e6)};
    Ieee80211HeMcsNssMap rxMcsNss;
    Ieee80211HeMcsNssMap txMcsNss;
    bool dlOfdma = true;
    bool ulOfdma = true;
    bool dcm = true;
    int maxDcmConstellation = 4;
    int maxDcmNss = 2;
    bool ldpc = false;
    bool preamblePuncturing = true;
    bool multiTidAggregationRx = false;
    bool multiTidAggregationTx = false;
    bool muBarTriggerRx = true;
    bool heTbBlockAckTx = true;
    int maxAmpduLengthExponent = 7;
    int maxMpduLength = 11454;
    int maxBlockAckBufferSize = 64;
    std::set<int> supportedRuToneSizes = {26, 52, 106, 242, 484, 996, 1992};
    bool dlMuMimoBeamformer = false;
    bool dlMuMimoBeamformee = false;
    int soundingDimensions = 0;
    int beamformeeSts20Mhz = 0;
    int beamformeeStsAbove20Mhz = 0;
    int feedbackMode = 0;
};

/** HE operating parameters advertised by an AP after capability negotiation. */
struct Ieee80211HeOperation
{
    uint8_t bssColor = 0;
    Hz operatingChannelWidth = Hz(20e6);
    int basicHeMcsNss = 0;
    bool defaultPeDurationPresent = false;
    int defaultPeDurationUs = 0;
};

/** Usable HE feature set and operation produced for a local/peer association. */
struct Ieee80211NegotiatedHeCapabilities
{
    Ieee80211HeCapabilities intersection;
    Ieee80211HeOperation operation;
    bool valid = false;
};

/**
 * Computes directional MCS/NSS maps and mutual HE capabilities.
 *
 * The result is valid only when downlink OFDMA, the requested operating width,
 * at least one RU size, and one transmit stream are mutually supported.
 */
inline Ieee80211NegotiatedHeCapabilities negotiateHeCapabilities(
        const Ieee80211HeCapabilities& local,
        const Ieee80211HeCapabilities& peer,
        const Ieee80211HeOperation& operation)
{
    Ieee80211NegotiatedHeCapabilities negotiated;
    negotiated.operation = operation;
    negotiated.intersection.supportedChannelWidths.clear();
    for (const auto& width : local.supportedChannelWidths)
        if (peer.supportedChannelWidths.count(width) != 0)
            negotiated.intersection.supportedChannelWidths.insert(width);
    negotiated.intersection.rxMcsNss.maxMcsPerNss.fill(-1);
    negotiated.intersection.txMcsNss.maxMcsPerNss.fill(-1);
    for (size_t i = 0; i < 8; ++i) {
        int localTx = local.txMcsNss.maxMcsPerNss[i];
        int peerRx = peer.rxMcsNss.maxMcsPerNss[i];
        int localRx = local.rxMcsNss.maxMcsPerNss[i];
        int peerTx = peer.txMcsNss.maxMcsPerNss[i];
        negotiated.intersection.txMcsNss.maxMcsPerNss[i] =
                localTx < 0 || peerRx < 0 ? -1 : std::min(localTx, peerRx);
        negotiated.intersection.rxMcsNss.maxMcsPerNss[i] =
                localRx < 0 || peerTx < 0 ? -1 : std::min(localRx, peerTx);
    }
    negotiated.intersection.dlOfdma = local.dlOfdma && peer.dlOfdma;
    negotiated.intersection.ulOfdma = local.ulOfdma && peer.ulOfdma;
    negotiated.intersection.dcm = local.dcm && peer.dcm;
    negotiated.intersection.maxDcmConstellation =
            std::min(local.maxDcmConstellation, peer.maxDcmConstellation);
    negotiated.intersection.maxDcmNss = std::min(local.maxDcmNss, peer.maxDcmNss);
    negotiated.intersection.ldpc = local.ldpc && peer.ldpc;
    negotiated.intersection.preamblePuncturing = local.preamblePuncturing && peer.preamblePuncturing;
    negotiated.intersection.multiTidAggregationRx =
            local.multiTidAggregationRx && peer.multiTidAggregationTx;
    negotiated.intersection.multiTidAggregationTx =
            local.multiTidAggregationTx && peer.multiTidAggregationRx;
    negotiated.intersection.muBarTriggerRx = local.muBarTriggerRx && peer.muBarTriggerRx;
    negotiated.intersection.heTbBlockAckTx = local.heTbBlockAckTx && peer.heTbBlockAckTx;
    negotiated.intersection.maxAmpduLengthExponent =
            std::min(local.maxAmpduLengthExponent, peer.maxAmpduLengthExponent);
    negotiated.intersection.maxMpduLength = std::min(local.maxMpduLength, peer.maxMpduLength);
    negotiated.intersection.maxBlockAckBufferSize =
            std::min(local.maxBlockAckBufferSize, peer.maxBlockAckBufferSize);
    negotiated.intersection.supportedRuToneSizes.clear();
    for (int toneSize : local.supportedRuToneSizes)
        if (peer.supportedRuToneSizes.count(toneSize) != 0)
            negotiated.intersection.supportedRuToneSizes.insert(toneSize);
    negotiated.intersection.dlMuMimoBeamformer = local.dlMuMimoBeamformer;
    negotiated.intersection.dlMuMimoBeamformee = peer.dlMuMimoBeamformee;
    negotiated.intersection.soundingDimensions = local.soundingDimensions;
    negotiated.intersection.beamformeeSts20Mhz = peer.beamformeeSts20Mhz;
    negotiated.intersection.beamformeeStsAbove20Mhz = peer.beamformeeStsAbove20Mhz;
    negotiated.intersection.feedbackMode = peer.feedbackMode;
    negotiated.valid = negotiated.intersection.dlOfdma &&
            negotiated.intersection.supportedChannelWidths.count(operation.operatingChannelWidth) != 0 &&
            !negotiated.intersection.supportedRuToneSizes.empty() &&
            negotiated.intersection.txMcsNss.maxMcsPerNss[0] >= 0;
    return negotiated;
}

inline int getMaxNss(const Ieee80211HeMcsNssMap& map)
{
    int maxNss = 0;
    for (int i = 0; i < 8; ++i) {
        if (map.maxMcsPerNss[i] >= 0)
            maxNss = i + 1;
    }
    return maxNss;
}

inline bool isDlMuMimoEligible(
        const Ieee80211HeCapabilities& apCapabilities,
        const Ieee80211HeCapabilities& staCapabilities,
        const Ieee80211NegotiatedHeCapabilities& negotiated,
        Hz operatingBandwidth,
        int numApAntennas)
{
    if (!apCapabilities.dlMuMimoBeamformer)
        return false;
    if (!staCapabilities.dlMuMimoBeamformee)
        return false;
    if (apCapabilities.supportedChannelWidths.count(operatingBandwidth) == 0 ||
        staCapabilities.supportedChannelWidths.count(operatingBandwidth) == 0)
        return false;
    if (apCapabilities.soundingDimensions < 2 || numApAntennas < 2)
        return false;
    if (staCapabilities.feedbackMode != 2 && staCapabilities.feedbackMode != 3)
        return false;

    int negotiatedNss = getMaxNss(negotiated.intersection.txMcsNss);
    if (negotiatedNss < 1)
        return false;

    if (operatingBandwidth == Hz(20e6)) {
        if (staCapabilities.beamformeeSts20Mhz < negotiatedNss)
            return false;
    } else {
        if (staCapabilities.beamformeeStsAbove20Mhz < negotiatedNss)
            return false;
    }
    return true;
}

} // namespace ieee80211
} // namespace inet

#endif
