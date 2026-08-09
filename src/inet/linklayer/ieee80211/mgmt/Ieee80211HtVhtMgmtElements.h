//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HTVHTMGMTELEMENTS_H
#define __INET_IEEE80211HTVHTMGMTELEMENTS_H

#include <cstdlib>

#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HtCapabilities.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211VhtCapabilities.h"

namespace inet { namespace ieee80211 {

inline Ieee80211HtCapabilitiesElement makeHtCapabilitiesElement(const Ieee80211HtCapabilities& capabilities)
{
    Ieee80211HtCapabilitiesElement element;
    element.ldpc = capabilities.ldpc;
    element.supportedChannelWidth40MHz = capabilities.supportedChannelWidths.count(MHz(40));
    element.shortGi20 = capabilities.shortGi20;
    element.shortGi40 = capabilities.shortGi40;
    element.maxAmpduLengthExponent = capabilities.maxAmpduLengthExponent;
    for (int i = 0; i < 4; i++) {
        element.rxMaxMcsForNss[i] = capabilities.rxMcsNss.maxMcsPerNss[i];
        element.txMaxMcsForNss[i] = capabilities.txMcsNss.maxMcsPerNss[i];
    }
    return element;
}

inline Ieee80211HtCapabilities makeHtCapabilities(const Ieee80211HtCapabilitiesElement& element)
{
    Ieee80211HtCapabilities capabilities;
    capabilities.supportedChannelWidths = {MHz(20)};
    if (element.supportedChannelWidth40MHz)
        capabilities.supportedChannelWidths.insert(MHz(40));
    capabilities.ldpc = element.ldpc;
    capabilities.shortGi20 = element.shortGi20;
    capabilities.shortGi40 = element.shortGi40;
    capabilities.maxAmpduLengthExponent = element.maxAmpduLengthExponent;
    for (int i = 0; i < 4; i++) {
        capabilities.rxMcsNss.maxMcsPerNss[i] = element.rxMaxMcsForNss[i];
        capabilities.txMcsNss.maxMcsPerNss[i] = element.txMaxMcsForNss[i];
    }
    return capabilities;
}

inline Ieee80211HtOperationElement makeHtOperationElement(const Ieee80211HtOperation& operation)
{
    Ieee80211HtOperationElement element;
    element.primaryChannel = operation.primaryChannel;
    element.secondaryChannelOffset = operation.secondaryChannelOffset;
    element.staChannelWidth40MHz = operation.operatingChannelWidth == MHz(40);
    element.protectionMode = static_cast<int>(operation.protectionMode);
    for (int i = 0; i < 4; i++) element.basicMaxMcsForNss[i] = operation.basicMcsNss.maxMcsPerNss[i];
    return element;
}

inline Ieee80211HtOperation makeHtOperation(const Ieee80211HtOperationElement& element)
{
    Ieee80211HtOperation operation;
    operation.primaryChannel = element.primaryChannel;
    operation.secondaryChannelOffset = element.secondaryChannelOffset;
    operation.operatingChannelWidth = element.staChannelWidth40MHz ? MHz(40) : MHz(20);
    if (element.protectionMode < 0 || element.protectionMode > 3)
        throw cRuntimeError("Invalid HT Protection field value: %d", element.protectionMode);
    operation.protectionMode = static_cast<Ieee80211HtProtectionMode>(element.protectionMode);
    for (int i = 0; i < 4; i++) operation.basicMcsNss.maxMcsPerNss[i] = element.basicMaxMcsForNss[i];
    return operation;
}

inline Ieee80211VhtCapabilitiesElement makeVhtCapabilitiesElement(const Ieee80211VhtCapabilities& capabilities)
{
    Ieee80211VhtCapabilitiesElement element;
    element.rxLdpc = capabilities.rxLdpc;
    element.supportedChannelWidthSet = capabilities.supports80Plus80MHz ? 2 :
            capabilities.supportedChannelWidths.count(MHz(160)) ? 1 : 0;
    element.shortGi80 = capabilities.shortGi80;
    element.shortGi160 = capabilities.shortGi160;
    element.maxAmpduLengthExponent = capabilities.maxAmpduLengthExponent;
    element.suBeamformer = capabilities.suBeamformer;
    element.suBeamformee = capabilities.suBeamformee;
    element.beamformeeSts = capabilities.beamformeeSts;
    element.soundingDimensions = capabilities.soundingDimensions;
    element.muBeamformer = capabilities.muBeamformer;
    element.muBeamformee = capabilities.muBeamformee;
    element.rxHighestLongGiDataRateMbps = capabilities.rxHighestLongGiDataRateMbps;
    element.maxNstsTotal = capabilities.maxNstsTotal;
    element.txHighestLongGiDataRateMbps = capabilities.txHighestLongGiDataRateMbps;
    element.extendedNssBwCapable = false;
    for (int i = 0; i < 8; i++) {
        element.rxMaxMcsForNss[i] = capabilities.rxMcsNss.maxMcsPerNss[i];
        element.txMaxMcsForNss[i] = capabilities.txMcsNss.maxMcsPerNss[i];
    }
    return element;
}

inline Ieee80211VhtCapabilities makeVhtCapabilities(const Ieee80211VhtCapabilitiesElement& element)
{
    Ieee80211VhtCapabilities capabilities;
    capabilities.supportedChannelWidths = {MHz(20), MHz(40), MHz(80)};
    if (element.supportedChannelWidthSet == 1 || element.supportedChannelWidthSet == 2)
        capabilities.supportedChannelWidths.insert(MHz(160));
    capabilities.supports80Plus80MHz = element.supportedChannelWidthSet == 2;
    capabilities.rxLdpc = element.rxLdpc;
    capabilities.ldpc = element.rxLdpc;
    capabilities.shortGi80 = element.shortGi80;
    capabilities.shortGi160 = element.shortGi160;
    capabilities.maxAmpduLengthExponent = element.maxAmpduLengthExponent;
    capabilities.suBeamformer = element.suBeamformer;
    capabilities.suBeamformee = element.suBeamformee;
    capabilities.beamformeeSts = element.beamformeeSts;
    capabilities.soundingDimensions = element.soundingDimensions;
    capabilities.muBeamformer = element.muBeamformer;
    capabilities.muBeamformee = element.muBeamformee;
    capabilities.rxHighestLongGiDataRateMbps = element.rxHighestLongGiDataRateMbps;
    capabilities.maxNstsTotal = element.maxNstsTotal;
    capabilities.txHighestLongGiDataRateMbps = element.txHighestLongGiDataRateMbps;
    for (int i = 0; i < 8; i++) {
        capabilities.rxMcsNss.maxMcsPerNss[i] = element.rxMaxMcsForNss[i];
        capabilities.txMcsNss.maxMcsPerNss[i] = element.txMaxMcsForNss[i];
    }
    return capabilities;
}

inline Ieee80211VhtOperationElement makeVhtOperationElement(const Ieee80211VhtOperation& operation)
{
    Ieee80211VhtOperationElement element;
    element.channelWidth = operation.operatingChannelWidth >= MHz(80) ? 1 : 0;
    element.centerFrequencySegment0 = operation.centerFrequencySegment0;
    element.centerFrequencySegment1 = operation.centerFrequencySegment1;
    for (int i = 0; i < 8; i++)
        element.basicMaxMcsForNss[i] = operation.basicMcsNss.maxMcsPerNss[i];
    return element;
}

inline Ieee80211VhtOperation makeVhtOperation(const Ieee80211VhtOperationElement& element,
        const Ieee80211HtOperation& htOperation)
{
    Ieee80211VhtOperation operation;
    // IEEE Std 802.11-2024 Tables 9-316/9-317: with Channel Width=1,
    // a nonzero CCFS1 separated by 8 channel numbers identifies 160 MHz.
    if (element.channelWidth == 0)
        operation.operatingChannelWidth = htOperation.operatingChannelWidth;
    else if (element.channelWidth == 1 && element.centerFrequencySegment1 != 0 &&
            std::abs(element.centerFrequencySegment1 - element.centerFrequencySegment0) == 8)
        operation.operatingChannelWidth = MHz(160);
    else if (element.channelWidth == 1 && element.centerFrequencySegment1 == 0)
        operation.operatingChannelWidth = MHz(80);
    else if (element.channelWidth == 1 && element.centerFrequencySegment1 != 0) {
        auto separation = std::abs(element.centerFrequencySegment1 - element.centerFrequencySegment0);
        if (separation <= 16)
            throw cRuntimeError("Malformed VHT Operation 80+80 CCFS separation: %d", separation);
        operation.operatingChannelWidth = MHz(160);
        operation.nonContiguous = true;
    }
    else
        throw cRuntimeError("Unsupported or malformed VHT Operation channel-width/CCFS combination");
    operation.centerFrequencySegment0 = element.centerFrequencySegment0;
    operation.centerFrequencySegment1 = element.centerFrequencySegment1;
    operation.numSpatialStreams = 0;
    for (int i = 0; i < 8; i++) {
        operation.basicMcsNss.maxMcsPerNss[i] = element.basicMaxMcsForNss[i];
        if (element.basicMaxMcsForNss[i] >= 0)
            operation.numSpatialStreams = i + 1;
    }
    return operation;
}

inline B getHtVhtMgmtElementsLength(const Ptr<const Ieee80211MgmtFrame>& frame)
{
    B length(0);
    if (frame->getHtCapabilitiesPresent()) length += B(28);
    if (frame->getHtOperationPresent()) length += B(24);
    if (frame->getVhtCapabilitiesPresent()) length += B(14);
    if (frame->getVhtOperationPresent()) length += B(7);
    return length;
}

} }

#endif
