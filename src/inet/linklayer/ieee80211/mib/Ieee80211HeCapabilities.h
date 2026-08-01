//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HECAPABILITIES_H
#define __INET_IEEE80211HECAPABILITIES_H

#include <algorithm>
#include <array>
#include <ostream>
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
        maxMcsPerNss.fill(11);
    }
};

/**
 * HE capability set used by management negotiation and MU scheduling.
 *
 * It covers the subset of IEEE 802.11ax capabilities that affects the INET
 * packet-level model: channel widths, MCS/NSS maps, OFDMA, coding, A-MPDU,
 * Block Ack, puncturing, and supported RU sizes.
 *
 * This is not an exhaustive reproduction of the HE Capabilities element
 * (Clause 9.4.2.247).  Fields that have no corresponding model behavior (e.g.
 * SU/MU beamforming feedback thresholds, Max NC, RX/TX HE Supp PPDU BW) are
 * omitted or represented by simplified parameters.
 */
struct Ieee80211HeCapabilities
{
    // HE MAC Capabilities Information: TWT requester/responder are bits 1/2
    // and broadcast TWT support is bit 18 (IEEE 802.11-2024, 9.4.2.247.2).
    bool twtRequester = false;
    bool twtResponder = false;
    bool broadcastTwt = false;
    int dynamicFragmentationLevel = 0;
    bool omControl = false;
    bool twoNav = false;
    bool erBss = false;
    bool ndpFeedbackReport = false;
    std::set<Hz> supportedChannelWidths = {Hz(20e6), Hz(40e6), Hz(80e6), Hz(160e6)};
    Ieee80211HeMcsNssMap rxMcsNss;
    Ieee80211HeMcsNssMap txMcsNss;
    bool dlOfdma = true;
    bool ulOfdma = true;
    bool dcm = true;
    int maxDcmConstellation = 4;
    int maxDcmNss = 2;
    bool ldpc = true;
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
    bool fullBandwidthUlMuMimo = false;
    bool partialBandwidthUlMuMimo = false;
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
    bool erSuDisable = true;
    bool defaultPeDurationPresent = false;
    int defaultPeDurationUs = 0;
};

/** Capabilities usable for one transmitter-to-receiver direction. */
struct Ieee80211HeDirectionalCapabilities
{
    bool valid = false;
    std::set<Hz> supportedChannelWidths;
    Ieee80211HeMcsNssMap mcsNss;
    bool ofdma = false;
    bool preamblePuncturing = false;
    bool multiTidAggregation = false;
    bool receiverCanReceiveMuBarTrigger = false;
    bool transmitterCanTransmitHeTbBlockAck = false;
    bool transmitterCanTransmitNdpFeedbackReport = false;
    bool transmitterCanTransmitFullBandwidthUlMuMimo = false;
    bool transmitterCanTransmitPartialBandwidthUlMuMimo = false;
    int receiverDynamicFragmentationLevel = 0;
    int receiverMaxAmpduLengthExponent = 0;
    int receiverMaxMpduLength = 0;
    int receiverMaxBlockAckBufferSize = 0;
    std::set<int> supportedRuToneSizes;
};

/** Symmetric capabilities which require support at both endpoints. */
struct Ieee80211HeMutualCapabilities
{
    bool dcm = false;
    int maxDcmConstellation = 0;
    int maxDcmNss = 0;
    bool ldpc = false;
    bool omControl = false;
    bool twoNav = false;
    bool erBss = false;
};

/** Auditable HE contracts and operation produced for a local/peer association. */
struct Ieee80211NegotiatedHeCapabilities
{
    Ieee80211HeCapabilities localAdvertisement;
    Ieee80211HeCapabilities peerAdvertisement;
    Ieee80211HeDirectionalCapabilities localTxPeerRx;
    Ieee80211HeDirectionalCapabilities localRxPeerTx;
    Ieee80211HeMutualCapabilities mutual;
    Ieee80211HeOperation operation;
};

/**
 * Computes explicit local-TX/peer-RX and local-RX/peer-TX contracts. The local
 * role determines whether a direction uses the advertised DL or UL OFDMA bit.
 */
inline Ieee80211NegotiatedHeCapabilities negotiateHeCapabilities(
        const Ieee80211HeCapabilities& local,
        const Ieee80211HeCapabilities& peer,
        const Ieee80211HeOperation& operation,
        bool localIsAccessPoint)
{
    Ieee80211NegotiatedHeCapabilities negotiated;
    negotiated.localAdvertisement = local;
    negotiated.peerAdvertisement = peer;
    negotiated.operation = operation;
    negotiated.localTxPeerRx.supportedChannelWidths.clear();
    negotiated.localRxPeerTx.supportedChannelWidths.clear();
    for (const auto& width : local.supportedChannelWidths) {
        if (peer.supportedChannelWidths.count(width) != 0) {
            negotiated.localTxPeerRx.supportedChannelWidths.insert(width);
            negotiated.localRxPeerTx.supportedChannelWidths.insert(width);
        }
    }
    negotiated.localTxPeerRx.mcsNss.maxMcsPerNss.fill(-1);
    negotiated.localRxPeerTx.mcsNss.maxMcsPerNss.fill(-1);
    for (size_t i = 0; i < 8; ++i) {
        int localTx = local.txMcsNss.maxMcsPerNss[i];
        int peerRx = peer.rxMcsNss.maxMcsPerNss[i];
        int localRx = local.rxMcsNss.maxMcsPerNss[i];
        int peerTx = peer.txMcsNss.maxMcsPerNss[i];
        negotiated.localTxPeerRx.mcsNss.maxMcsPerNss[i] =
                localTx < 0 || peerRx < 0 ? -1 : std::min(localTx, peerRx);
        negotiated.localRxPeerTx.mcsNss.maxMcsPerNss[i] =
                localRx < 0 || peerTx < 0 ? -1 : std::min(localRx, peerTx);
    }
    negotiated.localTxPeerRx.ofdma = localIsAccessPoint ?
            local.dlOfdma && peer.dlOfdma : local.ulOfdma && peer.ulOfdma;
    negotiated.localRxPeerTx.ofdma = localIsAccessPoint ?
            local.ulOfdma && peer.ulOfdma : local.dlOfdma && peer.dlOfdma;
    negotiated.mutual.dcm = local.dcm && peer.dcm;
    negotiated.mutual.maxDcmConstellation =
            std::min(local.maxDcmConstellation, peer.maxDcmConstellation);
    negotiated.mutual.maxDcmNss = std::min(local.maxDcmNss, peer.maxDcmNss);
    negotiated.mutual.ldpc = local.ldpc && peer.ldpc;
    negotiated.localTxPeerRx.preamblePuncturing = local.preamblePuncturing && peer.preamblePuncturing;
    negotiated.localRxPeerTx.preamblePuncturing = local.preamblePuncturing && peer.preamblePuncturing;
    negotiated.localTxPeerRx.multiTidAggregation =
            local.multiTidAggregationTx && peer.multiTidAggregationRx;
    negotiated.localRxPeerTx.multiTidAggregation =
            peer.multiTidAggregationTx && local.multiTidAggregationRx;
    negotiated.localTxPeerRx.receiverCanReceiveMuBarTrigger = peer.muBarTriggerRx;
    negotiated.localRxPeerTx.receiverCanReceiveMuBarTrigger = local.muBarTriggerRx;
    negotiated.localTxPeerRx.transmitterCanTransmitHeTbBlockAck = local.heTbBlockAckTx;
    negotiated.localRxPeerTx.transmitterCanTransmitHeTbBlockAck = peer.heTbBlockAckTx;
    negotiated.localTxPeerRx.transmitterCanTransmitNdpFeedbackReport = local.ndpFeedbackReport;
    negotiated.localRxPeerTx.transmitterCanTransmitNdpFeedbackReport = peer.ndpFeedbackReport;
    negotiated.localTxPeerRx.transmitterCanTransmitFullBandwidthUlMuMimo = local.fullBandwidthUlMuMimo;
    negotiated.localRxPeerTx.transmitterCanTransmitFullBandwidthUlMuMimo = peer.fullBandwidthUlMuMimo;
    negotiated.localTxPeerRx.transmitterCanTransmitPartialBandwidthUlMuMimo = local.partialBandwidthUlMuMimo;
    negotiated.localRxPeerTx.transmitterCanTransmitPartialBandwidthUlMuMimo = peer.partialBandwidthUlMuMimo;
    negotiated.localTxPeerRx.receiverDynamicFragmentationLevel = peer.dynamicFragmentationLevel;
    negotiated.localRxPeerTx.receiverDynamicFragmentationLevel = local.dynamicFragmentationLevel;
    negotiated.localTxPeerRx.receiverMaxAmpduLengthExponent = peer.maxAmpduLengthExponent;
    negotiated.localRxPeerTx.receiverMaxAmpduLengthExponent = local.maxAmpduLengthExponent;
    negotiated.localTxPeerRx.receiverMaxMpduLength = peer.maxMpduLength;
    negotiated.localRxPeerTx.receiverMaxMpduLength = local.maxMpduLength;
    negotiated.localTxPeerRx.receiverMaxBlockAckBufferSize = peer.maxBlockAckBufferSize;
    negotiated.localRxPeerTx.receiverMaxBlockAckBufferSize = local.maxBlockAckBufferSize;
    for (int toneSize : local.supportedRuToneSizes) {
        if (peer.supportedRuToneSizes.count(toneSize) != 0) {
            negotiated.localTxPeerRx.supportedRuToneSizes.insert(toneSize);
            negotiated.localRxPeerTx.supportedRuToneSizes.insert(toneSize);
        }
    }
    negotiated.mutual.omControl = local.omControl && peer.omControl;
    negotiated.mutual.twoNav = local.twoNav && peer.twoNav;
    negotiated.mutual.erBss = local.erBss && peer.erBss;
    negotiated.localTxPeerRx.valid =
            negotiated.localTxPeerRx.supportedChannelWidths.count(operation.operatingChannelWidth) != 0 &&
            negotiated.localTxPeerRx.mcsNss.maxMcsPerNss[0] >= 0;
    negotiated.localRxPeerTx.valid =
            negotiated.localRxPeerTx.supportedChannelWidths.count(operation.operatingChannelWidth) != 0 &&
            negotiated.localRxPeerTx.mcsNss.maxMcsPerNss[0] >= 0;
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

inline std::ostream& operator<<(std::ostream& os, const Ieee80211HeCapabilities& capabilities)
{
    os << "ldpc=" << (capabilities.ldpc ? "yes" : "no")
       << " dlOfdma=" << (capabilities.dlOfdma ? "yes" : "no")
       << " ulOfdma=" << (capabilities.ulOfdma ? "yes" : "no")
       << " twtReq=" << (capabilities.twtRequester ? "yes" : "no")
       << " twtResp=" << (capabilities.twtResponder ? "yes" : "no")
       << " bcastTwt=" << (capabilities.broadcastTwt ? "yes" : "no")
       << " multiTidRx=" << (capabilities.multiTidAggregationRx ? "yes" : "no")
       << " multiTidTx=" << (capabilities.multiTidAggregationTx ? "yes" : "no")
       << " dynFrag=" << capabilities.dynamicFragmentationLevel
       << " OMI=" << (capabilities.omControl ? "yes" : "no")
       << " twoNAV=" << (capabilities.twoNav ? "yes" : "no")
       << " ER-BSS=" << (capabilities.erBss ? "yes" : "no")
       << " NDP-FB=" << (capabilities.ndpFeedbackReport ? "yes" : "no")
       << " bf=" << (capabilities.dlMuMimoBeamformer ? "yes" : "no")
       << " bfee=" << (capabilities.dlMuMimoBeamformee ? "yes" : "no")
       << " ulMuMimo=" << (capabilities.fullBandwidthUlMuMimo ? "full" :
               capabilities.partialBandwidthUlMuMimo ? "partial" : "no")
       << " maxTxNss=" << getMaxNss(capabilities.txMcsNss)
       << " maxRxNss=" << getMaxNss(capabilities.rxMcsNss)
       << " ruSizes=" << capabilities.supportedRuToneSizes.size();
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const Ieee80211HeOperation& operation)
{
    os << "bssColor=" << (int)operation.bssColor
       << " width=" << operation.operatingChannelWidth
       << " erSu=" << (operation.erSuDisable ? "disabled" : "enabled")
       << " defaultPE=" << operation.defaultPeDurationUs << "us"
       << " basicMcsNss=" << operation.basicHeMcsNss;
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const Ieee80211NegotiatedHeCapabilities& capabilities)
{
    os << "localTxPeerRx=" << (capabilities.localTxPeerRx.valid ? "valid" : "invalid")
       << " localRxPeerTx=" << (capabilities.localRxPeerTx.valid ? "valid" : "invalid")
       << " ldpc=" << (capabilities.mutual.ldpc ? "yes" : "no")
       << " operation={" << capabilities.operation << "}";
    return os;
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

    int maxSupportedNss = std::min(getMaxNss(negotiated.localTxPeerRx.mcsNss),
            operatingBandwidth == Hz(20e6) ? staCapabilities.beamformeeSts20Mhz :
                                             staCapabilities.beamformeeStsAbove20Mhz);
    if (maxSupportedNss < 1)
        return false;
    return true;
}

} // namespace ieee80211
} // namespace inet

#endif
