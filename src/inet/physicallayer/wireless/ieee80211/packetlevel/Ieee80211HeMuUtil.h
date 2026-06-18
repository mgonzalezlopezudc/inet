//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEMUUTIL_H
#define __INET_IEEE80211HEMUUTIL_H

#include <algorithm>
#include <cmath>
#include <optional>

#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace physicallayer {

enum Ieee80211HePpduFormat {
    HE_MU_DOWNLINK = 0,
    HE_TRIGGER_BASED_UPLINK = 1
};

enum Ieee80211HeGuardInterval {
    HE_GI_0_8_US = 0,
    HE_GI_1_6_US = 1,
    HE_GI_3_2_US = 2
};

enum Ieee80211HeCoding {
    HE_CODING_BCC = 0,
    HE_CODING_LDPC = 1
};

struct Ieee80211HeUserPhyParameters
{
    Ieee80211HeRu ru;
    int mcs = 0;
    int numberOfSpatialStreams = 1;
    bool dcm = false;
    Ieee80211HeGuardInterval guardInterval = HE_GI_3_2_US;
    Ieee80211HeCoding coding = HE_CODING_BCC;
    B psduLength = B(0);
    int codedBitsPerSymbol = 0;
    int dataBitsPerSymbol = 0;
    int numberOfSymbols = 0;
    simtime_t preambleDuration = SimTime(40, SIMTIME_US);
    simtime_t headerDuration = SimTime(8, SIMTIME_US);
    simtime_t dataDuration = SIMTIME_ZERO;
    simtime_t duration = SIMTIME_ZERO;
};

inline int getHeMcsBitsPerSubcarrier(int mcs)
{
    static const int values[] = {1, 2, 2, 4, 4, 6, 6, 6, 8, 8, 10, 10};
    if (mcs < 0 || mcs > 11)
        throw cRuntimeError("Invalid HE MCS: %d", mcs);
    return values[mcs];
}

inline std::pair<int, int> getHeMcsCodeRate(int mcs)
{
    static const std::pair<int, int> values[] = {
        {1, 2}, {1, 2}, {3, 4}, {1, 2}, {3, 4}, {2, 3},
        {3, 4}, {5, 6}, {3, 4}, {5, 6}, {3, 4}, {5, 6}
    };
    if (mcs < 0 || mcs > 11)
        throw cRuntimeError("Invalid HE MCS: %d", mcs);
    return values[mcs];
}

inline simtime_t getHeGuardIntervalDuration(Ieee80211HeGuardInterval guardInterval)
{
    switch (guardInterval) {
        case HE_GI_0_8_US: return SimTime(800, SIMTIME_NS);
        case HE_GI_1_6_US: return SimTime(1600, SIMTIME_NS);
        case HE_GI_3_2_US: return SimTime(3200, SIMTIME_NS);
        default: throw cRuntimeError("Invalid HE guard interval: %d", (int)guardInterval);
    }
}

inline bool isHeDcmCombinationSupported(int mcs, int numberOfSpatialStreams)
{
    return (mcs == 0 || mcs == 1 || mcs == 3 || mcs == 4) &&
           numberOfSpatialStreams >= 1 && numberOfSpatialStreams <= 2;
}

inline Ieee80211HeUserPhyParameters computeHeUserPhyParameters(
        B psduLength, const Ieee80211HeRu& ru, int mcs,
        int numberOfSpatialStreams = 1, bool dcm = false,
        Ieee80211HeGuardInterval guardInterval = HE_GI_3_2_US,
        Ieee80211HeCoding coding = HE_CODING_BCC)
{
    if (ru.toneSize <= 0)
        throw cRuntimeError("Invalid HE RU tone size: %d", ru.toneSize);
    if (numberOfSpatialStreams < 1 || numberOfSpatialStreams > 8)
        throw cRuntimeError("Invalid HE number of spatial streams: %d", numberOfSpatialStreams);
    if (coding != HE_CODING_BCC)
        throw cRuntimeError("HE LDPC is not implemented");
    if (dcm && !isHeDcmCombinationSupported(mcs, numberOfSpatialStreams))
        throw cRuntimeError("Unsupported HE DCM combination: MCS %d, NSS %d", mcs, numberOfSpatialStreams);

    Ieee80211HeUserPhyParameters result;
    result.ru = ru;
    result.mcs = mcs;
    result.numberOfSpatialStreams = numberOfSpatialStreams;
    result.dcm = dcm;
    result.guardInterval = guardInterval;
    result.coding = coding;
    result.psduLength = psduLength;

    int dataSubcarriers = ru.dataSubcarriers > 0 ? ru.dataSubcarriers :
            getHeRuDataSubcarrierCount(ru.toneSize);
    auto codeRate = getHeMcsCodeRate(mcs);
    result.codedBitsPerSymbol = dataSubcarriers * getHeMcsBitsPerSubcarrier(mcs) *
            numberOfSpatialStreams;
    result.dataBitsPerSymbol = result.codedBitsPerSymbol * codeRate.first / codeRate.second;
    if (dcm)
        result.dataBitsPerSymbol /= 2;
    if (result.dataBitsPerSymbol <= 0)
        throw cRuntimeError("HE user has no data bits per symbol");

    constexpr int serviceBits = 16;
    constexpr int bccTailBits = 6;
    int64_t completeBits = serviceBits + psduLength.get<B>() * 8 + bccTailBits;
    result.numberOfSymbols = (completeBits + result.dataBitsPerSymbol - 1) /
            result.dataBitsPerSymbol;
    auto symbolDuration = SimTime(12800, SIMTIME_NS) +
            getHeGuardIntervalDuration(guardInterval);
    result.dataDuration = result.numberOfSymbols * symbolDuration;
    result.duration = result.preambleDuration + result.headerDuration + result.dataDuration;
    return result;
}

inline simtime_t estimateHeMuUserDuration(B psduLength, int toneSize, int mcs,
        int numberOfSpatialStreams = 1, bool dcm = false,
        Ieee80211HeGuardInterval guardInterval = HE_GI_3_2_US)
{
    Ieee80211HeRu ru;
    ru.toneSize = std::max(toneSize, 26);
    ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
    ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
    ru.bandwidth = Hz(ru.toneSize * 78125.0);
    return computeHeUserPhyParameters(psduLength, ru, mcs,
            numberOfSpatialStreams, dcm, guardInterval).duration;
}

constexpr uint16_t HE_STA_ID_BROADCAST = 2047;

inline uint16_t computeHeMuStaId(const MacAddress& address)
{
    // TODO replace this fallback with the association ID when it is available
    // at the packet-level PHY boundary.
    return address.isBroadcast() ? HE_STA_ID_BROADCAST : static_cast<uint16_t>(address.getInt() & 0x7ff);
}

inline std::optional<uint16_t> tryResolveHeMuStaId(const cModule *networkInterface, const MacAddress& address)
{
    if (address.isBroadcast())
        return HE_STA_ID_BROADCAST;
    if (networkInterface != nullptr) {
        auto mib = dynamic_cast<const ieee80211::Ieee80211Mib *>(networkInterface->getSubmodule("mib"));
        if (mib != nullptr) {
            if (mib->bssStationData.stationType == ieee80211::Ieee80211Mib::STATION &&
                    mib->bssStationData.associationId > 0)
                return mib->bssStationData.associationId;
            auto aid = mib->getAssociationId(address);
            if (aid > 0)
                return aid;
        }
    }
    return std::nullopt;
}

inline std::optional<uint16_t> resolveHeMuStaIdForReception(
        const cModule *networkInterface, const MacAddress& address)
{
    if (networkInterface != nullptr && networkInterface->getSubmodule("mib") != nullptr)
        return tryResolveHeMuStaId(networkInterface, address);
    return computeHeMuStaId(address); // compatibility for PHY-only test NICs
}

} // namespace physicallayer
} // namespace inet

#endif
