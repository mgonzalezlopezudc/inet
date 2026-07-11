//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEMGMTELEMENTS_H
#define __INET_IEEE80211HEMGMTELEMENTS_H

#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h"

namespace inet {
namespace ieee80211 {

inline bool containsChannelWidth(const Ieee80211HeCapabilities& capabilities, int widthMHz)
{
    return capabilities.supportedChannelWidths.count(Hz(widthMHz * 1e6)) != 0;
}

inline void copyMcsNssMapToElement(const Ieee80211HeMcsNssMap& source, Ieee80211HeMcsNssMapElement& destination)
{
    for (size_t i = 0; i < source.maxMcsPerNss.size(); ++i)
        destination.maxMcsForNss[i] = source.maxMcsPerNss[i];
}

inline void copyMcsNssMapFromElement(const Ieee80211HeMcsNssMapElement& source, Ieee80211HeMcsNssMap& destination)
{
    for (size_t i = 0; i < destination.maxMcsPerNss.size(); ++i)
        destination.maxMcsPerNss[i] = source.maxMcsForNss[i];
}

/** Converts the scheduler/MIB HE capability representation to a management-frame element. */
inline Ieee80211HeCapabilitiesElement makeHeCapabilitiesElement(const Ieee80211HeCapabilities& capabilities)
{
    Ieee80211HeCapabilitiesElement element;
    element.twtRequester = capabilities.twtRequester;
    element.twtResponder = capabilities.twtResponder;
    element.broadcastTwt = capabilities.broadcastTwt;
    element.dynamicFragmentationLevel = capabilities.dynamicFragmentationLevel;
    element.omControl = capabilities.omControl;
    element.twoNav = capabilities.twoNav;
    element.erBss = capabilities.erBss;
    element.ndpFeedbackReport = capabilities.ndpFeedbackReport;
    element.supportedChannelWidth40MHz = containsChannelWidth(capabilities, 40);
    element.supportedChannelWidth80MHz = containsChannelWidth(capabilities, 80);
    element.supportedChannelWidth160MHz = containsChannelWidth(capabilities, 160);
    element.supportedChannelWidth80Plus80MHz = false;
    copyMcsNssMapToElement(capabilities.rxMcsNss, element.rxMcsNss);
    copyMcsNssMapToElement(capabilities.txMcsNss, element.txMcsNss);
    element.dlOfdma = capabilities.dlOfdma;
    element.ulOfdma = capabilities.ulOfdma;
    element.dcm = capabilities.dcm;
    element.maxDcmConstellation = capabilities.maxDcmConstellation;
    element.maxDcmNss = capabilities.maxDcmNss;
    element.ldpc = capabilities.ldpc;
    element.preamblePuncturing = capabilities.preamblePuncturing;
    element.multiTidAggregationRx = capabilities.multiTidAggregationRx;
    element.multiTidAggregationTx = capabilities.multiTidAggregationTx;
    element.muBarTriggerRx = capabilities.muBarTriggerRx;
    element.heTbBlockAckTx = capabilities.heTbBlockAckTx;
    element.maxAmpduLengthExponent = capabilities.maxAmpduLengthExponent;
    element.maxMpduLength = capabilities.maxMpduLength;
    element.maxBlockAckBufferSize = capabilities.maxBlockAckBufferSize;
    element.ru26Tone = capabilities.supportedRuToneSizes.count(26) != 0;
    element.ru52Tone = capabilities.supportedRuToneSizes.count(52) != 0;
    element.ru106Tone = capabilities.supportedRuToneSizes.count(106) != 0;
    element.ru242Tone = capabilities.supportedRuToneSizes.count(242) != 0;
    element.ru484Tone = capabilities.supportedRuToneSizes.count(484) != 0;
    element.ru996Tone = capabilities.supportedRuToneSizes.count(996) != 0;
    element.ru1992Tone = capabilities.supportedRuToneSizes.count(1992) != 0;
    element.dlMuMimoBeamformer = capabilities.dlMuMimoBeamformer;
    element.dlMuMimoBeamformee = capabilities.dlMuMimoBeamformee;
    element.fullBandwidthUlMuMimo = capabilities.fullBandwidthUlMuMimo;
    element.partialBandwidthUlMuMimo = capabilities.partialBandwidthUlMuMimo;
    element.soundingDimensions = capabilities.soundingDimensions;
    element.beamformeeSts20Mhz = capabilities.beamformeeSts20Mhz;
    element.beamformeeStsAbove20Mhz = capabilities.beamformeeStsAbove20Mhz;
    element.feedbackMode = capabilities.feedbackMode;
    return element;
}

/** Converts a received HE capabilities element to the representation used by negotiation. */
inline Ieee80211HeCapabilities makeHeCapabilities(const Ieee80211HeCapabilitiesElement& element)
{
    Ieee80211HeCapabilities capabilities;
    capabilities.twtRequester = element.twtRequester;
    capabilities.twtResponder = element.twtResponder;
    capabilities.broadcastTwt = element.broadcastTwt;
    capabilities.dynamicFragmentationLevel = element.dynamicFragmentationLevel;
    capabilities.omControl = element.omControl;
    capabilities.twoNav = element.twoNav;
    capabilities.erBss = element.erBss;
    capabilities.ndpFeedbackReport = element.ndpFeedbackReport;
    capabilities.supportedChannelWidths.clear();
    capabilities.supportedChannelWidths.insert(Hz(20e6));
    if (element.supportedChannelWidth40MHz)
        capabilities.supportedChannelWidths.insert(Hz(40e6));
    if (element.supportedChannelWidth80MHz)
        capabilities.supportedChannelWidths.insert(Hz(80e6));
    if (element.supportedChannelWidth160MHz)
        capabilities.supportedChannelWidths.insert(Hz(160e6));
    copyMcsNssMapFromElement(element.rxMcsNss, capabilities.rxMcsNss);
    copyMcsNssMapFromElement(element.txMcsNss, capabilities.txMcsNss);
    capabilities.dlOfdma = element.dlOfdma;
    capabilities.ulOfdma = element.ulOfdma;
    capabilities.dcm = element.dcm;
    capabilities.maxDcmConstellation = element.maxDcmConstellation;
    capabilities.maxDcmNss = element.maxDcmNss;
    capabilities.ldpc = element.ldpc;
    capabilities.preamblePuncturing = element.preamblePuncturing;
    capabilities.multiTidAggregationRx = element.multiTidAggregationRx;
    capabilities.multiTidAggregationTx = element.multiTidAggregationTx;
    capabilities.muBarTriggerRx = element.muBarTriggerRx;
    capabilities.heTbBlockAckTx = element.heTbBlockAckTx;
    capabilities.maxAmpduLengthExponent = element.maxAmpduLengthExponent;
    capabilities.maxMpduLength = element.maxMpduLength;
    capabilities.maxBlockAckBufferSize = element.maxBlockAckBufferSize;
    capabilities.supportedRuToneSizes.clear();
    if (element.ru26Tone)
        capabilities.supportedRuToneSizes.insert(26);
    if (element.ru52Tone)
        capabilities.supportedRuToneSizes.insert(52);
    if (element.ru106Tone)
        capabilities.supportedRuToneSizes.insert(106);
    if (element.ru242Tone)
        capabilities.supportedRuToneSizes.insert(242);
    if (element.ru484Tone)
        capabilities.supportedRuToneSizes.insert(484);
    if (element.ru996Tone)
        capabilities.supportedRuToneSizes.insert(996);
    if (element.ru1992Tone)
        capabilities.supportedRuToneSizes.insert(1992);
    capabilities.dlMuMimoBeamformer = element.dlMuMimoBeamformer;
    capabilities.dlMuMimoBeamformee = element.dlMuMimoBeamformee;
    capabilities.fullBandwidthUlMuMimo = element.fullBandwidthUlMuMimo;
    capabilities.partialBandwidthUlMuMimo = element.partialBandwidthUlMuMimo;
    capabilities.soundingDimensions = element.soundingDimensions;
    capabilities.beamformeeSts20Mhz = element.beamformeeSts20Mhz;
    capabilities.beamformeeStsAbove20Mhz = element.beamformeeStsAbove20Mhz;
    capabilities.feedbackMode = element.feedbackMode;
    return capabilities;
}

/** Converts negotiated HE operating parameters to their management-frame element. */
inline Ieee80211HeOperationElement makeHeOperationElement(const Ieee80211HeOperation& operation)
{
    Ieee80211HeOperationElement element;
    element.bssColor = operation.bssColor;
    element.operatingChannelWidthMHz = (int)(operation.operatingChannelWidth.get() / 1e6);
    element.basicHeMcsNss = operation.basicHeMcsNss;
    element.defaultPeDurationPresent = operation.defaultPeDurationPresent;
    element.defaultPeDurationUs = operation.defaultPeDurationUs;
    return element;
}

/** Converts a received HE operation element to the MIB operation representation. */
inline Ieee80211HeOperation makeHeOperation(const Ieee80211HeOperationElement& element)
{
    Ieee80211HeOperation operation;
    operation.bssColor = element.bssColor;
    operation.operatingChannelWidth = Hz(element.operatingChannelWidthMHz * 1e6);
    operation.basicHeMcsNss = element.basicHeMcsNss;
    operation.defaultPeDurationPresent = element.defaultPeDurationPresent;
    operation.defaultPeDurationUs = element.defaultPeDurationUs;
    return operation;
}

inline Ieee80211He6GhzBandCapabilitiesElement makeHe6GhzBandCapabilitiesElement(const Ieee80211HeCapabilities& capabilities)
{
    Ieee80211He6GhzBandCapabilitiesElement element;
    element.minimumMpduStartSpacing = 0;
    element.maxAmpduLengthExponent = capabilities.maxAmpduLengthExponent;
    element.maxMpduLength = capabilities.maxMpduLength;
    return element;
}

inline B getHeCapabilitiesElementLength(const Ieee80211HeCapabilitiesElement& element)
{
    return B(1 + 1 + 1 + 6 + 11 + (element.supportedChannelWidth160MHz || element.supportedChannelWidth80Plus80MHz ? 8 : 4));
}

inline B getHeOperationElementLength()
{
    return B(1 + 1 + 1 + 3 + 1 + 2);
}

inline B getHe6GhzBandCapabilitiesElementLength()
{
    return B(1 + 1 + 1 + 2);
}

inline B getBroadcastTwtElementLength(const Ptr<const Ieee80211MgmtFrame>& frame)
{
    return frame->getBroadcastTwtPresent() ? B(3 + 15 * frame->getBroadcastTwtSchedulesArraySize()) : B(0);
}

/** Returns the encoded length of HE management elements present in a management frame. */
inline B getHeMgmtElementsLength(const Ptr<const Ieee80211MgmtFrame>& frame)
{
    B length = B(0);
    if (frame->getHeCapabilitiesPresent())
        length += getHeCapabilitiesElementLength(frame->getHeCapabilities());
    if (frame->getHeOperationPresent())
        length += getHeOperationElementLength();
    if (frame->getHe6GhzBandCapabilitiesPresent())
        length += getHe6GhzBandCapabilitiesElementLength();
    length += getBroadcastTwtElementLength(frame);
    return length;
}

} // namespace ieee80211
} // namespace inet

#endif
