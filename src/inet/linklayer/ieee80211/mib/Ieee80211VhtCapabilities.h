//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211VHTCAPABILITIES_H
#define __INET_IEEE80211VHTCAPABILITIES_H

#include <algorithm>
#include <array>
#include <set>
#include <cmath>
#include "inet/common/Units.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

struct Ieee80211VhtMcsNssMap
{
    std::array<int, 8> maxMcsPerNss;

    Ieee80211VhtMcsNssMap() { maxMcsPerNss.fill(9); }
};

/** Capabilities advertised by a VHT station for local configuration or negotiation. */
struct Ieee80211VhtCapabilities
{
    std::set<Hz> supportedChannelWidths = {MHz(20), MHz(40), MHz(80)};
    bool supports80Plus80MHz = false; // Requires two-segment geometry; scalar 160 is not sufficient.
    Ieee80211VhtMcsNssMap rxMcsNss;
    Ieee80211VhtMcsNssMap txMcsNss;
    bool rxLdpc = false;
    bool ldpc = false;
    bool stbc = false;
    int maxAmpduLengthExponent = 7; // up to 7 for VHT (1,048,575 bytes)
    int maxNss = 8;
    int maxMcs = 9;
    int rxHighestLongGiDataRateMbps = 0;
    int txHighestLongGiDataRateMbps = 0;
    int maxNstsTotal = 0; // 0 advertises the standard beamformee-STS fallback.
    bool shortGi80 = false;
    bool shortGi160 = false;
    bool suBeamformer = false;
    bool suBeamformee = false;
    int beamformeeSts = 2;
    int soundingDimensions = 2;
    bool muBeamformer = false;
    bool muBeamformee = false;
};

struct Ieee80211VhtDirectionalCapabilities
{
    bool valid = false;
    std::set<Hz> supportedChannelWidths;
    Ieee80211VhtMcsNssMap mcsNss;
    bool ldpc = false;
    bool shortGi80 = false;
    bool shortGi160 = false;
    bool suBeamforming = false;
    int soundingNsts = 0;
    int maxNstsTotal = 0;
    bool muMimo = false;
    int receiverMaxAmpduLengthExponent = 0;
};

/** VHT operating parameters selected for a BSS or an individual transmission. */
struct Ieee80211VhtOperation
{
    Hz operatingChannelWidth = Hz(20e6);
    int centerFrequencySegment0 = 0;
    int centerFrequencySegment1 = 0;
    bool nonContiguous = false;
    int numSpatialStreams = 1;
    bool shortGi = false;
    bool ldpc = false;
    Ieee80211VhtMcsNssMap basicMcsNss;

    Ieee80211VhtOperation() {
        basicMcsNss.maxMcsPerNss.fill(-1);
        basicMcsNss.maxMcsPerNss[0] = 7;
    }
};

inline int getIeee80211VhtChannelNumber(Hz centerFrequency)
{
    double centerMhz = centerFrequency.get<MHz>();
    double channel = centerMhz == 2484 ? 14 :
            centerMhz < 3000 ? (centerMhz - 2407) / 5 :
            centerMhz < 5925 ? (centerMhz - 5000) / 5 : (centerMhz - 5950) / 5;
    int centerChannel = std::lround(channel);
    if (std::abs(channel - centerChannel) > 1e-6 || centerChannel <= 0 || centerChannel > 255)
        throw cRuntimeError("VHT center frequency does not map to an IEEE 802.11 channel number");
    return centerChannel;
}

inline Ieee80211VhtOperation deriveIeee80211VhtOperation(Hz centerFrequency, Hz channelWidth,
        physicallayer::Ieee80211Primary80ChannelPosition primary80ChannelPosition =
                physicallayer::Ieee80211Primary80ChannelPosition::UNSPECIFIED)
{
    if (channelWidth != MHz(20) && channelWidth != MHz(40) &&
            channelWidth != MHz(80) && channelWidth != MHz(160))
        throw cRuntimeError("VHT operation requires a 20, 40, 80, or 160 MHz channel");
    int centerChannel = getIeee80211VhtChannelNumber(centerFrequency);
    Ieee80211VhtOperation operation;
    operation.operatingChannelWidth = channelWidth;
    if (channelWidth == MHz(160)) {
        // IEEE Std 802.11-2024, Tables 9-316 and 9-317: CCFS0 identifies the
        // primary 80 MHz segment and CCFS1 identifies the 160 MHz channel.
        if (primary80ChannelPosition == physicallayer::Ieee80211Primary80ChannelPosition::UNSPECIFIED)
            throw cRuntimeError("VHT 160 MHz operation requires an explicit primary 80 MHz channel position");
        operation.centerFrequencySegment0 = centerChannel +
                (primary80ChannelPosition == physicallayer::Ieee80211Primary80ChannelPosition::LOWER ? -8 : 8);
        operation.centerFrequencySegment1 = centerChannel;
        if (operation.centerFrequencySegment0 <= 0 || operation.centerFrequencySegment0 > 255)
            throw cRuntimeError("VHT 160 MHz center frequency cannot produce a legal CCFS0");
    }
    else
        operation.centerFrequencySegment0 = centerChannel;
    return operation;
}

inline Ieee80211VhtOperation deriveIeee80211Vht80Plus80Operation(Hz primarySegmentCenterFrequency,
        Hz secondarySegmentCenterFrequency)
{
    Ieee80211VhtOperation operation;
    operation.operatingChannelWidth = MHz(160);
    operation.centerFrequencySegment0 = getIeee80211VhtChannelNumber(primarySegmentCenterFrequency);
    operation.centerFrequencySegment1 = getIeee80211VhtChannelNumber(secondarySegmentCenterFrequency);
    operation.nonContiguous = true;
    auto separation = std::abs(operation.centerFrequencySegment1 - operation.centerFrequencySegment0);
    if (separation <= 16)
        throw cRuntimeError("VHT 80+80 MHz operation requires CCFS separation greater than 16 channel numbers");
    return operation;
}

/** Result of intersecting local and peer VHT capabilities for an operation. */
struct Ieee80211NegotiatedVhtCapabilities
{
    Ieee80211VhtCapabilities localAdvertisement;
    Ieee80211VhtCapabilities peerAdvertisement;
    Ieee80211VhtDirectionalCapabilities localTxPeerRx;
    Ieee80211VhtDirectionalCapabilities localRxPeerTx;
    Ieee80211VhtCapabilities intersection;
    Ieee80211VhtOperation operation;
    bool valid = false;
};

/**
 * Intersects symmetric VHT features and limits. The operation is copied
 * unchanged; callers are responsible for selecting an operating width that
 * both stations support.
 */
inline Ieee80211NegotiatedVhtCapabilities negotiateVhtCapabilities(
        const Ieee80211VhtCapabilities& local,
        const Ieee80211VhtCapabilities& peer,
        const Ieee80211VhtOperation& operation)
{
    Ieee80211NegotiatedVhtCapabilities negotiated;
    negotiated.localAdvertisement = local;
    negotiated.peerAdvertisement = peer;
    negotiated.operation = operation;
    negotiated.localTxPeerRx.supportedChannelWidths.clear();
    negotiated.localRxPeerTx.supportedChannelWidths.clear();
    for (const auto& width : local.supportedChannelWidths)
        if (peer.supportedChannelWidths.count(width)) {
            negotiated.localTxPeerRx.supportedChannelWidths.insert(width);
            negotiated.localRxPeerTx.supportedChannelWidths.insert(width);
        }
    negotiated.localTxPeerRx.mcsNss.maxMcsPerNss.fill(-1);
    negotiated.localRxPeerTx.mcsNss.maxMcsPerNss.fill(-1);
    for (size_t i = 0; i < 8; i++) {
        negotiated.localTxPeerRx.mcsNss.maxMcsPerNss[i] = local.txMcsNss.maxMcsPerNss[i] < 0 || peer.rxMcsNss.maxMcsPerNss[i] < 0 ? -1 : std::min(local.txMcsNss.maxMcsPerNss[i], peer.rxMcsNss.maxMcsPerNss[i]);
        negotiated.localRxPeerTx.mcsNss.maxMcsPerNss[i] = local.rxMcsNss.maxMcsPerNss[i] < 0 || peer.txMcsNss.maxMcsPerNss[i] < 0 ? -1 : std::min(local.rxMcsNss.maxMcsPerNss[i], peer.txMcsNss.maxMcsPerNss[i]);
    }
    negotiated.localTxPeerRx.ldpc = local.ldpc && peer.rxLdpc;
    negotiated.localRxPeerTx.ldpc = peer.ldpc && local.rxLdpc;
    negotiated.localTxPeerRx.shortGi80 = local.shortGi80 && peer.shortGi80;
    negotiated.localRxPeerTx.shortGi80 = negotiated.localTxPeerRx.shortGi80;
    negotiated.localTxPeerRx.shortGi160 = local.shortGi160 && peer.shortGi160;
    negotiated.localRxPeerTx.shortGi160 = negotiated.localTxPeerRx.shortGi160;
    negotiated.localTxPeerRx.suBeamforming = local.suBeamformer && peer.suBeamformee;
    negotiated.localRxPeerTx.suBeamforming = peer.suBeamformer && local.suBeamformee;
    negotiated.localTxPeerRx.soundingNsts = negotiated.localTxPeerRx.suBeamforming ?
            std::min(local.soundingDimensions, peer.beamformeeSts) : 0;
    negotiated.localRxPeerTx.soundingNsts = negotiated.localRxPeerTx.suBeamforming ?
            std::min(peer.soundingDimensions, local.beamformeeSts) : 0;
    negotiated.localTxPeerRx.muMimo = local.muBeamformer && peer.muBeamformee;
    negotiated.localRxPeerTx.muMimo = peer.muBeamformer && local.muBeamformee;
    // IEEE Std 802.11-2024, 9.4.2.156.3/Table 9-315: zero in Maximum
    // NSTS,total falls back to the Beamformee STS Capability value.
    negotiated.localTxPeerRx.maxNstsTotal = negotiated.localTxPeerRx.muMimo ?
            (peer.maxNstsTotal == 0 ? peer.beamformeeSts : peer.maxNstsTotal) : 0;
    negotiated.localRxPeerTx.maxNstsTotal = negotiated.localRxPeerTx.muMimo ?
            (local.maxNstsTotal == 0 ? local.beamformeeSts : local.maxNstsTotal) : 0;
    negotiated.localTxPeerRx.receiverMaxAmpduLengthExponent = peer.maxAmpduLengthExponent;
    negotiated.localRxPeerTx.receiverMaxAmpduLengthExponent = local.maxAmpduLengthExponent;
    bool localTxPeerRxWidthValid = negotiated.localTxPeerRx.supportedChannelWidths.count(operation.operatingChannelWidth) != 0 &&
            (!operation.nonContiguous || (local.supports80Plus80MHz && peer.supports80Plus80MHz));
    bool localRxPeerTxWidthValid = negotiated.localRxPeerTx.supportedChannelWidths.count(operation.operatingChannelWidth) != 0 &&
            (!operation.nonContiguous || (local.supports80Plus80MHz && peer.supports80Plus80MHz));
    negotiated.localTxPeerRx.valid = localTxPeerRxWidthValid && negotiated.localTxPeerRx.mcsNss.maxMcsPerNss[0] >= 0;
    negotiated.localRxPeerTx.valid = localRxPeerTxWidthValid && negotiated.localRxPeerTx.mcsNss.maxMcsPerNss[0] >= 0;
    negotiated.intersection.ldpc = negotiated.localTxPeerRx.ldpc && negotiated.localRxPeerTx.ldpc;
    negotiated.intersection.stbc = local.stbc && peer.stbc;
    negotiated.intersection.maxAmpduLengthExponent = std::min(local.maxAmpduLengthExponent, peer.maxAmpduLengthExponent);
    negotiated.intersection.maxNss = std::min(local.maxNss, peer.maxNss);
    negotiated.intersection.maxNstsTotal = std::min(
            local.maxNstsTotal == 0 ? local.beamformeeSts : local.maxNstsTotal,
            peer.maxNstsTotal == 0 ? peer.beamformeeSts : peer.maxNstsTotal);
    negotiated.intersection.maxMcs = std::min(local.maxMcs, peer.maxMcs);
    negotiated.valid = negotiated.localTxPeerRx.valid && negotiated.localRxPeerTx.valid;
    return negotiated;
}

} // namespace ieee80211
} // namespace inet

#endif
