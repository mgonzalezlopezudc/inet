//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211EHTMGMTELEMENTS_H
#define __INET_IEEE80211EHTMGMTELEMENTS_H

#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211EhtCapabilities.h"

namespace inet {
namespace ieee80211 {

inline bool containsEhtChannelWidth(const Ieee80211EhtCapabilities& capabilities, int widthMHz)
{
    return capabilities.supportedChannelWidths.count(Hz(widthMHz * 1e6)) != 0;
}

inline void copyEhtMcsNssMapToElement(const Ieee80211EhtMcsNssMap& source, Ieee80211EhtMcsNssMapElement& destination)
{
    for (size_t i = 0; i < source.maxMcsPerNss.size(); ++i)
        destination.maxMcsPerNss[i] = source.maxMcsPerNss[i];
}

inline void copyEhtMcsNssMapFromElement(const Ieee80211EhtMcsNssMapElement& source, Ieee80211EhtMcsNssMap& destination)
{
    for (size_t i = 0; i < destination.maxMcsPerNss.size(); ++i)
        destination.maxMcsPerNss[i] = source.maxMcsPerNss[i];
}

/** Converts the scheduler/MIB EHT capability representation to a management-frame element. */
inline Ieee80211EhtCapabilitiesElement makeEhtCapabilitiesElement(const Ieee80211EhtCapabilities& capabilities)
{
    Ieee80211EhtCapabilitiesElement element;
    element.supportedChannelWidth20MHz = containsEhtChannelWidth(capabilities, 20);
    element.supportedChannelWidth40MHz = containsEhtChannelWidth(capabilities, 40);
    element.supportedChannelWidth80MHz = containsEhtChannelWidth(capabilities, 80);
    element.supportedChannelWidth160MHz = containsEhtChannelWidth(capabilities, 160);
    element.supportedChannelWidth320MHz = containsEhtChannelWidth(capabilities, 320);
    
    copyEhtMcsNssMapToElement(capabilities.rxMcsNss, element.rxMcsNss);
    copyEhtMcsNssMapToElement(capabilities.txMcsNss, element.txMcsNss);
    
    element.dlOfdma = capabilities.dlOfdma;
    element.ulOfdma = capabilities.ulOfdma;
    element.dlMuMimo = capabilities.dlMuMimo;
    element.ulMuMimo = capabilities.ulMuMimo;
    element.ldpc = capabilities.ldpc;
    element.support4096Qam = capabilities.support4096Qam;
    element.preamblePuncturing = capabilities.preamblePuncturing;
    element.mlo = capabilities.mlo;
    element.str = capabilities.str;
    element.nstr = capabilities.nstr;
    element.emlsr = capabilities.emlsr;
    element.emlmr = capabilities.emlmr;
    element.maxAmpduLengthExponent = capabilities.maxAmpduLengthExponent;
    element.maxMpduLength = capabilities.maxMpduLength;
    element.maxBlockAckBufferSize = capabilities.maxBlockAckBufferSize;
    return element;
}

/** Converts a received EHT capabilities element to the representation used by negotiation. */
inline Ieee80211EhtCapabilities makeEhtCapabilities(const Ieee80211EhtCapabilitiesElement& element)
{
    Ieee80211EhtCapabilities capabilities;
    capabilities.supportedChannelWidths.clear();
    
    if (element.supportedChannelWidth20MHz)
        capabilities.supportedChannelWidths.insert(Hz(20e6));
    if (element.supportedChannelWidth40MHz)
        capabilities.supportedChannelWidths.insert(Hz(40e6));
    if (element.supportedChannelWidth80MHz)
        capabilities.supportedChannelWidths.insert(Hz(80e6));
    if (element.supportedChannelWidth160MHz)
        capabilities.supportedChannelWidths.insert(Hz(160e6));
    if (element.supportedChannelWidth320MHz)
        capabilities.supportedChannelWidths.insert(Hz(320e6));
        
    copyEhtMcsNssMapFromElement(element.rxMcsNss, capabilities.rxMcsNss);
    copyEhtMcsNssMapFromElement(element.txMcsNss, capabilities.txMcsNss);
    
    capabilities.dlOfdma = element.dlOfdma;
    capabilities.ulOfdma = element.ulOfdma;
    capabilities.dlMuMimo = element.dlMuMimo;
    capabilities.ulMuMimo = element.ulMuMimo;
    capabilities.ldpc = element.ldpc;
    capabilities.support4096Qam = element.support4096Qam;
    capabilities.preamblePuncturing = element.preamblePuncturing;
    capabilities.mlo = element.mlo;
    capabilities.str = element.str;
    capabilities.nstr = element.nstr;
    capabilities.emlsr = element.emlsr;
    capabilities.emlmr = element.emlmr;
    capabilities.maxAmpduLengthExponent = element.maxAmpduLengthExponent;
    capabilities.maxMpduLength = element.maxMpduLength;
    capabilities.maxBlockAckBufferSize = element.maxBlockAckBufferSize;
    return capabilities;
}

inline Ieee80211EhtOperationElement makeEhtOperationElement(const Ieee80211EhtOperation& operation)
{
    Ieee80211EhtOperationElement element;
    element.operatingChannelWidthMHz = std::lround(operation.operatingChannelWidth.get() / 1e6);
    element.disabledSubchannelBitmap = operation.disabledSubchannelBitmap;
    element.basicEhtMcsNss = operation.basicEhtMcsNss;
    return element;
}

inline Ieee80211EhtOperation makeEhtOperation(const Ieee80211EhtOperationElement& element)
{
    Ieee80211EhtOperation operation;
    operation.operatingChannelWidth = Hz(element.operatingChannelWidthMHz * 1e6);
    operation.disabledSubchannelBitmap = element.disabledSubchannelBitmap;
    operation.basicEhtMcsNss = element.basicEhtMcsNss;
    return operation;
}

} // namespace ieee80211
} // namespace inet

#endif
