//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"

#include <cmath>
#include <sstream>

namespace inet {

namespace ieee80211 {

Define_Module(Ieee80211Mib);

void Ieee80211Mib::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        WATCH(address);
        WATCH(mode);
        WATCH(qos);
        WATCH(bssData.bssid);
        WATCH(bssStationData.stationType);
        WATCH(bssStationData.isAssociated);
        WATCH(bssStationData.associationId);
        WATCH(bssAccessPointData.stations);
        WATCH(bssAccessPointData.associationIds);
        WATCH_EXPR("modeStr", getModeStr(mode));
        WATCH_EXPR("stationTypeStr", getStationTypeStr(bssStationData.stationType));
        WATCH_EXPR("qosStr", qos ? ", QoS" : ", Non-QoS");
        WATCH_EXPR("ssidStr", getSsidStr());
        WATCH_EXPR("ssid", bssData.ssid.empty() ? std::string("-") : bssData.ssid); // associated SSID ("-" if none), for node display strings
        WATCH_EXPR("associatedStr", bssStationData.stationType == STATION ? (bssStationData.isAssociated ? "\nAssociated" : "\nNot associated") : "");

        // Initialize local VHT capabilities
        localVhtCapabilities.ldpc = par("vhtLdpc").boolValue();
        localVhtCapabilities.maxNss = par("vhtMaxNss").intValue();
        localVhtCapabilities.maxAmpduLengthExponent = par("vhtMaxAmpduLengthExponent").intValue();
        localVhtCapabilities.txBeamforming = par("vhtBeamforming").boolValue();
        localVhtCapabilities.muMimo = par("vhtMuMimo").boolValue();

        localHtLdpc = par("htLdpc").boolValue();
        localHeCapabilities.ldpc = par("heLdpc").boolValue();
        localHeCapabilities.twtRequester = par("heTwtRequester").boolValue();
        localHeCapabilities.twtResponder = par("heTwtResponder").boolValue();
        localHeCapabilities.broadcastTwt = par("heBroadcastTwt").boolValue();
        localHeCapabilities.dynamicFragmentationLevel = par("heDynamicFragmentationLevel").intValue();
        if (localHeCapabilities.dynamicFragmentationLevel < 0 || localHeCapabilities.dynamicFragmentationLevel > 3)
            throw cRuntimeError("heDynamicFragmentationLevel must be between 0 and 3");
        localHeCapabilities.omControl = par("heOmControl").boolValue();
        localHeCapabilities.twoNav = par("heTwoNav").boolValue();
        localHeCapabilities.erBss = par("heErBss").boolValue();
        // IEEE 802.11-2024, 26.17.6: an AP operating an ER BSS shall not
        // advertise ER SU reception as disabled in its HE Operation element.
        heOperation.erSuDisable = !localHeCapabilities.erBss;
        localHeCapabilities.ndpFeedbackReport = par("heNdpFeedbackReport").boolValue();
        localHeCapabilities.multiTidAggregationRx = par("heMultiTidAggregationRx").boolValue();
        localHeCapabilities.multiTidAggregationTx = par("heMultiTidAggregationTx").boolValue();
        localHeCapabilities.dlMuMimoBeamformer = par("heDlMuMimoBeamformer").boolValue();
        localHeCapabilities.dlMuMimoBeamformee = par("heDlMuMimoBeamformee").boolValue();
        localHeCapabilities.fullBandwidthUlMuMimo = par("heFullBandwidthUlMuMimo").boolValue();
        localHeCapabilities.partialBandwidthUlMuMimo = par("hePartialBandwidthUlMuMimo").boolValue();
        if (localHeCapabilities.partialBandwidthUlMuMimo && !localHeCapabilities.fullBandwidthUlMuMimo)
            throw cRuntimeError("partial-bandwidth HE UL MU-MIMO requires full-bandwidth HE UL MU-MIMO support");
        localHeCapabilities.soundingDimensions = par("heSoundingDimensions").intValue();
        localHeCapabilities.beamformeeSts20Mhz = par("heBeamformeeSts20Mhz").intValue();
        localHeCapabilities.beamformeeStsAbove20Mhz = par("heBeamformeeStsAbove20Mhz").intValue();
        localHeCapabilities.feedbackMode = par("heFeedbackMode").intValue();
        int defaultPeDurationUs = par("heDefaultPeDurationUs").intValue();
        if (defaultPeDurationUs != 0 && defaultPeDurationUs != 4 &&
                defaultPeDurationUs != 8 && defaultPeDurationUs != 12 &&
                defaultPeDurationUs != 16)
            throw cRuntimeError("heDefaultPeDurationUs must be 0, 4, 8, 12, or 16 us");
        heOperation.defaultPeDurationPresent = defaultPeDurationUs != 0;
        heOperation.defaultPeDurationUs = defaultPeDurationUs;

        // IEEE 802.11-2024 Clause 9.4.2.249 ("HE Operation element").
        // HE BSS Color is a 6-bit identifier (valid range 1 to 63). A value of 0 indicates BSS Color is disabled.
        // It is advertised in beacons and association responses so associated stations can configure
        // their OBSS packet detection thresholds.
        int heBssColor = par("heBssColor").intValue();
        if (heBssColor < 0 || heBssColor > 63)
            throw cRuntimeError("heBssColor must be between 0 and 63");
        heOperation.bssColor = heBssColor;

        localEhtCapabilities.ldpc = par("ehtLdpc").boolValue();
        localEhtCapabilities.dlOfdma = par("ehtDlOfdma").boolValue();
        localEhtCapabilities.ulOfdma = par("ehtUlOfdma").boolValue();
        localEhtCapabilities.dlMuMimo = par("ehtDlMuMimo").boolValue();
        localEhtCapabilities.ulMuMimo = par("ehtUlMuMimo").boolValue();
        localEhtCapabilities.support4096Qam = par("eht4096Qam").boolValue();
        localEhtCapabilities.preamblePuncturing = par("ehtPreamblePuncturing").boolValue();
        localEhtCapabilities.mlo = par("ehtMlo").boolValue();
        localEhtCapabilities.str = par("ehtStr").boolValue();
        localEhtCapabilities.nstr = par("ehtNstr").boolValue();
        localEhtCapabilities.emlsr = par("ehtEmlsr").boolValue();
        localEhtCapabilities.emlmr = par("ehtEmlmr").boolValue();
        int ehtMaxMcs = par("ehtMaxMcs").intValue();
        if (ehtMaxMcs < 0 || ehtMaxMcs > 13)
            throw cRuntimeError("ehtMaxMcs must be between 0 and 13");
        int ehtMaxNss = par("ehtMaxNss").intValue();
        if (ehtMaxNss < 1 || ehtMaxNss > 8)
            throw cRuntimeError("ehtMaxNss must be between 1 and 8");
        localEhtCapabilities.rxMcsNss.maxMcsPerNss.fill(-1);
        localEhtCapabilities.txMcsNss.maxMcsPerNss.fill(-1);
        for (int i = 0; i < ehtMaxNss; ++i) {
            localEhtCapabilities.rxMcsNss.maxMcsPerNss[i] = ehtMaxMcs;
            localEhtCapabilities.txMcsNss.maxMcsPerNss[i] = ehtMaxMcs;
        }
        ehtOperation.operatingChannelWidth = Hz(par("ehtOperatingChannelWidth").doubleValue());
        if (localEhtCapabilities.supportedChannelWidths.count(ehtOperation.operatingChannelWidth) == 0)
            throw cRuntimeError("ehtOperatingChannelWidth must be 20, 40, 80, 160, or 320 MHz");
        int disabledSubchannelBitmap = par("ehtDisabledSubchannelBitmap").intValue();
        if (disabledSubchannelBitmap < 0 || disabledSubchannelBitmap > 0xffff)
            throw cRuntimeError("ehtDisabledSubchannelBitmap must be between 0 and 65535");
        ehtOperation.disabledSubchannelBitmap = disabledSubchannelBitmap;
        ehtOperation.basicEhtMcsNss = par("ehtBasicMcsNss").intValue();

        vhtOperation.operatingChannelWidth = Hz(par("vhtOperatingChannelWidth").doubleValue());
        vhtOperation.ldpc = localVhtCapabilities.ldpc;
        vhtOperation.numSpatialStreams = localVhtCapabilities.maxNss;

        WATCH(localHtLdpc);
        WATCH(localHeCapabilities.ldpc);
        WATCH(localHeCapabilities.twtRequester);
        WATCH(localHeCapabilities.twtResponder);
        WATCH(localHeCapabilities.broadcastTwt);
        WATCH(localHeCapabilities.dynamicFragmentationLevel);
        WATCH(localHeCapabilities.omControl);
        WATCH(localHeCapabilities.twoNav);
        WATCH(localHeCapabilities.erBss);
        WATCH(localHeCapabilities.ndpFeedbackReport);
        WATCH(localHeCapabilities.multiTidAggregationRx);
        WATCH(localHeCapabilities.multiTidAggregationTx);
        WATCH(localHeCapabilities.dlMuMimoBeamformer);
        WATCH(localHeCapabilities.dlMuMimoBeamformee);
        WATCH(localHeCapabilities.fullBandwidthUlMuMimo);
        WATCH(localHeCapabilities.partialBandwidthUlMuMimo);
        WATCH(localHeCapabilities.soundingDimensions);
        WATCH(localHeCapabilities.beamformeeSts20Mhz);
        WATCH(localHeCapabilities.beamformeeStsAbove20Mhz);
        WATCH(localHeCapabilities.feedbackMode);
        WATCH(heOperation.bssColor);
        WATCH(heOperation.defaultPeDurationPresent);
        WATCH(heOperation.defaultPeDurationUs);
        WATCH(localEhtCapabilities.ldpc);
        WATCH(localEhtCapabilities.dlOfdma);
        WATCH(localEhtCapabilities.ulOfdma);
        WATCH(localEhtCapabilities.support4096Qam);
        WATCH(localEhtCapabilities.preamblePuncturing);
        WATCH(localEhtCapabilities.mlo);
        WATCH(localEhtCapabilities.str);
        WATCH(localEhtCapabilities.nstr);
        WATCH(localEhtCapabilities.emlsr);
        WATCH(localEhtCapabilities.emlmr);
        WATCH(ehtOperation.operatingChannelWidth);
        WATCH(ehtOperation.disabledSubchannelBitmap);
        WATCH(ehtOperation.basicEhtMcsNss);
        WATCH_MAP(bssAccessPointData.links);
        WATCH_MAP(bssAccessPointData.advertisedHeCapabilities);
        WATCH_MAP(bssAccessPointData.negotiatedHeCapabilities);
        WATCH_MAP(bssAccessPointData.advertisedEhtCapabilities);
        WATCH_MAP(bssAccessPointData.negotiatedEhtCapabilities);
        WATCH_EXPR("heCapabilitiesSummary", getHeCapabilitiesSummary());
        WATCH_EXPR("heOperationSummary", getHeOperationSummary());
        WATCH_EXPR("ehtCapabilitiesSummary", getEhtCapabilitiesSummary());
        WATCH_EXPR("ehtOperationSummary", getEhtOperationSummary());
        WATCH_EXPR("negotiatedHePeers", getNegotiatedHePeerCount());
        WATCH_EXPR("negotiatedEhtPeers", getNegotiatedEhtPeerCount());
    }
}

int Ieee80211Mib::getNegotiatedHePeerCount() const
{
    int count = 0;
    for (const auto& entry : bssAccessPointData.negotiatedHeCapabilities)
        if (entry.second.valid)
            count++;
    return count;
}

int Ieee80211Mib::getNegotiatedEhtPeerCount() const
{
    int count = 0;
    for (const auto& entry : bssAccessPointData.negotiatedEhtCapabilities)
        if (entry.second.valid)
            count++;
    return count;
}

std::string Ieee80211Mib::getHeCapabilitiesSummary() const
{
    std::stringstream stream;
    stream << "LDPC=" << (localHeCapabilities.ldpc ? "yes" : "no")
           << ", DL-OFDMA=" << (localHeCapabilities.dlOfdma ? "yes" : "no")
           << ", UL-OFDMA=" << (localHeCapabilities.ulOfdma ? "yes" : "no")
           << ", TWT=" << (localHeCapabilities.twtRequester || localHeCapabilities.twtResponder || localHeCapabilities.broadcastTwt ? "yes" : "no")
           << ", dynFrag=" << localHeCapabilities.dynamicFragmentationLevel
           << ", OMI=" << (localHeCapabilities.omControl ? "yes" : "no")
           << ", twoNAV=" << (localHeCapabilities.twoNav ? "yes" : "no")
           << ", ER-BSS=" << (localHeCapabilities.erBss ? "yes" : "no")
           << ", NDP-FB=" << (localHeCapabilities.ndpFeedbackReport ? "yes" : "no")
           << ", MU-MIMO BFer=" << (localHeCapabilities.dlMuMimoBeamformer ? "yes" : "no")
           << ", BFmee=" << (localHeCapabilities.dlMuMimoBeamformee ? "yes" : "no")
           << ", UL-MU-MIMO=" << (localHeCapabilities.fullBandwidthUlMuMimo ? "full" : "no")
           << ", maxTxNss=" << getMaxNss(localHeCapabilities.txMcsNss)
           << ", maxRxNss=" << getMaxNss(localHeCapabilities.rxMcsNss)
           << ", peers=" << getNegotiatedHePeerCount();
    return stream.str();
}

std::string Ieee80211Mib::getEhtCapabilitiesSummary() const
{
    std::stringstream stream;
    stream << "LDPC=" << (localEhtCapabilities.ldpc ? "yes" : "no")
           << ", DL-OFDMA=" << (localEhtCapabilities.dlOfdma ? "yes" : "no")
           << ", UL-OFDMA=" << (localEhtCapabilities.ulOfdma ? "yes" : "no")
           << ", 4096-QAM=" << (localEhtCapabilities.support4096Qam ? "yes" : "no")
           << ", puncturing=" << (localEhtCapabilities.preamblePuncturing ? "yes" : "no")
           << ", MLO=" << (localEhtCapabilities.mlo ? "yes" : "no")
           << ", EMLSR=" << (localEhtCapabilities.emlsr ? "yes" : "no")
           << ", EMLMR=" << (localEhtCapabilities.emlmr ? "yes" : "no")
           << ", maxTxNss=" << getMaxNss(localEhtCapabilities.txMcsNss)
           << ", maxRxNss=" << getMaxNss(localEhtCapabilities.rxMcsNss)
           << ", peers=" << getNegotiatedEhtPeerCount();
    return stream.str();
}

std::string Ieee80211Mib::getHeOperationSummary() const
{
    std::stringstream stream;
    stream << "bssColor=" << (int)heOperation.bssColor
           << ", width=" << heOperation.operatingChannelWidth
           << ", erSu=" << (heOperation.erSuDisable ? "disabled" : "enabled")
           << ", defaultPE=" << heOperation.defaultPeDurationUs << "us"
           << ", basicMcsNss=" << heOperation.basicHeMcsNss;
    return stream.str();
}

std::string Ieee80211Mib::getEhtOperationSummary() const
{
    std::stringstream stream;
    stream << "width=" << ehtOperation.operatingChannelWidth
           << ", disabledSubchannels=0x" << std::hex << ehtOperation.disabledSubchannelBitmap << std::dec
           << ", basicMcsNss=" << ehtOperation.basicEhtMcsNss;
    return stream.str();
}

std::string Ieee80211Mib::getSsidStr() const
{
    if (mode == INFRASTRUCTURE)
        return "\nSSID: " + bssData.ssid + ", " + bssData.bssid.str();
    return "";
}

void Ieee80211Mib::setStationTransmitPower(const MacAddress& address, double transmitPowerDbm)
{
    auto& link = bssAccessPointData.links[address];
    link.transmitPowerDbm = transmitPowerDbm;
    if (!std::isnan(link.receivedPowerDbm)) {
        link.pathLossDb = link.transmitPowerDbm - link.receivedPowerDbm;
        link.valid = true;
    }
}

void Ieee80211Mib::updateStationReceivedPower(const MacAddress& address, units::values::W receivedPower)
{
    if (receivedPower.get() <= 0)
        return;
    auto& link = bssAccessPointData.links[address];
    link.receivedPowerDbm = 10 * std::log10(receivedPower.get() / 1e-3);
    link.pathLossDb = link.transmitPowerDbm - link.receivedPowerDbm;
    link.lastUpdate = simTime();
    link.valid = true;
}

const Ieee80211Mib::BssAccessPointData::LinkData *Ieee80211Mib::findStationLink(const MacAddress& address) const
{
    auto it = bssAccessPointData.links.find(address);
    return it == bssAccessPointData.links.end() ? nullptr : &it->second;
}

short Ieee80211Mib::allocateAssociationId(const MacAddress& address)
{
    auto existing = bssAccessPointData.associationIds.find(address);
    if (existing != bssAccessPointData.associationIds.end())
        return existing->second;
    for (short aid = 1; aid <= 2007; aid++) {
        bool used = false;
        for (const auto& entry : bssAccessPointData.associationIds)
            if (entry.second == aid) {
                used = true;
                break;
            }
        if (!used) {
            bssAccessPointData.associationIds[address] = aid;
            return aid;
        }
    }
    throw cRuntimeError("No IEEE 802.11 association ID is available");
}

void Ieee80211Mib::releaseAssociationId(const MacAddress& address)
{
    bssAccessPointData.associationIds.erase(address);
    removePeerHeCapabilities(address);
    removePeerEhtCapabilities(address);
    removePeerVhtCapabilities(address);
}

short Ieee80211Mib::getAssociationId(const MacAddress& address) const
{
    auto it = bssAccessPointData.associationIds.find(address);
    return it == bssAccessPointData.associationIds.end() ? -1 : it->second;
}

MacAddress Ieee80211Mib::getStationAddress(short associationId) const
{
    for (const auto& entry : bssAccessPointData.associationIds)
        if (entry.second == associationId)
            return entry.first;
    return MacAddress::UNSPECIFIED_ADDRESS;
}

void Ieee80211Mib::setPeerHeCapabilities(const MacAddress& address,
        const Ieee80211HeCapabilities& capabilities, const Ieee80211HeOperation& operation)
{
    bssAccessPointData.advertisedHeCapabilities[address] = capabilities;
    bssAccessPointData.negotiatedHeCapabilities[address] =
            negotiateHeCapabilities(localHeCapabilities, capabilities, operation);
}

void Ieee80211Mib::removePeerHeCapabilities(const MacAddress& address)
{
    bssAccessPointData.advertisedHeCapabilities.erase(address);
    bssAccessPointData.negotiatedHeCapabilities.erase(address);
}

const Ieee80211NegotiatedHeCapabilities *Ieee80211Mib::findNegotiatedHeCapabilities(
        const MacAddress& address) const
{
    auto it = bssAccessPointData.negotiatedHeCapabilities.find(address);
    return it == bssAccessPointData.negotiatedHeCapabilities.end() ? nullptr : &it->second;
}

void Ieee80211Mib::setPeerEhtCapabilities(const MacAddress& address,
        const Ieee80211EhtCapabilities& capabilities, const Ieee80211EhtOperation& operation)
{
    bssAccessPointData.advertisedEhtCapabilities[address] = capabilities;
    bssAccessPointData.negotiatedEhtCapabilities[address] =
            negotiateEhtCapabilities(localEhtCapabilities, capabilities, operation);
}

void Ieee80211Mib::removePeerEhtCapabilities(const MacAddress& address)
{
    bssAccessPointData.advertisedEhtCapabilities.erase(address);
    bssAccessPointData.negotiatedEhtCapabilities.erase(address);
}

const Ieee80211NegotiatedEhtCapabilities *Ieee80211Mib::findNegotiatedEhtCapabilities(
        const MacAddress& address) const
{
    auto it = bssAccessPointData.negotiatedEhtCapabilities.find(address);
    return it == bssAccessPointData.negotiatedEhtCapabilities.end() ? nullptr : &it->second;
}

void Ieee80211Mib::setPeerVhtCapabilities(const MacAddress& address,
        const Ieee80211VhtCapabilities& capabilities, const Ieee80211VhtOperation& operation)
{
    bssAccessPointData.advertisedVhtCapabilities[address] = capabilities;
    bssAccessPointData.negotiatedVhtCapabilities[address] =
            negotiateVhtCapabilities(localVhtCapabilities, capabilities, operation);
}

void Ieee80211Mib::removePeerVhtCapabilities(const MacAddress& address)
{
    bssAccessPointData.advertisedVhtCapabilities.erase(address);
    bssAccessPointData.negotiatedVhtCapabilities.erase(address);
}

const Ieee80211NegotiatedVhtCapabilities *Ieee80211Mib::findNegotiatedVhtCapabilities(
        const MacAddress& address) const
{
    auto it = bssAccessPointData.negotiatedVhtCapabilities.find(address);
    return it == bssAccessPointData.negotiatedVhtCapabilities.end() ? nullptr : &it->second;
}

const char *Ieee80211Mib::getModeStr(Ieee80211Mib::Mode mode)
{
    switch (mode) {
        case INFRASTRUCTURE: return "Infrastructure";
        case INDEPENDENT: return "Ad-hoc";
        case MESH: return "Mesh";
        default: return "?";
    }
}

const char *Ieee80211Mib::getStationTypeStr(Ieee80211Mib::BssStationType stationType)
{
    switch (stationType) {
        case ACCESS_POINT: return ", AP";
        case STATION: return ", STA";
        default: return "";
    }
}

} // namespace ieee80211

} // namespace inet
