//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mgmt/Ieee80211CapabilityElements.h"

#include <algorithm>

namespace inet {
namespace ieee80211 {

using namespace physicallayer;

namespace {

uint16_t makeVhtMcsMap(const Ieee80211ModeSet *modeSet, int maximumSpatialStreams)
{
    int maximumMcs[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    if (modeSet != nullptr) {
        for (int i = 0; i < modeSet->getNumModes(); i++) {
            const auto *dataMode = modeSet->getMode(i)->getDataMode();
            if (dataMode->getPhyFormat() != Ieee80211PhyFormat::VHT_SU)
                continue;
            int numberOfSpatialStreams = dataMode->getNumberOfSpatialStreams();
            if (numberOfSpatialStreams >= 1 && numberOfSpatialStreams <= maximumSpatialStreams &&
                numberOfSpatialStreams <= 8)
                maximumMcs[numberOfSpatialStreams - 1] = std::max(
                        maximumMcs[numberOfSpatialStreams - 1], static_cast<int>(dataMode->getMcsIndex()));
        }
    }

    uint16_t map = 0;
    for (int spatialStream = 0; spatialStream < 8; spatialStream++) {
        int value = maximumMcs[spatialStream] < 0 ? 3 :
                    maximumMcs[spatialStream] <= 7 ? 0 :
                    maximumMcs[spatialStream] == 8 ? 1 : 2;
        map |= value << (2 * spatialStream);
    }
    return map;
}

} // namespace

void populateIeee80211CapabilityElements(const Ptr<Ieee80211MgmtFrame>& frame,
        const Ieee80211ModeSet *modeSet, int maximumSpatialStreams,
        bool htLdpcRxSupported, bool vhtLdpcRxSupported)
{
    // IEEE Std 802.11-2024: HT Capabilities 9.4.2.54.2/.4 and Table 9-224
    // define LDPC/width and the HT-MCS bitmask; VHT Capabilities
    // 9.4.2.156.2/.3 and Tables 9-313 to 9-315 define LDPC/width and the
    // per-NSS VHT-MCS map. The No LDPC preference is B3 of the Operating
    // Mode field (9.4.1.51, Table 9-110; transmit behavior in 10.15).
    // Clause 19.1.1 limits HT to four spatial streams; the VHT map has eight.
    if (maximumSpatialStreams < 1 || maximumSpatialStreams > 8)
        throw cRuntimeError("IEEE 802.11 capability generation requires 1 to 8 spatial streams");
    bool hasHt = false;
    bool hasVht = false;
    bool htSupports40Mhz = false;
    bool vhtSupports160Mhz = false;
    bool htMcsSupported[80] = {};
    if (modeSet != nullptr) {
        for (int i = 0; i < modeSet->getNumModes(); i++) {
            const auto *dataMode = modeSet->getMode(i)->getDataMode();
            auto format = dataMode->getPhyFormat();
            if (format == Ieee80211PhyFormat::HT) {
                hasHt = true;
                htSupports40Mhz |= dataMode->getBandwidth() == MHz(40);
                int mcs = dataMode->getMcsIndex();
                if (dataMode->getNumberOfSpatialStreams() <= maximumSpatialStreams &&
                    mcs >= 0 && mcs < 80)
                    htMcsSupported[mcs] = true;
            }
            else if (format == Ieee80211PhyFormat::VHT_SU) {
                hasVht = true;
                vhtSupports160Mhz |= dataMode->getBandwidth() == MHz(160);
                // A VHT STA is also an HT STA. Its compatible HT receive MCS
                // mask contains MCS 0-7 for each supported VHT spatial stream.
                int nss = dataMode->getNumberOfSpatialStreams();
                int mcs = dataMode->getMcsIndex();
                // HT equal-modulation MCS 0-31 represent only NSS 1-4.
                // VHT NSS 5-8 must not be advertised as HT MCS 32-63,
                // because those HT indices have different meanings.
                if (nss >= 1 && nss <= maximumSpatialStreams && nss <= 4 &&
                    mcs >= 0 && mcs <= 7)
                    htMcsSupported[8 * (nss - 1) + mcs] = true;
            }
        }
    }
    hasHt |= hasVht;
    htSupports40Mhz |= hasVht;

    int addedBytes = 0;
    if (hasHt) {
        frame->setHtCapabilitiesPresent(true);
        for (size_t i = 0; i < IEEE80211_HT_CAPABILITIES_ELEMENT_LENGTH; i++)
            frame->setHtCapabilities(i, 0);
        uint8_t htCapabilitiesInformation = htLdpcRxSupported ? 0x01 : 0x00;
        if (htSupports40Mhz)
            htCapabilitiesInformation |= 0x02;
        frame->setHtCapabilities(0, htCapabilitiesInformation);
        for (int mcs = 0; mcs < 80; mcs++)
            if (htMcsSupported[mcs])
                frame->setHtCapabilities(3 + mcs / 8,
                        frame->getHtCapabilities(3 + mcs / 8) | (1 << (mcs % 8)));
        addedBytes += 2 + IEEE80211_HT_CAPABILITIES_ELEMENT_LENGTH;
    }
    if (hasVht) {
        frame->setExtendedCapabilitiesPresent(true);
        frame->setExtendedCapabilitiesArraySize(IEEE80211_EXTENDED_CAPABILITIES_OPERATING_MODE_LENGTH);
        for (size_t i = 0; i < IEEE80211_EXTENDED_CAPABILITIES_OPERATING_MODE_LENGTH; i++)
            frame->setExtendedCapabilities(i, 0);
        frame->setExtendedCapabilities(7, 0x40); // bit 62: Operating Mode Notification
        addedBytes += 2 + IEEE80211_EXTENDED_CAPABILITIES_OPERATING_MODE_LENGTH;

        frame->setVhtCapabilitiesPresent(true);
        for (size_t i = 0; i < IEEE80211_VHT_CAPABILITIES_ELEMENT_LENGTH; i++)
            frame->setVhtCapabilities(i, 0);
        uint8_t vhtCapabilitiesInformation = vhtLdpcRxSupported ? 0x10 : 0x00;
        if (vhtSupports160Mhz)
            vhtCapabilitiesInformation |= 0x04; // Supported Channel Width Set = 1
        frame->setVhtCapabilities(0, vhtCapabilitiesInformation);
        uint16_t mcsMap = makeVhtMcsMap(modeSet, maximumSpatialStreams);
        frame->setVhtCapabilities(4, mcsMap & 0xFF);
        frame->setVhtCapabilities(5, mcsMap >> 8);
        frame->setVhtCapabilities(8, mcsMap & 0xFF);
        frame->setVhtCapabilities(9, mcsMap >> 8);
        addedBytes += 2 + IEEE80211_VHT_CAPABILITIES_ELEMENT_LENGTH;
    }
    frame->setChunkLength(frame->getChunkLength() + B(addedBytes));
}

} // namespace ieee80211
} // namespace inet
