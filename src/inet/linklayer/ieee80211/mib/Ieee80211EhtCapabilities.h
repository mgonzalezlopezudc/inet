//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211EHTCAPABILITIES_H
#define __INET_IEEE80211EHTCAPABILITIES_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <ostream>
#include <set>
#include <tuple>

#include "inet/common/Units.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

/** Per-spatial-stream maximum EHT MCS map; -1 denotes an unsupported stream. */
struct Ieee80211EhtMcsNssMap
{
    std::array<int, 8> maxMcsPerNss;

    Ieee80211EhtMcsNssMap()
    {
        maxMcsPerNss.fill(-1);
        maxMcsPerNss[0] = 13;
    }
};

/**
 * EHT capability set used by management negotiation and future MLO scheduling.
 *
 * This models the packet-level features that have a direct INET behavior:
 * channel widths, MCS/NSS maps, OFDMA, 4096-QAM, puncturing, coding, and the
 * basic MLD/EML/STR flags. It intentionally omits fields that currently have
 * no packet-level consumer.
 *
 * Source anchors:
 *  - 80211be-2024:chunk:00628 and :00629 define the EHT Capabilities element.
 *  - 80211be-2024:chunk:00653..00656 define the Supported EHT-MCS And NSS Set.
 */
struct Ieee80211EhtCapabilities
{
    std::set<Hz> supportedChannelWidths = {Hz(20e6), Hz(40e6), Hz(80e6), Hz(160e6), Hz(320e6)};
    Ieee80211EhtMcsNssMap rxMcsNss;
    Ieee80211EhtMcsNssMap txMcsNss;
    bool dlOfdma = true;
    bool ulOfdma = true;
    bool dlMuMimo = false;
    bool ulMuMimo = false;
    bool ldpc = true;
    bool support4096Qam = true;
    bool ehtDup6GHz = false;
    bool preamblePuncturing = true;
    bool mlo = false;
    bool str = true;
    bool nstr = false;
    bool emlsr = false;
    bool emlmr = false;
    int maxAmpduLengthExponent = 7;
    int maxMpduLength = 11454;
    int maxBlockAckBufferSize = 256;
};

/** EHT operating parameters advertised by an AP after capability negotiation. */
struct Ieee80211EhtOperation
{
    Hz operatingChannelWidth = Hz(20e6);
    uint16_t disabledSubchannelBitmap = 0;
    int basicEhtMcsNss = 0;
    bool mcs15Disabled = false;
};

/** Usable EHT feature set and operation produced for a local/peer association. */
struct Ieee80211NegotiatedEhtCapabilities
{
    Ieee80211EhtCapabilities intersection;
    Ieee80211EhtOperation operation;
    bool valid = false;
};

// Semantic state comparisons deliberately enumerate every modeled field. When
// adding a field to one of these value types, add it to the corresponding
// comparison so capability generations remain complete and deterministic.
inline bool operator==(const Ieee80211EhtMcsNssMap& a, const Ieee80211EhtMcsNssMap& b)
{
    return a.maxMcsPerNss == b.maxMcsPerNss;
}

inline bool operator==(const Ieee80211EhtCapabilities& a, const Ieee80211EhtCapabilities& b)
{
    return std::tie(a.supportedChannelWidths, a.rxMcsNss, a.txMcsNss, a.dlOfdma, a.ulOfdma,
                   a.dlMuMimo, a.ulMuMimo, a.ldpc, a.support4096Qam, a.ehtDup6GHz,
                   a.preamblePuncturing, a.mlo, a.str, a.nstr, a.emlsr, a.emlmr,
                   a.maxAmpduLengthExponent, a.maxMpduLength, a.maxBlockAckBufferSize) ==
           std::tie(b.supportedChannelWidths, b.rxMcsNss, b.txMcsNss, b.dlOfdma, b.ulOfdma,
                   b.dlMuMimo, b.ulMuMimo, b.ldpc, b.support4096Qam, b.ehtDup6GHz,
                   b.preamblePuncturing, b.mlo, b.str, b.nstr, b.emlsr, b.emlmr,
                   b.maxAmpduLengthExponent, b.maxMpduLength, b.maxBlockAckBufferSize);
}

inline bool operator==(const Ieee80211EhtOperation& a, const Ieee80211EhtOperation& b)
{
    return std::tie(a.operatingChannelWidth, a.disabledSubchannelBitmap,
                   a.basicEhtMcsNss, a.mcs15Disabled) ==
           std::tie(b.operatingChannelWidth, b.disabledSubchannelBitmap,
                   b.basicEhtMcsNss, b.mcs15Disabled);
}

inline bool operator==(const Ieee80211NegotiatedEhtCapabilities& a, const Ieee80211NegotiatedEhtCapabilities& b)
{
    return std::tie(a.intersection, a.operation, a.valid) ==
           std::tie(b.intersection, b.operation, b.valid);
}

inline int getMaxNss(const Ieee80211EhtMcsNssMap& map)
{
    int maxNss = 0;
    for (int i = 0; i < 8; ++i)
        if (map.maxMcsPerNss[i] >= 0)
            maxNss = i + 1;
    return maxNss;
}

inline bool supportsEhtMcs13(const Ieee80211EhtMcsNssMap& map)
{
    for (auto maxMcs : map.maxMcsPerNss)
        if (maxMcs >= 13)
            return true;
    return false;
}

/**
 * Computes directional MCS/NSS maps and mutual EHT capabilities.
 *
 * The result is valid only when the requested operating width is supported,
 * downlink OFDMA is mutually supported, and at least one transmit stream can
 * be used. MLO validity is independent so single-link EHT remains valid.
 */
inline Ieee80211NegotiatedEhtCapabilities negotiateEhtCapabilities(
        const Ieee80211EhtCapabilities& local,
        const Ieee80211EhtCapabilities& peer,
        const Ieee80211EhtOperation& operation)
{
    Ieee80211NegotiatedEhtCapabilities negotiated;
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
    negotiated.intersection.dlMuMimo = local.dlMuMimo && peer.dlMuMimo;
    negotiated.intersection.ulMuMimo = local.ulMuMimo && peer.ulMuMimo;
    negotiated.intersection.ldpc = local.ldpc && peer.ldpc;
    negotiated.intersection.support4096Qam =
            local.support4096Qam && peer.support4096Qam &&
            supportsEhtMcs13(negotiated.intersection.txMcsNss);
    negotiated.intersection.ehtDup6GHz = local.ehtDup6GHz && peer.ehtDup6GHz;
    negotiated.intersection.preamblePuncturing =
            local.preamblePuncturing && peer.preamblePuncturing;
    negotiated.intersection.mlo = local.mlo && peer.mlo;
    negotiated.intersection.str = negotiated.intersection.mlo && local.str && peer.str;
    negotiated.intersection.nstr = negotiated.intersection.mlo && local.nstr && peer.nstr;
    negotiated.intersection.emlsr = negotiated.intersection.mlo && local.emlsr && peer.emlsr;
    negotiated.intersection.emlmr = negotiated.intersection.mlo && local.emlmr && peer.emlmr;
    negotiated.intersection.maxAmpduLengthExponent =
            std::min(local.maxAmpduLengthExponent, peer.maxAmpduLengthExponent);
    negotiated.intersection.maxMpduLength = std::min(local.maxMpduLength, peer.maxMpduLength);
    negotiated.intersection.maxBlockAckBufferSize =
            std::min(local.maxBlockAckBufferSize, peer.maxBlockAckBufferSize);
    negotiated.valid =
            negotiated.intersection.dlOfdma &&
            negotiated.intersection.supportedChannelWidths.count(operation.operatingChannelWidth) != 0 &&
            negotiated.intersection.txMcsNss.maxMcsPerNss[0] >= 0;
    return negotiated;
}

inline std::ostream& operator<<(std::ostream& os, const Ieee80211EhtCapabilities& capabilities)
{
    os << "ldpc=" << (capabilities.ldpc ? "yes" : "no")
       << " dlOfdma=" << (capabilities.dlOfdma ? "yes" : "no")
       << " ulOfdma=" << (capabilities.ulOfdma ? "yes" : "no")
       << " 4096Qam=" << (capabilities.support4096Qam ? "yes" : "no")
       << " ehtDup6GHz=" << (capabilities.ehtDup6GHz ? "yes" : "no")
       << " puncturing=" << (capabilities.preamblePuncturing ? "yes" : "no")
       << " mlo=" << (capabilities.mlo ? "yes" : "no")
       << " str=" << (capabilities.str ? "yes" : "no")
       << " emlsr=" << (capabilities.emlsr ? "yes" : "no")
       << " emlmr=" << (capabilities.emlmr ? "yes" : "no")
       << " maxTxNss=" << getMaxNss(capabilities.txMcsNss)
       << " maxRxNss=" << getMaxNss(capabilities.rxMcsNss)
       << " widths=" << capabilities.supportedChannelWidths.size();
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const Ieee80211EhtOperation& operation)
{
    os << "width=" << operation.operatingChannelWidth
       << " disabledSubchannels=0x" << std::hex << operation.disabledSubchannelBitmap << std::dec
       << " basicMcsNss=" << operation.basicEhtMcsNss
       << " mcs15=" << (operation.mcs15Disabled ? "disabled" : "enabled");
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const Ieee80211NegotiatedEhtCapabilities& capabilities)
{
    os << "valid=" << (capabilities.valid ? "yes" : "no")
       << " {" << capabilities.intersection << "}"
       << " operation={" << capabilities.operation << "}";
    return os;
}

} // namespace ieee80211
} // namespace inet

#endif
