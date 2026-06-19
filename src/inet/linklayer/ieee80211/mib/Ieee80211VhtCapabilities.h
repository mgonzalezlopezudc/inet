//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211VHTCAPABILITIES_H
#define __INET_IEEE80211VHTCAPABILITIES_H

#include <algorithm>
#include <set>
#include "inet/common/Units.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

struct Ieee80211VhtCapabilities
{
    bool ldpc = false;
    bool stbc = false;
    bool txBeamforming = false;
    bool muMimo = false;
    int maxAmpduLengthExponent = 7; // up to 7 for VHT (1,048,575 bytes)
    int maxNss = 8;
    int maxMcs = 9;
};

struct Ieee80211VhtOperation
{
    Hz operatingChannelWidth = Hz(20e6);
    int numSpatialStreams = 1;
    bool shortGi = false;
    bool ldpc = false;
};

struct Ieee80211NegotiatedVhtCapabilities
{
    Ieee80211VhtCapabilities intersection;
    Ieee80211VhtOperation operation;
    bool valid = false;
};

inline Ieee80211NegotiatedVhtCapabilities negotiateVhtCapabilities(
        const Ieee80211VhtCapabilities& local,
        const Ieee80211VhtCapabilities& peer,
        const Ieee80211VhtOperation& operation)
{
    Ieee80211NegotiatedVhtCapabilities negotiated;
    negotiated.operation = operation;
    negotiated.intersection.ldpc = local.ldpc && peer.ldpc;
    negotiated.intersection.stbc = local.stbc && peer.stbc;
    negotiated.intersection.txBeamforming = local.txBeamforming && peer.txBeamforming;
    negotiated.intersection.muMimo = local.muMimo && peer.muMimo;
    negotiated.intersection.maxAmpduLengthExponent = std::min(local.maxAmpduLengthExponent, peer.maxAmpduLengthExponent);
    negotiated.intersection.maxNss = std::min(local.maxNss, peer.maxNss);
    negotiated.intersection.maxMcs = std::min(local.maxMcs, peer.maxMcs);
    negotiated.valid = true;
    return negotiated;
}

} // namespace ieee80211
} // namespace inet

#endif
