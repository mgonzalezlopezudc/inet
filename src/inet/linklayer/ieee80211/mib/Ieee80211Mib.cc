//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"

#include "inet/common/ModuleAccess.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IAntenna.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211EhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Channel.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211EhtPreamblePuncturing.h"
#include "inet/physicallayer/wireless/ieee80211/contract/IIeee80211VhtPacketRadio.h"
#include "inet/physicallayer/wireless/ieee80211/contract/IIeee80211HePacketRadio.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <sstream>

namespace inet {

namespace ieee80211 {

Define_Module(Ieee80211Mib);
simsignal_t Ieee80211Mib::peerAssociationChangedSignal = cComponent::registerSignal("peerAssociationChanged");

void Ieee80211Mib::updateLocalOperationalRates(const physicallayer::Ieee80211ModeSet *modeSet)
{
    updateLocalOperationalRates(modeSet, par("bssBasicRateCodes").stdstringValue());
}

void Ieee80211Mib::updateLocalOperationalRates(const physicallayer::Ieee80211ModeSet *modeSet, const std::string& configuredBasicRateCodes)
{
    if (modeSet == nullptr)
        throw cRuntimeError("Cannot derive legacy operational rates without an IEEE 802.11 mode set");
    std::vector<Ieee80211LegacyRate> operationalRates;
    for (int i = 0; i < modeSet->getNumModes(); ++i) {
        auto family = modeSet->getPhyFamily(i);
        if (family != physicallayer::Ieee80211PhyFamily::DSSS &&
                family != physicallayer::Ieee80211PhyFamily::ERP_OFDM &&
                family != physicallayer::Ieee80211PhyFamily::OFDM)
            continue;
        auto units = (int)std::ceil(modeSet->getMode(i)->getDataMode()->
                getNetBitrate().get<Mbps>() * 2);
        if (units < 1 || units > 127)
            continue;
        Ieee80211LegacyRate rate;
        rate.rate = units;
        rate.basic = modeSet->isMandatory(i);
        bool duplicate = false;
        for (auto& existing : operationalRates)
            if (existing.rate == rate.rate) {
                existing.basic = existing.basic || rate.basic;
                duplicate = true;
                break;
            }
        if (!duplicate)
            operationalRates.push_back(rate);
    }
    std::vector<Ieee80211LegacyRate> basicRates;
    if (configuredBasicRateCodes.empty()) {
        // Project default policy: use the PHY mandatory legacy rates as the BSS Basic Rate Set.
        for (const auto& rate : operationalRates)
            if (rate.basic)
                basicRates.push_back(rate);
    }
    else {
        cStringTokenizer tokenizer(configuredBasicRateCodes.c_str());
        std::set<int> seenCodes;
        while (tokenizer.hasMoreTokens()) {
            const char *token = tokenizer.nextToken();
            char *end = nullptr;
            long parsedCode = strtol(token, &end, 10);
            if (end == token || *end != '\0' || parsedCode < 1 || parsedCode > 127)
                throw cRuntimeError("Invalid BSS Basic rate code token '%s'; expected an integer from 1 to 127", token);
            int code = parsedCode;
            if (!seenCodes.insert(code).second)
                throw cRuntimeError("Duplicate BSS Basic rate code %d", code);
            auto it = std::find_if(operationalRates.begin(), operationalRates.end(),
                    [code](const auto& rate) { return rate.rate == code; });
            if (it == operationalRates.end())
                throw cRuntimeError("Configured BSS Basic rate code %d is not in the local Operational Rate Set", code);
            auto basicRate = *it;
            basicRate.basic = true;
            basicRates.push_back(basicRate);
        }
        for (auto& rate : operationalRates)
            rate.basic = std::any_of(basicRates.begin(), basicRates.end(),
                    [&rate](const auto& basicRate) { return basicRate.rate == rate.rate; });
    }
    localOperationalRates = std::move(operationalRates);
    localBssBasicRates = std::move(basicRates);
}

void Ieee80211Mib::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        WATCH(address);
        WATCH(mode);
        WATCH(qos);
        WATCH_EXPR("bssid", getBssid());
        WATCH_EXPR("stationType", getStationType());
        WATCH_EXPR("isAssociated", isAssociated());
        WATCH_EXPR("associationId", getLocalAssociationId());
        WATCH_EXPR("peerAssociationCount", getPeerAssociationSnapshots().size());
        WATCH_EXPR("peerAssociations", getPeerAssociationSummary());
        WATCH_EXPR("modeStr", getModeStr(mode));
        WATCH_EXPR("stationTypeStr", getStationTypeStr(getStationType()));
        WATCH_EXPR("qosStr", qos ? ", QoS" : ", Non-QoS");
        WATCH_EXPR("ssidStr", getSsidStr());
        WATCH_EXPR("ssid", getSsid().empty() ? std::string("-") : getSsid()); // associated SSID ("-" if none), for node display strings
        WATCH_EXPR("associatedStr", getStationType() == STATION ? (isAssociated() ? "\nAssociated" : "\nNot associated") : "");

        // Initialize local VHT capabilities
        localVhtCapabilities.ldpc = par("vhtLdpc").boolValue();
        localVhtCapabilities.rxLdpc = localVhtCapabilities.ldpc;
        // IEEE 802.11-2024 Figure 9-707 encodes VHT MCS maps as 0-7, 0-8, or 0-9.
        localVhtCapabilities.maxMcs = par("vhtMaxMcs").intValue();
        if (localVhtCapabilities.maxMcs != 7 && localVhtCapabilities.maxMcs != 8 && localVhtCapabilities.maxMcs != 9)
            throw cRuntimeError("vhtMaxMcs must be 7, 8, or 9");
        localVhtCapabilities.maxNss = par("vhtMaxNss").intValue();
        if (localVhtCapabilities.maxNss < 1 || localVhtCapabilities.maxNss > 8)
            throw cRuntimeError("vhtMaxNss must be between 1 and 8");
        localVhtCapabilities.maxNstsTotal = par("vhtMaxNstsTotal").intValue();
        if (localVhtCapabilities.maxNstsTotal < 0 || localVhtCapabilities.maxNstsTotal > 8)
            throw cRuntimeError("vhtMaxNstsTotal must be 0 or between 1 and 8");
        localVhtCapabilities.maxAmpduLengthExponent = par("vhtMaxAmpduLengthExponent").intValue();
        localVhtCapabilities.shortGi80 = par("vhtShortGi80").boolValue();
        localVhtCapabilities.shortGi160 = par("vhtShortGi160").boolValue();
        localVhtCapabilities.supports80Plus80MHz = par("vht80Plus80MHz").boolValue();
        const bool configuredMuBeamformer = par("vhtMuBeamformer").boolValue() || par("vhtMuMimo").boolValue();
        const bool configuredMuBeamformee = par("vhtMuBeamformee").boolValue();
        // IEEE Std 802.11-2024, Table 9-313: MU beamformer/beamformee
        // capability depends on the corresponding SU capability.
        localVhtCapabilities.suBeamformer = par("vhtSuBeamformer").boolValue() ||
                par("vhtBeamforming").boolValue() || configuredMuBeamformer;
        localVhtCapabilities.suBeamformee = par("vhtSuBeamformee").boolValue() || configuredMuBeamformee;
        localVhtCapabilities.beamformeeSts = par("vhtBeamformeeSts");
        localVhtCapabilities.soundingDimensions = par("vhtSoundingDimensions");
        if (localVhtCapabilities.beamformeeSts < 1 || localVhtCapabilities.beamformeeSts > 8 ||
                localVhtCapabilities.soundingDimensions < 1 || localVhtCapabilities.soundingDimensions > 8)
            throw cRuntimeError("VHT beamformee STS and sounding dimensions must be in the range 1..8");
        localVhtCapabilities.muBeamformer = configuredMuBeamformer;
        localVhtCapabilities.muBeamformee = configuredMuBeamformee;
        if (localVhtCapabilities.maxNstsTotal != 0 &&
                localVhtCapabilities.maxNstsTotal < localVhtCapabilities.beamformeeSts)
            throw cRuntimeError("vhtMaxNstsTotal must be at least vhtBeamformeeSts when nonzero");

        localHtLdpc = par("htLdpc").boolValue();
        localHtCapabilities.ldpc = localHtLdpc;
        int htMcsFeedback = par("htMcsFeedback").intValue();
        if (htMcsFeedback == 1 || htMcsFeedback < 0 || htMcsFeedback > 3)
            throw cRuntimeError("htMcsFeedback must be 0, 2, or 3 (1 is reserved)");
        localHtCapabilities.mcsFeedback = static_cast<Ieee80211HtMcsFeedback>(htMcsFeedback);
        localHtCapabilities.htcSupport = par("htHtcSupport").boolValue();
        localHtCapabilities.receiveNdp = par("htReceiveNdp").boolValue();
        localHtCapabilities.transmitNdp = par("htTransmitNdp").boolValue();
        auto readHtFeedbackCapability = [this](const char *parameter) {
            int value = par(parameter).intValue();
            if (value < 0 || value > 3)
                throw cRuntimeError("%s must be between 0 and 3", parameter);
            return static_cast<Ieee80211HtExplicitFeedback>(value);
        };
        localHtCapabilities.explicitCsiFeedback = readHtFeedbackCapability("htExplicitCsiFeedback");
        localHtCapabilities.explicitNoncompressedFeedback = readHtFeedbackCapability("htExplicitNoncompressedFeedback");
        localHtCapabilities.explicitCompressedFeedback = readHtFeedbackCapability("htExplicitCompressedFeedback");
        int htProtectionMode = par("htProtectionMode").intValue();
        if (htProtectionMode < 0 || htProtectionMode > 3)
            throw cRuntimeError("htProtectionMode must be between 0 and 3");
        htOperation.protectionMode = static_cast<Ieee80211HtProtectionMode>(htProtectionMode);
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
        // IEEE 802.11-2024 Table 9-378 encodes HE MCS maps as 0-7, 0-9, or 0-11.
        int heMaxMcs = par("heMaxMcs").intValue();
        if (heMaxMcs != 7 && heMaxMcs != 9 && heMaxMcs != 11)
            throw cRuntimeError("heMaxMcs must be 7, 9, or 11");
        int heMaxNss = par("heMaxNss").intValue();
        if (heMaxNss < 1 || heMaxNss > 8)
            throw cRuntimeError("heMaxNss must be between 1 and 8");
        cModule *radioModule = *par("radioModule").stringValue() ? getModuleFromPar<cModule>(par("radioModule"), this) : nullptr;
        // The opMode parameter is stable during local initialization, unlike a
        // provider's runtime mode-set pointer. Populate the snapshot here so
        // simplified peer management never depends on same-stage module order.
        if (radioModule != nullptr && radioModule->hasPar("opMode"))
            updateLocalOperationalRates(physicallayer::Ieee80211ModeSet::getModeSet(
                    radioModule->par("opMode").stringValue()));
        if (auto heRadio = dynamic_cast<physicallayer::IIeee80211HePacketRadio *>(radioModule))
            heRadio->setHeBssColor(heOperation.bssColor);
        if (auto vhtRadio = dynamic_cast<physicallayer::IIeee80211VhtPacketRadio *>(radioModule)) {
            int antennaCount = vhtRadio->getVhtAntennaCount();
            if (antennaCount < 1 || antennaCount > 8)
                throw cRuntimeError("Packet-level VHT radio antenna count must be in the range 1..8");
            localVhtCapabilities.maxNss = std::min(localVhtCapabilities.maxNss, antennaCount);
            localVhtCapabilities.beamformeeSts = std::min(localVhtCapabilities.beamformeeSts, antennaCount);
            localVhtCapabilities.soundingDimensions = std::min(localVhtCapabilities.soundingDimensions, antennaCount);
            if (localVhtCapabilities.maxNstsTotal != 0)
                localVhtCapabilities.maxNstsTotal = std::min(localVhtCapabilities.maxNstsTotal, antennaCount);
        }
        if (radioModule != nullptr) {
                int numAntennas = -1;
                auto radio = dynamic_cast<physicallayer::IRadio *>(radioModule);
                if (radio != nullptr && radio->getAntenna() != nullptr)
                    numAntennas = radio->getAntenna()->getNumAntennas();
                if (numAntennas <= 0) {
                    cModule *antennaModule = radioModule->getSubmodule("antenna");
                    if (antennaModule != nullptr && antennaModule->hasPar("numAntennas"))
                        numAntennas = antennaModule->par("numAntennas").intValue();
                }
                if (numAntennas <= 0) {
                    for (cModule::SubmoduleIterator it(radioModule); !it.end(); ++it) {
                        cModule *sub = *it;
                        if (sub->hasPar("numAntennas")) {
                            numAntennas = sub->par("numAntennas").intValue();
                            break;
                        }
                    }
                }
                if (numAntennas > 0)
                    heMaxNss = std::min(heMaxNss, numAntennas);
                EV_DETAIL << "Ieee80211Mib: " << getFullPath() << " resolved numAntennas=" << numAntennas << " final heMaxNss=" << heMaxNss << endl;
        }
        localHeCapabilities.rxMcsNss.maxMcsPerNss.fill(-1);
        localHeCapabilities.txMcsNss.maxMcsPerNss.fill(-1);
        for (int i = 0; i < heMaxNss; ++i) {
            localHeCapabilities.rxMcsNss.maxMcsPerNss[i] = heMaxMcs;
            localHeCapabilities.txMcsNss.maxMcsPerNss[i] = heMaxMcs;
        }
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
        localEhtCapabilities.ehtDup6GHz = par("ehtDup6GHz").boolValue();
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
        if (disabledSubchannelBitmap != 0 && !localEhtCapabilities.preamblePuncturing)
            throw cRuntimeError("ehtDisabledSubchannelBitmap requires ehtPreamblePuncturing=true");
        if (!physicallayer::isValidIeee80211EhtPreamblePuncturing(
                ehtOperation.disabledSubchannelBitmap, ehtOperation.operatingChannelWidth))
            throw cRuntimeError("ehtDisabledSubchannelBitmap is not a permitted EHT Operation pattern for %g MHz",
                    ehtOperation.operatingChannelWidth.get() / 1e6);
        ehtOperation.basicEhtMcsNss = par("ehtBasicMcsNss").intValue();
        ehtOperation.mcs15Disabled = par("ehtMcs15Disabled").boolValue();

        vhtOperation.ldpc = localVhtCapabilities.ldpc;
        vhtOperation.numSpatialStreams = localVhtCapabilities.maxNss;
        vhtOperation.basicMcsNss.maxMcsPerNss.fill(-1);
        for (int i = 0; i < vhtOperation.numSpatialStreams; ++i)
            vhtOperation.basicMcsNss.maxMcsPerNss[i] = 7;
        localVhtCapabilities.rxMcsNss.maxMcsPerNss.fill(-1);
        localVhtCapabilities.txMcsNss.maxMcsPerNss.fill(-1);
        if (localVhtCapabilities.maxNss < 1 || localVhtCapabilities.maxNss > 8)
            throw cRuntimeError("vhtMaxNss must be between 1 and 8");
        for (int i = 0; i < localVhtCapabilities.maxNss; i++) {
            localVhtCapabilities.rxMcsNss.maxMcsPerNss[i] = localVhtCapabilities.maxMcs;
            localVhtCapabilities.txMcsNss.maxMcsPerNss[i] = localVhtCapabilities.maxMcs;
        }
        localHtCapabilities.rxMcsNss.maxMcsPerNss.fill(-1);
        localHtCapabilities.txMcsNss.maxMcsPerNss.fill(-1);
        int htMaxMcs = par("htMaxMcs").intValue();
        if (htMaxMcs < 0 || htMaxMcs > 7)
            throw cRuntimeError("htMaxMcs must be between 0 and 7");
        int htMaxNss = par("htMaxNss").intValue();
        if (htMaxNss < 1 || htMaxNss > 4)
            throw cRuntimeError("htMaxNss must be between 1 and 4");
        for (int i = 0; i < htMaxNss; i++) {
            localHtCapabilities.rxMcsNss.maxMcsPerNss[i] = htMaxMcs;
            localHtCapabilities.txMcsNss.maxMcsPerNss[i] = htMaxMcs;
        }
        localHtCapabilities.shortGi20 = par("htShortGi20").boolValue();
        localHtCapabilities.shortGi40 = par("htShortGi40").boolValue();
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
        WATCH(localEhtCapabilities.ehtDup6GHz);
        WATCH(localEhtCapabilities.preamblePuncturing);
        WATCH(localEhtCapabilities.mlo);
        WATCH(localEhtCapabilities.str);
        WATCH(localEhtCapabilities.nstr);
        WATCH(localEhtCapabilities.emlsr);
        WATCH(localEhtCapabilities.emlmr);
        WATCH(ehtOperation.operatingChannelWidth);
        WATCH(ehtOperation.disabledSubchannelBitmap);
        WATCH(ehtOperation.basicEhtMcsNss);
        WATCH(ehtOperation.mcs15Disabled);
        WATCH_EXPR("peerCapabilityCount", getPeerCapabilitySnapshots().size());
        WATCH_EXPR("peerCapabilities", getPeerCapabilitySummary());
        WATCH_EXPR("peerLinkCount", getPeerLinkSnapshots().size());
        WATCH_EXPR("peerLinks", getPeerLinkSummary());
        WATCH_EXPR("heCapabilitiesSummary", getHeCapabilitiesSummary());
        WATCH_EXPR("heOperationSummary", getHeOperationSummary());
        WATCH_EXPR("ehtCapabilitiesSummary", getEhtCapabilitiesSummary());
        WATCH_EXPR("ehtOperationSummary", getEhtOperationSummary());
        WATCH_EXPR("negotiatedHePeers", getNegotiatedHePeerCount());
        WATCH_EXPR("negotiatedEhtPeers", getNegotiatedEhtPeerCount());
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        if (!*par("radioModule").stringValue())
            return;
        auto providerModule = getModuleFromPar<cModule>(par("radioModule"), this);
        modeSetProvider = dynamic_cast<physicallayer::IIeee80211ModeSetProvider *>(providerModule);
        const physicallayer::Ieee80211ModeSet *modeSet = modeSetProvider == nullptr &&
                providerModule->hasPar("opMode") ?
                physicallayer::Ieee80211ModeSet::getModeSet(
                        providerModule->par("opMode").stringValue()) :
                (modeSetProvider == nullptr ? nullptr : modeSetProvider->getModeSet());
        if (modeSet == nullptr)
            throw cRuntimeError("The configured IEEE 802.11 radio provides neither a mode set nor an opMode fallback");
        updateLocalOperationalRates(modeSet);
        // Layered IEEE 802.11 radios do not expose the packet-level mode-set
        // provider. Keep their legacy initialization intact; opt-in features
        // that require packet-level PHY authority reject them explicitly.
        if (modeSetProvider == nullptr)
            return;
        auto channel = modeSetProvider->getChannel();
        if (channel == nullptr)
            throw cRuntimeError("The configured IEEE 802.11 mode-set provider has no current channel");
        // VHT operation information has no 320 MHz encoding. EHT carries the
        // 320 MHz operation separately, so do not try to force a 320 MHz
        // channel through the VHT CCFS derivation.
        if (channel->getBand()->getChannelTopology() == physicallayer::Ieee80211ChannelTopology::NONCONTIGUOUS)
            vhtOperation = deriveIeee80211Vht80Plus80Operation(channel->getCenterFrequency(), channel->getSecondary80CenterFrequency());
        else if (channel->getOperatingChannelWidth() <= MHz(160))
            vhtOperation = deriveIeee80211VhtOperation(channel->getBondedCenterFrequency(), channel->getOperatingChannelWidth(),
                    channel->getPrimary80ChannelPosition());
        else
            vhtOperation = Ieee80211VhtOperation();
        vhtOperation.ldpc = localVhtCapabilities.ldpc;
        vhtOperation.numSpatialStreams = localVhtCapabilities.maxNss;
        if (vhtOperation.operatingChannelWidth == MHz(160))
            localVhtCapabilities.supportedChannelWidths.insert(MHz(160));
        if (vhtOperation.nonContiguous && !localVhtCapabilities.supports80Plus80MHz)
            throw cRuntimeError("The current non-contiguous VHT channel requires vht80Plus80MHz=true");
        vhtOperation.shortGi = vhtOperation.operatingChannelWidth == MHz(160) ?
                localVhtCapabilities.shortGi160 :
                vhtOperation.operatingChannelWidth == MHz(80) ? localVhtCapabilities.shortGi80 : false;
        auto configuredSecondaryChannelOffset = physicallayer::Ieee80211Channel::parseSecondaryChannelOffset(
                par("htSecondaryChannelOffset"));
        // The radio derives the secondary channel offset from the channel
        // grid for wide VHT/HE/EHT operation when the parameter is "none";
        // only an explicit disagreement is an error.
        if (configuredSecondaryChannelOffset != physicallayer::IEEE80211_SECONDARY_CHANNEL_NONE &&
                configuredSecondaryChannelOffset != channel->getSecondaryChannelOffset())
            throw cRuntimeError("MIB and radio htSecondaryChannelOffset parameters disagree");
        bool ht40Operation = physicallayer::Ieee80211ModeSet::isHtProfileName(modeSet->getProfileName()) &&
                configuredSecondaryChannelOffset != physicallayer::IEEE80211_SECONDARY_CHANNEL_NONE;
        // IEEE Std 802.11-2024, 9.4.2.55 (Figures 9-462/9-463 and Table
        // 9-134): width and secondary offset are advertised only for HT40.
        htOperation.operatingChannelWidth = ht40Operation ? Hz(MHz(40)) : Hz(MHz(20));
        htOperation.secondaryChannelOffset = ht40Operation ? configuredSecondaryChannelOffset :
                physicallayer::IEEE80211_SECONDARY_CHANNEL_NONE;
        // The 2.4 GHz band stores a zero-based vector index internally; the
        // HT Operation field carries the IEEE channel number (1 through 14).
        if (physicallayer::Ieee80211ModeSet::isHtProfileName(modeSet->getProfileName()))
            htOperation.primaryChannel = channel->getChannelNumber() + 1;
    }
}

int Ieee80211Mib::getNegotiatedHePeerCount() const
{
    int count = 0;
    for (const auto& peer : getPeerCapabilitySnapshots())
        if (peer.getNegotiatedHe() && (peer.getNegotiatedHe()->localTxPeerRx.valid || peer.getNegotiatedHe()->localRxPeerTx.valid))
            count++;
    return count;
}

int Ieee80211Mib::getNegotiatedEhtPeerCount() const
{
    int count = 0;
    for (const auto& peer : getPeerCapabilitySnapshots())
        if (peer.getNegotiatedEht() && peer.getNegotiatedEht()->valid)
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
           << ", EHT-DUP-6GHz=" << (localEhtCapabilities.ehtDup6GHz ? "yes" : "no")
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
           << ", basicMcsNss=" << ehtOperation.basicEhtMcsNss
           << ", MCS15=" << (ehtOperation.mcs15Disabled ? "disabled" : "enabled");
    return stream.str();
}

std::string Ieee80211Mib::getPeerCapabilitySummary() const
{
    std::stringstream stream;
    bool first = true;
    for (const auto& peer : getPeerCapabilitySnapshots()) {
        if (!first)
            stream << "; ";
        first = false;
        stream << peer.getAddress() << "{HT=" << (peer.getAdvertisedHt() ? "yes" : "no")
               << ",VHT=" << (peer.getAdvertisedVht() ? "yes" : "no")
               << ",HE=" << (peer.getAdvertisedHe() ? "yes" : "no")
               << ",EHT=" << (peer.getAdvertisedEht() ? "yes" : "no")
               << ",rates=" << (peer.getLegacyRates() ? peer.getLegacyRates()->size() : 0)
               << ",generation=" << peer.getGeneration() << "}";
    }
    return stream.str();
}

std::string Ieee80211Mib::getPeerLinkSummary() const
{
    std::stringstream stream;
    bool first = true;
    for (const auto& link : getPeerLinkSnapshots()) {
        if (!first)
            stream << "; ";
        first = false;
        stream << link.getAddress() << "{valid=" << (link.isValid() ? "yes" : "no")
               << ",txPower=" << link.getTransmitPowerDbm()
               << ",rxPower=" << link.getReceivedPowerDbm()
               << ",pathLoss=" << link.getPathLossDb()
               << ",generation=" << link.getGeneration() << "}";
    }
    return stream.str();
}

std::string Ieee80211Mib::getSsidStr() const
{
    if (mode == INFRASTRUCTURE)
        return "\nSSID: " + getSsid() + ", " + getBssid().str();
    return "";
}

Ieee80211Mib::LocalAssociationSnapshot Ieee80211Mib::getLocalAssociationSnapshot() const
{
    return associationState.getLocalSnapshot();
}

Ieee80211Mib::PeerAssociationSnapshot Ieee80211Mib::getPeerAssociationSnapshot(const MacAddress& address) const
{
    return associationState.getPeerSnapshot(address);
}

std::vector<Ieee80211Mib::PeerAssociationSnapshot> Ieee80211Mib::getPeerAssociationSnapshots() const
{
    return associationState.getPeerSnapshots();
}

std::string Ieee80211Mib::getPeerAssociationSummary() const
{
    std::stringstream stream;
    bool first = true;
    for (const auto& peer : getPeerAssociationSnapshots()) {
        if (!first)
            stream << "; ";
        first = false;
        stream << peer.getAddress() << "{status=";
        if (!peer.hasMemberStatus())
            stream << "-";
        else
            switch (peer.getMemberStatus()) {
                case NOT_AUTHENTICATED: stream << "NOT_AUTHENTICATED"; break;
                case AUTHENTICATED: stream << "AUTHENTICATED"; break;
                case ASSOCIATED: stream << "ASSOCIATED"; break;
                default: stream << "INVALID"; break;
            }
        stream << ",aid=";
        if (peer.hasAssociationId())
            stream << peer.getAssociationId();
        else
            stream << "-";
        stream << ",generation=" << peer.getGeneration() << "}";
    }
    return stream.str();
}

bool Ieee80211Mib::isPeerAssociated(const MacAddress& address) const
{
    auto snapshot = getPeerAssociationSnapshot(address);
    return snapshot.hasMemberStatus() && snapshot.getMemberStatus() == ASSOCIATED;
}

bool Ieee80211Mib::isPeerNotAuthenticated(const MacAddress& address) const
{
    auto snapshot = getPeerAssociationSnapshot(address);
    return !snapshot.hasMemberStatus() || snapshot.getMemberStatus() == NOT_AUTHENTICATED;
}

void Ieee80211Mib::setBssStationType(BssStationType stationType)
{
    associationState.setStationType(stationType);
}

void Ieee80211Mib::setBssIdentity(const std::string& ssid, const MacAddress& bssid)
{
    associationState.setBssIdentity(ssid, bssid);
}

void Ieee80211Mib::installLocalAssociation(const std::string& ssid, const MacAddress& bssid, short associationId)
{
    associationState.installLocalAssociation(ssid, bssid, associationId);
}

void Ieee80211Mib::clearLocalAssociation()
{
    associationState.clearLocalAssociation();
}

void Ieee80211Mib::setPeerMemberStatus(const MacAddress& address, BssMemberStatus memberStatus)
{
    if (memberStatus == ASSOCIATED)
        throw cRuntimeError("Use commitPeerAssociation() to associate an IEEE 802.11 peer");
    associationState.setPeerMemberStatus(address, memberStatus);
}

short Ieee80211Mib::reservePeerAssociation(const MacAddress& address)
{
    return associationState.reservePeerAssociation(address);
}

void Ieee80211Mib::releasePeerAssociationReservation(const MacAddress& address, short associationId)
{
    associationState.releasePeerAssociationReservation(address, associationId);
}

Ieee80211Mib::PeerAssociationSnapshot Ieee80211Mib::commitPeerAssociation(const MacAddress& address)
{
    auto transition = associationState.commitPeerAssociation(address);
    auto listeners = peerAssociationListeners;
    for (auto listener : listeners)
        listener->peerAssociationChanged(transition);
    Ieee80211PeerAssociationChangedEvent event(transition);
    emit(peerAssociationChangedSignal, &event);
    return transition.getNewSnapshot();
}

Ieee80211Mib::PeerAssociationSnapshot Ieee80211Mib::commitPeerAssociation(
        const MacAddress& address, short associationId)
{
    auto transition = associationState.commitPeerAssociation(address, associationId);
    auto listeners = peerAssociationListeners;
    for (auto listener : listeners)
        listener->peerAssociationChanged(transition);
    Ieee80211PeerAssociationChangedEvent event(transition);
    emit(peerAssociationChangedSignal, &event);
    return transition.getNewSnapshot();
}

Ieee80211Mib::PeerAssociationSnapshot Ieee80211Mib::clearPeerAssociation(
        const MacAddress& address, BssMemberStatus memberStatus)
{
    removePeerCapabilities(address);
    auto transition = associationState.clearPeerAssociation(address, memberStatus);
    if (transition.getAssociationEpoch() != 0) {
        auto listeners = peerAssociationListeners;
        for (auto listener : listeners)
            listener->peerAssociationChanged(transition);
        Ieee80211PeerAssociationChangedEvent event(transition);
        emit(peerAssociationChangedSignal, &event);
    }
    return transition.getNewSnapshot();
}

void Ieee80211Mib::addPeerAssociationListener(IIeee80211PeerAssociationListener *listener)
{
    if (listener == nullptr)
        throw cRuntimeError("Cannot register a null IEEE 802.11 peer association listener");
    if (std::find(peerAssociationListeners.begin(), peerAssociationListeners.end(), listener) == peerAssociationListeners.end())
        peerAssociationListeners.push_back(listener);
}

void Ieee80211Mib::removePeerAssociationListener(IIeee80211PeerAssociationListener *listener)
{
    peerAssociationListeners.erase(
            std::remove(peerAssociationListeners.begin(), peerAssociationListeners.end(), listener),
            peerAssociationListeners.end());
}

void Ieee80211Mib::setStationTransmitPower(const MacAddress& address, double transmitPowerDbm)
{
    peerLinkState.setTransmitPower(address, transmitPowerDbm);
}

void Ieee80211Mib::updateStationReceivedPower(const MacAddress& address, units::values::W receivedPower)
{
    peerLinkState.updateReceivedPower(address, receivedPower, simTime());
}

std::optional<Ieee80211Mib::PeerLinkSnapshot> Ieee80211Mib::getPeerLinkSnapshot(const MacAddress& address) const
{
    return peerLinkState.getSnapshot(address);
}

std::vector<Ieee80211Mib::PeerLinkSnapshot> Ieee80211Mib::getPeerLinkSnapshots() const
{
    return peerLinkState.getSnapshots();
}

Ieee80211Mib::PeerCapabilitySnapshot Ieee80211Mib::getPeerCapabilitySnapshot(const MacAddress& address) const
{
    return peerCapabilityState.getSnapshot(address);
}

std::vector<Ieee80211Mib::PeerCapabilitySnapshot> Ieee80211Mib::getPeerCapabilitySnapshots() const
{
    return peerCapabilityState.getSnapshots();
}

short Ieee80211Mib::getAssociationId(const MacAddress& address) const
{
    return associationState.getAssociationId(address);
}

MacAddress Ieee80211Mib::getStationAddress(short associationId) const
{
    return associationState.getStationAddress(associationId);
}

void Ieee80211Mib::setPeerHeCapabilities(const MacAddress& address,
        const Ieee80211HeCapabilities& capabilities, const Ieee80211HeOperation& operation)
{
    peerCapabilityState.setHe(address, capabilities,
            negotiateHeCapabilities(localHeCapabilities, capabilities, operation,
                    getStationType() == ACCESS_POINT));
}

void Ieee80211Mib::removePeerHeCapabilities(const MacAddress& address)
{
    peerCapabilityState.removeHe(address);
}

void Ieee80211Mib::removePeerCapabilities(const MacAddress& address)
{
    peerCapabilityState.removeAll(address);
}

Ieee80211SupportedRatesElement Ieee80211Mib::getSupportedRatesElement() const
{
    Ieee80211SupportedRatesElement element;
    element.numRates = std::min<size_t>(8, localOperationalRates.size());
    for (int i = 0; i < element.numRates; ++i)
        element.rates[i] = localOperationalRates[i];
    return element;
}

Ieee80211ExtendedSupportedRatesElement Ieee80211Mib::getExtendedSupportedRatesElement() const
{
    Ieee80211ExtendedSupportedRatesElement element;
    auto count = localOperationalRates.size() > 8 ? localOperationalRates.size() - 8 : 0;
    element.numRates = std::min<size_t>(255, count);
    for (int i = 0; i < element.numRates; ++i)
        element.rates[i] = localOperationalRates[i + 8];
    return element;
}

void Ieee80211Mib::setPeerLegacyRates(const MacAddress& address,
        const Ieee80211SupportedRatesElement& supportedRates,
        const Ieee80211ExtendedSupportedRatesElement& extendedSupportedRates)
{
    if (supportedRates.numRates < 1 || supportedRates.numRates > 8 ||
            extendedSupportedRates.numRates < 0 ||
            extendedSupportedRates.numRates > 255)
        throw cRuntimeError("Malformed peer Supported Rates element counts");
    std::vector<Ieee80211LegacyRate> rates;
    rates.reserve(supportedRates.numRates + extendedSupportedRates.numRates);
    for (int i = 0; i < supportedRates.numRates; ++i)
        if (supportedRates.rates[i].rate < 1 || supportedRates.rates[i].rate > 127)
            throw cRuntimeError("Malformed peer legacy rate code");
        else
            rates.push_back(supportedRates.rates[i]);
    for (int i = 0; i < extendedSupportedRates.numRates; ++i)
        if (extendedSupportedRates.rates[i].rate < 1 || extendedSupportedRates.rates[i].rate > 127)
            throw cRuntimeError("Malformed peer extended legacy rate code");
        else
            rates.push_back(extendedSupportedRates.rates[i]);
    peerCapabilityState.setLegacyRates(address, rates);
}

std::optional<std::vector<Ieee80211LegacyRate>> Ieee80211Mib::getPeerLegacyRates(
        const MacAddress& address) const
{
    return peerCapabilityState.getSnapshot(address).getLegacyRates();
}

std::vector<Ieee80211LegacyRate> Ieee80211Mib::getBssBasicLegacyRates() const
{
    if (getStationType() == STATION)
        return currentBssBasicRates;
    if (!localBssBasicRates.empty())
        return localBssBasicRates;
    std::vector<Ieee80211LegacyRate> fallback;
    for (const auto& rate : localOperationalRates)
        if (rate.basic)
            fallback.push_back(rate);
    return fallback;
}

void Ieee80211Mib::installCurrentBssBasicLegacyRates(
        const Ieee80211SupportedRatesElement& supportedRates,
        const Ieee80211ExtendedSupportedRatesElement& extendedSupportedRates)
{
    std::vector<Ieee80211LegacyRate> rates;
    auto appendBasic = [&rates](const Ieee80211LegacyRate& rate) {
        if (rate.rate < 1 || rate.rate > 127)
            throw cRuntimeError("Malformed current-BSS legacy rate code");
        if (rate.basic)
            rates.push_back(rate);
    };
    if (supportedRates.numRates < 1 || supportedRates.numRates > 8 ||
            extendedSupportedRates.numRates < 0 || extendedSupportedRates.numRates > 255)
        throw cRuntimeError("Malformed current-BSS Supported Rates element counts");
    for (int i = 0; i < supportedRates.numRates; ++i)
        appendBasic(supportedRates.rates[i]);
    for (int i = 0; i < extendedSupportedRates.numRates; ++i)
        appendBasic(extendedSupportedRates.rates[i]);
    currentBssBasicRates = std::move(rates);
}

std::optional<Ieee80211NegotiatedHeCapabilities> Ieee80211Mib::getNegotiatedHeCapabilities(
        const MacAddress& address) const
{
    return peerCapabilityState.getSnapshot(address).getNegotiatedHe();
}

bool Ieee80211Mib::isHeModeAllowedForPeer(const physicallayer::IIeee80211Mode *mode,
        const MacAddress& peerAddress) const
{
    // IEEE Std 802.11-2024, 10.6.5.8: the selected HE MCS/NSS and channel
    // width must be within the effective local-TX/peer-RX capability set.
    auto heMode = dynamic_cast<const physicallayer::Ieee80211HeMode *>(mode);
    if (heMode == nullptr)
        return true;
    auto dataMode = heMode->getDataMode();
    int nss = dataMode->getNumberOfSpatialStreams();
    int mcs = dataMode->getMcsIndex();
    if (nss < 1 || nss > 8 || mcs < 0 || mcs > 11)
        return false;
    bool extendedRangeSu = heMode->getPreambleMode()->getPreambleFormat() ==
            physicallayer::Ieee80211HePreambleMode::HE_PREAMBLE_ER_SU;
    if (peerAddress.isMulticast())
        return (!extendedRangeSu || !heOperation.erSuDisable) &&
                heOperation.basicHeMcsNss >= mcs &&
                dataMode->getBandwidth() <= heOperation.operatingChannelWidth;
    if (!isAssociated() && !isPeerAssociated(peerAddress))
        return false;
    auto negotiated = getNegotiatedHeCapabilities(peerAddress);
    return negotiated && negotiated->localTxPeerRx.valid &&
            negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[nss - 1] >= mcs &&
            negotiated->localTxPeerRx.supportedChannelWidths.count(dataMode->getBandwidth()) != 0 &&
            (!extendedRangeSu || !negotiated->operation.erSuDisable);
}

void Ieee80211Mib::setPeerEhtCapabilities(const MacAddress& address,
        const Ieee80211EhtCapabilities& capabilities, const Ieee80211EhtOperation& operation)
{
    peerCapabilityState.setEht(address, capabilities,
            negotiateEhtCapabilities(localEhtCapabilities, capabilities, operation));
}

void Ieee80211Mib::removePeerEhtCapabilities(const MacAddress& address)
{
    peerCapabilityState.removeEht(address);
}

std::optional<Ieee80211NegotiatedEhtCapabilities> Ieee80211Mib::getNegotiatedEhtCapabilities(
        const MacAddress& address) const
{
    return peerCapabilityState.getSnapshot(address).getNegotiatedEht();
}

bool Ieee80211Mib::isEhtModeAllowedForPeer(const physicallayer::IIeee80211Mode *mode,
        const MacAddress& peerAddress) const
{
    auto ehtMode = dynamic_cast<const physicallayer::Ieee80211EhtMode *>(mode);
    if (ehtMode == nullptr)
        return true;
    auto dataMode = ehtMode->getDataMode();
    int mcs = dataMode->getMcsIndex();
    if (mcs == 14) {
        auto negotiated = peerAddress.isMulticast() ? std::optional<Ieee80211NegotiatedEhtCapabilities>() : getNegotiatedEhtCapabilities(peerAddress);
        bool validWidth = dataMode->getBandwidth() == MHz(80) ||
                dataMode->getBandwidth() == MHz(160) || dataMode->getBandwidth() == MHz(320);
        return ehtMode->getCenterFrequencyMode() == physicallayer::Ieee80211EhtMode::BAND_6GHZ &&
                validWidth && dataMode->getNumberOfSpatialStreams() == 1 && dataMode->isLdpc() &&
                negotiated && negotiated->valid &&
                negotiated->intersection.ehtDup6GHz && negotiated->intersection.ldpc &&
                negotiated->operation.disabledSubchannelBitmap == 0;
    }
    if (mcs == 15) {
        auto negotiated = peerAddress.isMulticast() ? std::optional<Ieee80211NegotiatedEhtCapabilities>() : getNegotiatedEhtCapabilities(peerAddress);
        return dataMode->getNumberOfSpatialStreams() == 1 &&
                (!negotiated || !negotiated->operation.mcs15Disabled);
    }
    return true;
}

bool Ieee80211Mib::isHtModeAllowedForPeer(const physicallayer::IIeee80211Mode *mode,
        const MacAddress& peerAddress) const
{
    auto htMode = dynamic_cast<const physicallayer::Ieee80211HtMode *>(mode);
    if (htMode == nullptr)
        return true;
    if (peerAddress.isMulticast()) {
        auto dataMode = htMode->getDataMode();
        int nss = dataMode->getNumberOfSpatialStreams();
        int perStreamMcs = dataMode->getMcsIndex() % 8;
        return nss >= 1 && nss <= 4 &&
                htOperation.basicMcsNss.maxMcsPerNss[nss - 1] >= perStreamMcs &&
                dataMode->getBandwidth() <= htOperation.operatingChannelWidth;
    }
    if (!isAssociated() && !isPeerAssociated(peerAddress))
        return false;
    auto negotiated = getNegotiatedHtCapabilities(peerAddress);
    if (!negotiated || !negotiated->localTxPeerRx.valid)
        return false;
    auto dataMode = htMode->getDataMode();
    int nss = dataMode->getNumberOfSpatialStreams();
    int perStreamMcs = dataMode->getMcsIndex() % 8;
    if (nss < 1 || nss > 4 || negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[nss - 1] < perStreamMcs ||
            negotiated->localTxPeerRx.supportedChannelWidths.count(dataMode->getBandwidth()) == 0)
        return false;
    return dataMode->getGuardIntervalType() != physicallayer::Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT ||
            (dataMode->getBandwidth() == MHz(20) ? negotiated->localTxPeerRx.shortGi20 : negotiated->localTxPeerRx.shortGi40);
}

bool Ieee80211Mib::isVhtModeAllowedForPeer(const physicallayer::IIeee80211Mode *mode,
        const MacAddress& peerAddress) const
{
    auto vhtMode = dynamic_cast<const physicallayer::Ieee80211VhtMode *>(mode);
    if (vhtMode == nullptr)
        return true;
    if (peerAddress.isMulticast()) {
        auto dataMode = vhtMode->getDataMode();
        int nss = dataMode->getNumberOfSpatialStreams();
        return nss >= 1 && nss <= 8 &&
                vhtOperation.basicMcsNss.maxMcsPerNss[nss - 1] >= dataMode->getMcsIndex() &&
                dataMode->getBandwidth() <= vhtOperation.operatingChannelWidth;
    }
    if (!isAssociated() && !isPeerAssociated(peerAddress))
        return false;
    auto negotiated = getNegotiatedVhtCapabilities(peerAddress);
    if (!negotiated || !negotiated->localTxPeerRx.valid)
        return false;
    auto dataMode = vhtMode->getDataMode();
    int nss = dataMode->getNumberOfSpatialStreams();
    int mcs = dataMode->getMcsIndex();
    if (nss < 1 || nss > 8 || negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[nss - 1] < mcs ||
            negotiated->localTxPeerRx.supportedChannelWidths.count(dataMode->getBandwidth()) == 0)
        return false;
    if (dataMode->getGuardIntervalType() != physicallayer::Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT)
        return true;
    return dataMode->getBandwidth() == MHz(160) ? negotiated->localTxPeerRx.shortGi160 :
            dataMode->getBandwidth() == MHz(80) ? negotiated->localTxPeerRx.shortGi80 : true;
}

bool Ieee80211Mib::isLdpcAllowedForPeer(const physicallayer::IIeee80211Mode *mode,
        const MacAddress& peerAddress) const
{
    if (dynamic_cast<const physicallayer::Ieee80211HtMode *>(mode) != nullptr) {
        if (peerAddress.isMulticast())
            return false;
        if (!isHtModeAllowedForPeer(mode, peerAddress))
            return false;
        auto negotiated = getNegotiatedHtCapabilities(peerAddress);
        return negotiated && negotiated->localTxPeerRx.valid && negotiated->localTxPeerRx.ldpc;
    }
    if (dynamic_cast<const physicallayer::Ieee80211VhtMode *>(mode) != nullptr) {
        if (peerAddress.isMulticast())
            return false;
        if (!isVhtModeAllowedForPeer(mode, peerAddress))
            return false;
        auto negotiated = getNegotiatedVhtCapabilities(peerAddress);
        return negotiated && negotiated->localTxPeerRx.valid && negotiated->localTxPeerRx.ldpc;
    }
    if (dynamic_cast<const physicallayer::Ieee80211HeMode *>(mode) != nullptr) {
        if (!peerAddress.isMulticast()) {
            auto negotiated = getNegotiatedHeCapabilities(peerAddress);
            if (negotiated)
                return negotiated->mutual.ldpc;
        }
        return localHeCapabilities.ldpc;
    }
    if (dynamic_cast<const physicallayer::Ieee80211EhtMode *>(mode) != nullptr) {
        if (!peerAddress.isMulticast()) {
            auto negotiated = getNegotiatedEhtCapabilities(peerAddress);
            if (negotiated)
                return negotiated->intersection.ldpc;
        }
        return localEhtCapabilities.ldpc;
    }
    return false;
}

void Ieee80211Mib::setPeerVhtCapabilities(const MacAddress& address,
        const Ieee80211VhtCapabilities& capabilities, const Ieee80211VhtOperation& operation)
{
    peerCapabilityState.setVht(address, capabilities,
            negotiateVhtCapabilities(localVhtCapabilities, capabilities, operation));
}

void Ieee80211Mib::removePeerVhtCapabilities(const MacAddress& address)
{
    peerCapabilityState.removeVht(address);
}

uint64_t Ieee80211Mib::getVhtAssociationGeneration(const MacAddress& address) const
{
    return peerCapabilityState.getSnapshot(address).getVhtGeneration();
}

std::optional<Ieee80211NegotiatedVhtCapabilities> Ieee80211Mib::getNegotiatedVhtCapabilities(
        const MacAddress& address) const
{
    return peerCapabilityState.getSnapshot(address).getNegotiatedVht();
}

void Ieee80211Mib::setPeerHtCapabilities(const MacAddress& address,
        const Ieee80211HtCapabilities& capabilities, const Ieee80211HtOperation& operation)
{
    peerCapabilityState.setHt(address, capabilities,
            negotiateHtCapabilities(localHtCapabilities, capabilities, operation));
}

void Ieee80211Mib::removePeerHtCapabilities(const MacAddress& address)
{
    peerCapabilityState.removeHt(address);
}

std::optional<Ieee80211NegotiatedHtCapabilities> Ieee80211Mib::getNegotiatedHtCapabilities(const MacAddress& address) const
{
    return peerCapabilityState.getSnapshot(address).getNegotiatedHt();
}

uint64_t Ieee80211Mib::getHtAssociationGeneration(const MacAddress& address) const
{
    return peerCapabilityState.getSnapshot(address).getHtGeneration();
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
}

} // namespace ieee80211

} // namespace inet
