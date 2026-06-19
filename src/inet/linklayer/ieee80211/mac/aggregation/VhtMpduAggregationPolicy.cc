//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/aggregation/VhtMpduAggregationPolicy.h"

namespace inet {
namespace ieee80211 {

Define_Module(VhtMpduAggregationPolicy);

void VhtMpduAggregationPolicy::initialize()
{
    // SimpleModule initialize: check parameters
    if (hasPar("maxAmpduLengthExponent")) {
        maxAmpduLengthExponent = par("maxAmpduLengthExponent");
    }
}

std::vector<Packet *> *VhtMpduAggregationPolicy::computeAggregateFrames(std::vector<Packet *> *frames)
{
    Enter_Method("computeAggregateFrames");
    if (!frames || frames->empty())
        return nullptr;

    // Calculate maximum A-MPDU size in bytes: 2^(13 + E) - 1
    long long maxAMpduSize = (1LL << (13 + maxAmpduLengthExponent)) - 1;

    auto aggregated = new std::vector<Packet *>();
    long long currentSize = 0;

    for (auto frame : *frames) {
        long long frameLength = frame->getDataLength().get<B>();
        long long neededSize = frameLength + 4; // 4-byte delimiter overhead

        if (!aggregated->empty()) {
            neededSize += (4 - (neededSize % 4)) % 4; // 4-byte padding
        }

        if (currentSize + neededSize > maxAMpduSize) {
            break;
        }

        aggregated->push_back(frame);
        currentSize += neededSize;
    }

    if (aggregated->size() <= 1) {
        delete aggregated;
        return nullptr;
    }

    return aggregated;
}

} // namespace ieee80211
} // namespace inet
