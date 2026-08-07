//
// Copyright (C) 2012 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

#include <algorithm>

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Band.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ErpOfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211FhssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HrDsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211IrMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211EhtMode.h"

namespace inet {

namespace physicallayer {

Register_Abstract_Class(Ieee80211ModeSet);

bool Ieee80211ModeSet::isHtOrVhtMode(const IIeee80211Mode *mode)
{
    return dynamic_cast<const Ieee80211HtMode *>(mode) != nullptr ||
            dynamic_cast<const Ieee80211VhtMode *>(mode) != nullptr;
}

bool Ieee80211ModeSet::isHtProfileName(const char *profileName)
{
    return profileName != nullptr && (!strcmp(profileName, "n(mixed-2.4Ghz)") || !strcmp(profileName, "n(greenfield-2.4Ghz)"));
}

bool Ieee80211ModeSet::isPeerNegotiatedFecMode(const IIeee80211Mode *mode)
{
    return isHtOrVhtMode(mode) || dynamic_cast<const Ieee80211HeMode *>(mode) != nullptr ||
            dynamic_cast<const Ieee80211EhtMode *>(mode) != nullptr;
}

#define EHT_MODE_ENTRY(WIDTH, NSS, MCS, MANDATORY) \
        { MANDATORY, Ieee80211EhtCompliantModes::getCompliantMode(&Ieee80211EhtmcsTable::ehtMcs##MCS##BW##WIDTH##MHzNss##NSS, Ieee80211EhtMode::BAND_6GHZ, Ieee80211EhtPreambleMode::EHT_PREAMBLE_SU, Ieee80211EhtModeBase::EHT_GUARD_INTERVAL_LONG) },
#define EHT_MODE_ENTRIES_FOR_NSS(WIDTH, NSS) \
    EHT_MODE_ENTRY(WIDTH, NSS, 0, NSS == 1) \
    EHT_MODE_ENTRY(WIDTH, NSS, 1, false) \
    EHT_MODE_ENTRY(WIDTH, NSS, 2, false) \
    EHT_MODE_ENTRY(WIDTH, NSS, 3, false) \
    EHT_MODE_ENTRY(WIDTH, NSS, 4, false) \
    EHT_MODE_ENTRY(WIDTH, NSS, 5, false) \
    EHT_MODE_ENTRY(WIDTH, NSS, 6, false) \
    EHT_MODE_ENTRY(WIDTH, NSS, 7, false) \
    EHT_MODE_ENTRY(WIDTH, NSS, 8, false) \
    EHT_MODE_ENTRY(WIDTH, NSS, 9, false) \
    EHT_MODE_ENTRY(WIDTH, NSS, 10, false) \
    EHT_MODE_ENTRY(WIDTH, NSS, 11, false) \
    EHT_MODE_ENTRY(WIDTH, NSS, 12, false) \
    EHT_MODE_ENTRY(WIDTH, NSS, 13, false)
#define EHT_MODE_ENTRIES_FOR_BW(WIDTH) \
    EHT_MODE_ENTRIES_FOR_NSS(WIDTH, 1) \
    EHT_MODE_ENTRIES_FOR_NSS(WIDTH, 2) \
    EHT_MODE_ENTRIES_FOR_NSS(WIDTH, 3) \
    EHT_MODE_ENTRIES_FOR_NSS(WIDTH, 4) \
    EHT_MODE_ENTRIES_FOR_NSS(WIDTH, 5) \
    EHT_MODE_ENTRIES_FOR_NSS(WIDTH, 6) \
    EHT_MODE_ENTRIES_FOR_NSS(WIDTH, 7) \
    EHT_MODE_ENTRIES_FOR_NSS(WIDTH, 8)
#define EHT_SPECIAL_MODE_ENTRIES(WIDTH) \
    EHT_MODE_ENTRY(WIDTH, 1, 15, false)

#define HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, MCS, FORMAT) \
    { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs##MCS##BW##WIDTH##MHz, Ieee80211HtMode::BAND_2_4GHZ, FORMAT, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
#define HT_OPTIONAL_MODE_ENTRY(WIDTH, MCS) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, MCS, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED)
#define HT_UEQM_MODE_ENTRIES_FORMAT(WIDTH, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 33, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 34, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 35, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 36, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 37, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 38, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 39, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 40, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 41, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 42, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 43, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 44, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 45, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 46, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 47, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 48, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 49, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 50, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 51, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 52, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 53, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 54, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 55, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 56, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 57, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 58, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 59, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 60, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 61, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 62, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 63, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 64, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 65, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 66, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 67, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 68, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 69, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 70, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 71, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 72, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 73, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 74, FORMAT) \
    HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 75, FORMAT) HT_OPTIONAL_MODE_ENTRY_FORMAT(WIDTH, 76, FORMAT)
#define HT_UEQM_MODE_ENTRIES(WIDTH) \
    HT_UEQM_MODE_ENTRIES_FORMAT(WIDTH, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED)

namespace {

Ieee80211PhyFamily classifyPhyFamily(const IIeee80211Mode *mode)
{
    if (dynamic_cast<const Ieee80211HeMode *>(mode) != nullptr)
        return Ieee80211PhyFamily::HE;
    if (dynamic_cast<const Ieee80211EhtMode *>(mode) != nullptr)
        return Ieee80211PhyFamily::EHT;
    if (dynamic_cast<const Ieee80211VhtMode *>(mode) != nullptr)
        return Ieee80211PhyFamily::VHT;
    if (dynamic_cast<const Ieee80211HtMode *>(mode) != nullptr)
        return Ieee80211PhyFamily::HT;
    if (dynamic_cast<const Ieee80211ErpOfdmMode *>(mode) != nullptr)
        return Ieee80211PhyFamily::ERP_OFDM;
    if (dynamic_cast<const Ieee80211OfdmMode *>(mode) != nullptr)
        return Ieee80211PhyFamily::OFDM;
    if (dynamic_cast<const Ieee80211DsssMode *>(mode) != nullptr ||
            dynamic_cast<const Ieee80211HrDsssMode *>(mode) != nullptr)
        return Ieee80211PhyFamily::DSSS;
    return Ieee80211PhyFamily::UNSPECIFIED;
}

} // namespace

std::vector<Ieee80211ModeSet::Entry> Ieee80211ModeSet::completeGuardIntervalVariants(
        const char *profileName, const std::vector<Entry>& entries)
{
    std::vector<Entry> completeEntries = entries;
    if (isHtProfileName(profileName)) {
        for (const auto& entry : entries) {
            auto mode = dynamic_cast<const Ieee80211HtMode *>(entry.mode);
            if (mode == nullptr)
                continue;
            auto dataMode = mode->getDataMode();
            auto guardIntervalType = dataMode->getGuardIntervalType() == Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG ?
                    Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT : Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG;
            completeEntries.push_back({entry.isMandatory, Ieee80211HtCompliantModes::getCompliantMode(
                    dataMode->getModulationAndCodingScheme(), mode->getCenterFrequencyMode(),
                    mode->getPreambleMode()->getPreambleFormat(), guardIntervalType),
                    entry.phyFamily, entry.supportRequirement});
        }
    }
    else if (!strcmp(profileName, "ac")) {
        for (const auto& entry : entries) {
            auto mode = dynamic_cast<const Ieee80211VhtMode *>(entry.mode);
            if (mode == nullptr)
                continue;
            auto dataMode = mode->getDataMode();
            auto guardIntervalType = dataMode->getGuardIntervalType() == Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG ?
                    Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT : Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG;
            completeEntries.push_back({entry.isMandatory, Ieee80211VhtCompliantModes::getCompliantMode(
                    dataMode->getModulationAndCodingScheme(), mode->getCenterFrequencyMode(),
                    mode->getPreambleMode()->getPreambleFormat(), guardIntervalType),
                    entry.phyFamily, entry.supportRequirement});
        }
    }
    else if (!strcmp(profileName, "ax-catalog")) {
        for (const auto& entry : entries) {
            auto mode = dynamic_cast<const Ieee80211HeMode *>(entry.mode);
            if (mode == nullptr)
                continue;
            auto dataMode = mode->getDataMode();
            for (auto guardIntervalType : {Ieee80211HeModeBase::HE_GUARD_INTERVAL_SHORT,
                                           Ieee80211HeModeBase::HE_GUARD_INTERVAL_MEDIUM,
                                           Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG}) {
                if (guardIntervalType == dataMode->getGuardIntervalType())
                    continue;
                completeEntries.push_back({entry.isMandatory, Ieee80211HeCompliantModes::getCompliantMode(
                        dataMode->getModulationAndCodingScheme(), mode->getCenterFrequencyMode(),
                        mode->getPreambleMode()->getPreambleFormat(), guardIntervalType, dataMode->isLdpc()),
                        entry.phyFamily, entry.supportRequirement});
            }
        }
    }
    for (auto& entry : completeEntries)
        if (entry.phyFamily == Ieee80211PhyFamily::UNSPECIFIED)
            entry.phyFamily = classifyPhyFamily(entry.mode);
    return completeEntries;
}

Ieee80211ModeSet Ieee80211ModeSet::createHeProfile(const char *profileName,
        Ieee80211OperatingBand operatingBand, const std::vector<Ieee80211ModeSet>& baseModeSets)
{
    std::vector<Entry> entries;
    auto appendCompatibilitySet = [&](const char *baseName, bool preserveBasicRates) {
        for (const auto& modeSet : baseModeSets) {
            if (modeSet.profileName != baseName)
                continue;
            for (const auto& source : modeSet.entries) {
                auto family = classifyPhyFamily(source.mode);
                auto requirement = Ieee80211SupportRequirement::COMPATIBILITY;
                if (family == Ieee80211PhyFamily::HT) {
                    auto dataMode = check_and_cast<const Ieee80211HtMode *>(source.mode)->getDataMode();
                    requirement = dataMode->getNumberOfSpatialStreams() == 1 && dataMode->getMcsIndex() <= 7 && dataMode->getBandwidth() == MHz(20) ?
                            Ieee80211SupportRequirement::MANDATORY : Ieee80211SupportRequirement::OPTIONAL;
                }
                else if (family == Ieee80211PhyFamily::VHT) {
                    auto dataMode = check_and_cast<const Ieee80211VhtMode *>(source.mode)->getDataMode();
                    requirement = dataMode->getNumberOfSpatialStreams() == 1 && dataMode->getMcsIndex() <= 7 ?
                            Ieee80211SupportRequirement::CONDITIONALLY_MANDATORY : Ieee80211SupportRequirement::OPTIONAL;
                }
                entries.push_back({preserveBasicRates && source.isMandatory,
                        source.mode, family, requirement});
            }
            return;
        }
        throw cRuntimeError("Missing compatibility mode set '%s' while constructing '%s'", baseName, profileName);
    };

    // IEEE Std 802.11-2024 Clause 27.1.1: admit only the earlier PHYs that
    // apply to the selected operating band. Their legacy/basic-rate bits stay
    // separate from the HE mandatory/optional support classification.
    if (operatingBand == Ieee80211OperatingBand::BAND_2_4_GHZ) {
        appendCompatibilitySet("g(mixed)", true);
        appendCompatibilitySet("n(mixed-2.4Ghz)", false);
    }
    else if (operatingBand == Ieee80211OperatingBand::BAND_5_GHZ) {
        appendCompatibilitySet("a", true);
        appendCompatibilitySet("ac", false);
        // Rebind the existing 2.4 GHz HT catalog to its 5 GHz timing mode;
        // the repository does not otherwise expose an n(mixed-5GHz) set.
        for (const auto& modeSet : baseModeSets) {
            if (modeSet.profileName != "n(mixed-2.4Ghz)")
                continue;
            for (const auto& source : modeSet.entries) {
                auto sourceMode = dynamic_cast<const Ieee80211HtMode *>(source.mode);
                if (sourceMode == nullptr)
                    continue;
                auto dataMode = sourceMode->getDataMode();
                auto mode = Ieee80211HtCompliantModes::getCompliantMode(
                        dataMode->getModulationAndCodingScheme(), Ieee80211HtMode::BAND_5GHZ,
                        sourceMode->getPreambleMode()->getPreambleFormat(), dataMode->getGuardIntervalType(),
                        dataMode->getCode() != nullptr && dataMode->getCode()->isLdpc());
                bool baseline = dataMode->getNumberOfSpatialStreams() == 1 && dataMode->getMcsIndex() <= 7 && dataMode->getBandwidth() == MHz(20);
                entries.push_back({false, mode, Ieee80211PhyFamily::HT,
                        baseline ? Ieee80211SupportRequirement::CONDITIONALLY_MANDATORY : Ieee80211SupportRequirement::OPTIONAL});
            }
            break;
        }
    }
    else if (operatingBand == Ieee80211OperatingBand::BAND_6_GHZ) {
        // Clause 27.1.1 bases 6 GHz HE operation directly on the Clause 17
        // OFDM PHY. Preserve its mandatory 6, 12, and 24 Mb/s basic rates.
        appendCompatibilitySet("a", true);
    }

    const Ieee80211ModeSet *catalog = nullptr;
    for (const auto& modeSet : baseModeSets)
        if (modeSet.profileName == "ax-catalog") {
            catalog = &modeSet;
            break;
        }
    if (catalog == nullptr)
        throw cRuntimeError("Missing HE mode catalog");

    Ieee80211HeMode::BandMode bandMode = Ieee80211HeMode::BAND_5GHZ;
    if (operatingBand == Ieee80211OperatingBand::BAND_2_4_GHZ)
        bandMode = Ieee80211HeMode::BAND_2_4GHZ;
    else if (operatingBand == Ieee80211OperatingBand::BAND_6_GHZ)
        bandMode = Ieee80211HeMode::BAND_6GHZ;

    for (const auto& source : catalog->entries) {
        auto sourceMode = check_and_cast<const Ieee80211HeMode *>(source.mode);
        auto dataMode = sourceMode->getDataMode();
        auto bandwidth = dataMode->getBandwidth();
        if (operatingBand == Ieee80211OperatingBand::BAND_2_4_GHZ && bandwidth > MHz(40))
            continue;
        auto mcs = dataMode->getMcsIndex();
        auto nss = dataMode->getNumberOfSpatialStreams();
        // Clause 27.1.1: BCC is not used for an HE SU PPDU wider than
        // 20 MHz or for HE-MCS 10/11.
        bool requiresLdpc = bandwidth > MHz(20) || mcs >= 10;
        auto mode = Ieee80211HeCompliantModes::getCompliantMode(
                dataMode->getModulationAndCodingScheme(), bandMode,
                Ieee80211HePreambleMode::HE_PREAMBLE_SU,
            dataMode->getGuardIntervalType(), requiresLdpc);
        bool mandatoryHeMode = nss == 1 && mcs <= 7;
        entries.push_back({false, mode, Ieee80211PhyFamily::HE,
                mandatoryHeMode ? Ieee80211SupportRequirement::MANDATORY : Ieee80211SupportRequirement::OPTIONAL});
    }
    return Ieee80211ModeSet(profileName, "ax", operatingBand, entries);
}

static Ieee80211ModeSet createHtModeSet(const char *profileName, Ieee80211HtPreambleMode::HighTroughputPreambleFormat preambleMode)
{
    return Ieee80211ModeSet(profileName, {
        { true, &Ieee80211DsssCompliantModes::dsssMode1Mbps },
        { true, &Ieee80211DsssCompliantModes::dsssMode2Mbps },
        { true, &Ieee80211HrDsssCompliantModes::hrDsssMode5_5MbpsCckLongPreamble },
        { true, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode6Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode9Mbps },
        { true, &Ieee80211HrDsssCompliantModes::hrDsssMode11MbpsCckLongPreamble },
        { true, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode12Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode18Mbps },
        { true, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode24Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode36Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode48Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode54Mbps },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs0BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs1BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs2BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs3BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs4BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs5BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs6BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs7BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs8BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs9BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs10BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs11BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs12BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs13BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs14BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs15BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs16BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs17BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs18BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs19BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs20BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs21BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs22BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs23BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs24BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs25BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs26BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs27BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs28BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs29BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs30BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs31BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        HT_UEQM_MODE_ENTRIES_FORMAT(20, preambleMode)
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs0BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs1BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs2BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs3BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs4BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs5BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs6BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs7BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs8BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs9BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs10BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs11BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs12BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs13BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs14BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs15BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs16BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs17BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs18BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs19BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs20BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs21BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs22BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs23BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs24BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs25BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs26BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs27BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs28BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs29BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs30BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs31BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, preambleMode, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        HT_OPTIONAL_MODE_ENTRY_FORMAT(40, 32, preambleMode)
        HT_UEQM_MODE_ENTRIES_FORMAT(40, preambleMode)
    });
}

const DelayedInitializer<std::vector<Ieee80211ModeSet>> Ieee80211ModeSet::modeSets([]() {
    auto result = new std::vector<Ieee80211ModeSet> {
    Ieee80211ModeSet("a", {
        { true, &Ieee80211OfdmCompliantModes::ofdmMode6MbpsCS20MHz },
        { false, &Ieee80211OfdmCompliantModes::ofdmMode9MbpsCS20MHz },
        { true, &Ieee80211OfdmCompliantModes::ofdmMode12MbpsCS20MHz },
        { false, &Ieee80211OfdmCompliantModes::ofdmMode18MbpsCS20MHz },
        { true, &Ieee80211OfdmCompliantModes::ofdmMode24MbpsCS20MHz },
        { false, &Ieee80211OfdmCompliantModes::ofdmMode36Mbps },
        { false, &Ieee80211OfdmCompliantModes::ofdmMode48Mbps },
        { false, &Ieee80211OfdmCompliantModes::ofdmMode54Mbps },
    }),
    Ieee80211ModeSet("b", {
        { true, &Ieee80211DsssCompliantModes::dsssMode1Mbps },
        { true, &Ieee80211DsssCompliantModes::dsssMode2Mbps },
        { true, &Ieee80211HrDsssCompliantModes::hrDsssMode5_5MbpsCckLongPreamble },
        { true, &Ieee80211HrDsssCompliantModes::hrDsssMode11MbpsCckLongPreamble },
    }),
    // TODO slotTime, cwMin, cwMax must be identical in all modes
    Ieee80211ModeSet("g(mixed)", {
        { true, &Ieee80211DsssCompliantModes::dsssMode1Mbps },
        { true, &Ieee80211DsssCompliantModes::dsssMode2Mbps },
        { true, &Ieee80211HrDsssCompliantModes::hrDsssMode5_5MbpsCckLongPreamble },
        { true, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode6Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode9Mbps },
        { true, &Ieee80211HrDsssCompliantModes::hrDsssMode11MbpsCckLongPreamble },
        { true, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode12Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode18Mbps },
        { true, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode24Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode36Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode48Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOfdmMode54Mbps }, // TODO ERP-CCK, ERP-PBCC, DSSS-OFDM
    }),
    Ieee80211ModeSet("g(erp)", {
        { true, &Ieee80211ErpOfdmCompliantModes::erpOnlyOfdmMode6Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOnlyOfdmMode9Mbps },
        { true, &Ieee80211ErpOfdmCompliantModes::erpOnlyOfdmMode12Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOnlyOfdmMode18Mbps },
        { true, &Ieee80211ErpOfdmCompliantModes::erpOnlyOfdmMode24Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOnlyOfdmMode36Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOnlyOfdmMode48Mbps },
        { false, &Ieee80211ErpOfdmCompliantModes::erpOnlyOfdmMode54Mbps },
    }),
    Ieee80211ModeSet("p", {
        { true, &Ieee80211OfdmCompliantModes::ofdmMode3MbpsCS10MHz },
        { false, &Ieee80211OfdmCompliantModes::ofdmMode4_5MbpsCS10MHz },
        { true, &Ieee80211OfdmCompliantModes::ofdmMode6MbpsCS10MHz },
        { false, &Ieee80211OfdmCompliantModes::ofdmMode9MbpsCS10MHz },
        { true, &Ieee80211OfdmCompliantModes::ofdmMode12MbpsCS10MHz },
        { false, &Ieee80211OfdmCompliantModes::ofdmMode18MbpsCS10MHz },
        { false, &Ieee80211OfdmCompliantModes::ofdmMode24MbpsCS10MHz },
        { false, &Ieee80211OfdmCompliantModes::ofdmMode27Mbps },
    }),
    createHtModeSet("n(mixed-2.4Ghz)", Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED),
    createHtModeSet("n(greenfield-2.4Ghz)", Ieee80211HtPreambleMode::HT_PREAMBLE_GREENFIELD),
    Ieee80211ModeSet("ac", {
        { true, &Ieee80211OfdmCompliantModes::ofdmMode6MbpsCS20MHz },
        { true, &Ieee80211OfdmCompliantModes::ofdmMode12MbpsCS20MHz },
        { true, &Ieee80211OfdmCompliantModes::ofdmMode24MbpsCS20MHz },
        { true, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW20MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW20MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW20MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW20MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW20MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW20MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW20MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW20MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW20MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW20MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW20MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW20MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW20MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW20MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW20MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW20MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW20MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW20MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW20MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW20MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW20MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW20MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW20MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW20MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW20MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW20MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW20MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW20MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW20MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW20MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW20MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW20MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW20MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW20MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW20MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW20MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW20MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW20MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW20MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW20MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW20MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW20MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW20MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW20MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW20MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW20MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW20MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW20MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW20MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW20MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW20MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW20MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW20MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW20MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW20MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW20MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW20MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW20MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW20MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW20MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW20MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW20MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW20MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW20MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW20MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW20MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW20MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW20MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW20MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW20MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW20MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW20MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW20MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW20MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW40MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW40MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW40MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW40MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW40MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW40MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW40MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW40MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW40MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW40MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW40MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW40MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW40MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW40MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW40MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW40MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW40MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW40MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW40MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW40MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW40MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW40MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW40MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW40MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW40MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW40MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW40MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW40MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW40MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW40MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW40MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW40MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW40MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW40MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW40MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW40MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW40MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW40MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW40MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW40MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW40MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW40MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW40MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW40MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW40MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW40MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW40MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW40MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW40MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW40MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW40MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW40MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW40MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW40MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW40MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW40MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW40MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW40MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW40MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW40MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW40MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW40MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW40MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW40MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW40MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW40MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW40MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW40MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW40MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW40MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW40MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW40MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW40MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW40MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW40MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW40MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW40MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW40MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW40MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW40MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW80MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW80MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW80MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW80MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW80MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW80MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW80MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW80MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW80MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW80MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW80MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW80MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW80MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW80MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW80MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW80MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW80MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW80MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW80MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW80MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW80MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW80MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW80MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW80MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW80MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW80MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW80MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW80MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW80MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW80MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW80MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW80MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW80MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW80MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW80MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW80MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW80MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW80MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW80MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW80MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW80MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW80MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW80MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW80MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW80MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW80MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW80MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW80MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW80MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW80MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW80MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW80MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW80MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW80MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW80MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW80MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW80MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW80MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW80MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW80MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW80MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW80MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW80MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW80MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW80MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW80MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW80MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW80MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW80MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW80MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW80MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW80MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW80MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW80MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW80MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW80MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW80MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW160MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW160MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW160MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW160MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW160MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW160MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW160MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW160MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW160MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW160MHzNss1, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW160MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW160MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW160MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW160MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW160MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW160MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW160MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW160MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW160MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW160MHzNss2, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW160MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW160MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW160MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW160MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW160MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW160MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW160MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW160MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW160MHzNss3, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW160MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW160MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW160MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW160MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW160MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW160MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW160MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW160MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW160MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW160MHzNss4, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW160MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW160MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW160MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW160MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW160MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW160MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW160MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW160MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW160MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW160MHzNss5, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW160MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW160MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW160MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW160MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW160MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW160MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW160MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW160MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW160MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW160MHzNss6, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW160MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW160MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW160MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW160MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW160MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW160MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW160MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW160MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW160MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW160MHzNss7, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs0BW160MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs1BW160MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs2BW160MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs3BW160MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs4BW160MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs5BW160MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs6BW160MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs7BW160MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs8BW160MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211VhtCompliantModes::getCompliantMode(&Ieee80211VhtmcsTable::vhtMcs9BW160MHzNss8, Ieee80211VhtMode::BAND_5GHZ, Ieee80211VhtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT) },
    }),
        Ieee80211ModeSet("ax-catalog", "ax", Ieee80211OperatingBand::BAND_5_GHZ, {
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW20MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW20MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW20MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW20MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW20MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW20MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW20MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW20MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW20MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW20MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW20MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW20MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW20MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW20MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW20MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW20MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW20MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW20MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW20MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW20MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW20MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW20MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW20MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW20MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW20MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW20MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW20MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW20MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW20MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW20MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW20MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW20MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW20MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW20MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW20MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW20MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW20MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW20MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW20MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW20MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW20MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW20MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW20MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW20MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW20MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW20MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW20MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW20MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW20MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW20MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW20MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW20MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW20MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW20MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW20MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW20MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW20MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW20MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW20MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW20MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW20MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW20MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW20MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW20MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW20MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW20MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW20MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW20MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW20MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW20MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW20MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW20MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW20MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW20MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW20MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW20MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW20MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW20MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW20MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW20MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW20MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW20MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW20MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW20MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW20MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW20MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW20MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW20MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW20MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW20MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW20MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW20MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW20MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW20MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW20MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW20MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW40MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW40MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW40MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW40MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW40MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW40MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW40MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW40MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW40MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW40MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW40MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW40MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW40MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW40MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW40MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW40MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW40MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW40MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW40MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW40MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW40MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW40MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW40MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW40MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW40MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW40MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW40MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW40MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW40MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW40MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW40MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW40MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW40MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW40MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW40MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW40MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW40MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW40MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW40MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW40MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW40MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW40MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW40MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW40MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW40MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW40MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW40MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW40MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW40MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW40MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW40MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW40MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW40MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW40MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW40MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW40MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW40MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW40MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW40MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW40MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW40MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW40MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW40MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW40MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW40MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW40MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW40MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW40MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW40MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW40MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW40MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW40MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW40MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW40MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW40MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW40MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW40MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW40MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW40MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW40MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW40MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW40MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW40MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW40MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW40MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW40MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW40MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW40MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW40MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW40MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW40MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW40MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW40MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW40MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW40MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW40MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW80MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW80MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW80MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW80MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW80MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW80MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW80MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW80MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW80MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW80MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW80MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW80MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW80MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW80MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW80MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW80MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW80MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW80MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW80MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW80MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW80MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW80MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW80MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW80MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW80MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW80MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW80MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW80MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW80MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW80MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW80MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW80MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW80MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW80MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW80MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW80MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW80MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW80MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW80MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW80MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW80MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW80MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW80MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW80MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW80MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW80MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW80MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW80MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW80MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW80MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW80MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW80MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW80MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW80MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW80MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW80MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW80MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW80MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW80MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW80MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW80MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW80MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW80MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW80MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW80MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW80MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW80MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW80MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW80MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW80MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW80MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW80MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW80MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW80MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW80MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW80MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW80MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW80MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW80MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW80MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW80MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW80MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW80MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW80MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW80MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW80MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW80MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW80MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW80MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW80MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW80MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW80MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW80MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW80MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW80MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW80MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW160MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW160MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW160MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW160MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW160MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW160MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW160MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW160MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW160MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW160MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW160MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW160MHzNss1, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW160MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW160MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW160MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW160MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW160MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW160MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW160MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW160MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW160MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW160MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW160MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW160MHzNss2, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW160MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW160MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW160MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW160MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW160MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW160MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW160MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW160MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW160MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW160MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW160MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW160MHzNss3, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW160MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW160MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW160MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW160MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW160MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW160MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW160MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW160MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW160MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW160MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW160MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW160MHzNss4, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW160MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW160MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW160MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW160MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW160MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW160MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW160MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW160MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW160MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW160MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW160MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW160MHzNss5, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW160MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW160MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW160MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW160MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW160MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW160MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW160MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW160MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW160MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW160MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW160MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW160MHzNss6, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW160MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW160MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW160MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW160MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW160MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW160MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW160MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW160MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW160MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW160MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW160MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW160MHzNss7, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs0BW160MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs1BW160MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs2BW160MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs3BW160MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs4BW160MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs5BW160MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs6BW160MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs7BW160MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs8BW160MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs9BW160MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs10BW160MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HeCompliantModes::getCompliantMode(&Ieee80211HemcsTable::heMcs11BW160MHzNss8, Ieee80211HeMode::BAND_5GHZ, Ieee80211HePreambleMode::HE_PREAMBLE_SU, Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG) }
    }),
    Ieee80211ModeSet("be", {
        EHT_MODE_ENTRIES_FOR_BW(20)
        EHT_SPECIAL_MODE_ENTRIES(20)
        EHT_MODE_ENTRIES_FOR_BW(40)
        EHT_SPECIAL_MODE_ENTRIES(40)
        EHT_MODE_ENTRIES_FOR_BW(80)
        EHT_SPECIAL_MODE_ENTRIES(80)
        EHT_MODE_ENTRY(80, 1, 14, false)
        EHT_MODE_ENTRIES_FOR_BW(160)
        EHT_SPECIAL_MODE_ENTRIES(160)
        EHT_MODE_ENTRY(160, 1, 14, false)
        EHT_MODE_ENTRIES_FOR_BW(320)
        EHT_SPECIAL_MODE_ENTRIES(320)
        EHT_MODE_ENTRY(320, 1, 14, false)
    })
    };
    result->push_back(createHeProfile("ax-2.4GHz", Ieee80211OperatingBand::BAND_2_4_GHZ, *result));
    result->push_back(createHeProfile("ax-5GHz", Ieee80211OperatingBand::BAND_5_GHZ, *result));
    result->push_back(createHeProfile("ax-6GHz", Ieee80211OperatingBand::BAND_6_GHZ, *result));
    return result;
});

#undef EHT_MODE_ENTRIES_FOR_BW
#undef EHT_SPECIAL_MODE_ENTRIES
#undef EHT_MODE_ENTRIES_FOR_NSS
#undef EHT_MODE_ENTRY
#undef HT_UEQM_MODE_ENTRIES
#undef HT_OPTIONAL_MODE_ENTRY

Ieee80211ModeSet::Ieee80211ModeSet(const char *name, const std::vector<Entry> entries) :
    name(name),
    profileName(name),
    entries(completeGuardIntervalVariants(name, entries))
{
    std::vector<Entry> *nonConstEntries = const_cast<std::vector<Entry> *>(&this->entries);
    std::stable_sort(nonConstEntries->begin(), nonConstEntries->end(), EntryNetBitrateComparator());
    auto referenceMode = entries[0].mode;
    for (auto entry : entries) {
        auto mode = entry.mode;
        if (mode->getSifsTime() != referenceMode->getSifsTime() ||
            mode->getSlotTime() != referenceMode->getSlotTime() ||
            mode->getPhyRxStartDelay() != referenceMode->getPhyRxStartDelay())
        {
            // FIXME throw cRuntimeError("Sifs, slot and phyRxStartDelay time must be identical within a ModeSet");
        }
    }
    if (!strcmp(name, "be"))
        supportedChannelWidths = IEEE80211_WIDTH_20 | IEEE80211_WIDTH_40 | IEEE80211_WIDTH_80 |
                IEEE80211_WIDTH_160 | IEEE80211_WIDTH_320;
}

Ieee80211ModeSet::Ieee80211ModeSet(const char *profileName, const char *name,
        Ieee80211OperatingBand operatingBand, const std::vector<Entry> entries) :
    name(name),
    profileName(profileName),
    entries(completeGuardIntervalVariants(profileName, entries)),
    operatingBand(operatingBand)
{
    std::vector<Entry> *nonConstEntries = const_cast<std::vector<Entry> *>(&this->entries);
    std::stable_sort(nonConstEntries->begin(), nonConstEntries->end(), EntryNetBitrateComparator());
    if (entries.empty())
        throw cRuntimeError("Empty 802.11 mode profile '%s'", profileName);
    supportedChannelWidths = IEEE80211_WIDTH_20 | IEEE80211_WIDTH_40;
    if (operatingBand != Ieee80211OperatingBand::BAND_2_4_GHZ)
        supportedChannelWidths |= IEEE80211_WIDTH_80 | IEEE80211_WIDTH_160;
    channelWidthScopedBasicRates = false;
    bandAware = true;
}

namespace {

using namespace inet::physicallayer;

// The LDPC and BCC variants of a given HT/VHT/HE mode are represented by
// distinct mode objects (they differ in airtime due to the BCC tail bits),
// but the mode set only stores the BCC variant. Treat the LDPC variant as
// equivalent so that receivers accept LDPC transmissions.

bool isSameHtModeIgnoringLdpc(const IIeee80211Mode *a, const IIeee80211Mode *b)
{
    auto htA = dynamic_cast<const Ieee80211HtMode *>(a);
    auto htB = dynamic_cast<const Ieee80211HtMode *>(b);
    if (htA == nullptr || htB == nullptr)
        return false;
    auto dataA = htA->getDataMode();
    auto dataB = htB->getDataMode();
    return dataA->getMcsIndex() == dataB->getMcsIndex() &&
           dataA->getNumberOfSpatialStreams() == dataB->getNumberOfSpatialStreams() &&
           dataA->getBandwidth() == dataB->getBandwidth() &&
           dataA->getGuardIntervalType() == dataB->getGuardIntervalType() &&
           htA->getPreambleMode()->getPreambleFormat() == htB->getPreambleMode()->getPreambleFormat() &&
           htA->getCenterFrequencyMode() == htB->getCenterFrequencyMode();
}

bool isSameVhtModeIgnoringLdpc(const IIeee80211Mode *a, const IIeee80211Mode *b)
{
    auto vhtA = dynamic_cast<const Ieee80211VhtMode *>(a);
    auto vhtB = dynamic_cast<const Ieee80211VhtMode *>(b);
    if (vhtA == nullptr || vhtB == nullptr)
        return false;
    auto dataA = vhtA->getDataMode();
    auto dataB = vhtB->getDataMode();
    return dataA->getMcsIndex() == dataB->getMcsIndex() &&
           dataA->getNumberOfSpatialStreams() == dataB->getNumberOfSpatialStreams() &&
           dataA->getBandwidth() == dataB->getBandwidth() &&
           dataA->getGuardIntervalType() == dataB->getGuardIntervalType() &&
           vhtA->getPreambleMode()->getPreambleFormat() == vhtB->getPreambleMode()->getPreambleFormat() &&
           vhtA->getCenterFrequencyMode() == vhtB->getCenterFrequencyMode();
}

bool isSameHeModeIgnoringLdpc(const IIeee80211Mode *a, const IIeee80211Mode *b)
{
    auto heA = dynamic_cast<const Ieee80211HeMode *>(a);
    auto heB = dynamic_cast<const Ieee80211HeMode *>(b);
    if (heA == nullptr || heB == nullptr)
        return false;
    auto dataA = heA->getDataMode();
    auto dataB = heB->getDataMode();
    auto preambleA = heA->getPreambleMode()->getPreambleFormat();
    auto preambleB = heB->getPreambleMode()->getPreambleFormat();
    bool compatibleSuPreamble = preambleA == preambleB ||
            (preambleA == Ieee80211HePreambleMode::HE_PREAMBLE_SU &&
             preambleB == Ieee80211HePreambleMode::HE_PREAMBLE_ER_SU) ||
            (preambleA == Ieee80211HePreambleMode::HE_PREAMBLE_ER_SU &&
             preambleB == Ieee80211HePreambleMode::HE_PREAMBLE_SU);
    bool codingCanDiffer = dataA->getBandwidth() == MHz(20) &&
            dataA->getMcsIndex() <= 9 && dataA->getNumberOfSpatialStreams() <= 4;
    return dataA->getMcsIndex() == dataB->getMcsIndex() &&
           dataA->getNumberOfSpatialStreams() == dataB->getNumberOfSpatialStreams() &&
           dataA->getBandwidth() == dataB->getBandwidth() &&
           dataA->getGuardIntervalType() == dataB->getGuardIntervalType() &&
           compatibleSuPreamble &&
           heA->getCenterFrequencyMode() == heB->getCenterFrequencyMode() &&
           (dataA->isLdpc() == dataB->isLdpc() || codingCanDiffer);
}

bool isSameEhtMode(const IIeee80211Mode *a, const IIeee80211Mode *b)
{
    auto ehtA = dynamic_cast<const Ieee80211EhtMode *>(a);
    auto ehtB = dynamic_cast<const Ieee80211EhtMode *>(b);
    if (ehtA == nullptr || ehtB == nullptr)
        return false;
    auto dataA = ehtA->getDataMode();
    auto dataB = ehtB->getDataMode();
    return dataA->getMcsIndex() == dataB->getMcsIndex() &&
           dataA->getNumberOfSpatialStreams() == dataB->getNumberOfSpatialStreams() &&
           dataA->getBandwidth() == dataB->getBandwidth() &&
           dataA->getGuardIntervalType() == dataB->getGuardIntervalType() &&
           ehtA->getPreambleMode()->getPreambleFormat() == ehtB->getPreambleMode()->getPreambleFormat() &&
           ehtA->getCenterFrequencyMode() == ehtB->getCenterFrequencyMode();
}

} // namespace

int Ieee80211ModeSet::findModeIndex(const IIeee80211Mode *mode) const
{
    for (size_t index = 0; index < entries.size(); index++)
        if (entries[index].mode == mode)
            return index;
    // Where both coding schemes are legal, accept the corresponding LDPC/BCC
    // object as the same supported modulation profile.
    for (size_t index = 0; index < entries.size(); index++)
        if (isSameHtModeIgnoringLdpc(entries[index].mode, mode) ||
            isSameVhtModeIgnoringLdpc(entries[index].mode, mode) ||
            isSameHeModeIgnoringLdpc(entries[index].mode, mode) ||
            isSameEhtMode(entries[index].mode, mode))
            return index;
    return -1;
}

int Ieee80211ModeSet::getModeIndex(const IIeee80211Mode *mode) const
{
    int index = findModeIndex(mode);
    if (index < 0)
        throw cRuntimeError("Unknown mode");
    else
        return index;
}

bool Ieee80211ModeSet::getIsMandatory(const IIeee80211Mode *mode) const
{
    return entries[getModeIndex(mode)].isMandatory;
}

const IIeee80211Mode *Ieee80211ModeSet::findHeMode(int mcs, int numSpatialStreams, Hz bandwidth, bool ldpc) const
{
    for (const auto& entry : entries) {
        auto heMode = dynamic_cast<const Ieee80211HeMode *>(entry.mode);
        if (heMode == nullptr)
            continue;
        auto dataMode = heMode->getDataMode();
        if ((int)dataMode->getMcsIndex() == mcs &&
                dataMode->getNumberOfSpatialStreams() == numSpatialStreams &&
                dataMode->getBandwidth() == bandwidth &&
                (dataMode->getCode() != nullptr && dataMode->getCode()->isLdpc()) == ldpc)
            return heMode;
    }
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::findVhtMode(int mcs,
        int numSpatialStreams, Hz bandwidth, bool ldpc) const
{
    for (const auto& entry : entries) {
        auto vhtMode = dynamic_cast<const Ieee80211VhtMode *>(entry.mode);
        if (vhtMode == nullptr)
            continue;
        auto dataMode = vhtMode->getDataMode();
        if ((int)dataMode->getMcsIndex() == mcs &&
                dataMode->getNumberOfSpatialStreams() == numSpatialStreams &&
                dataMode->getBandwidth() == bandwidth &&
                (dataMode->getCode() != nullptr && dataMode->getCode()->isLdpc()) == ldpc)
            return vhtMode;
    }
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getVhtSuNdpMode(
        const IIeee80211Mode *referenceMode, int numberOfSpaceTimeStreams) const
{
    if (referenceMode == nullptr || !containsMode(referenceMode) ||
            getPhyFamily(referenceMode) != Ieee80211PhyFamily::VHT)
        throw cRuntimeError("VHT SU NDP mode derivation requires a VHT reference mode from this mode set");
    if (referenceMode->getDataMode()->getBandwidth() != MHz(20))
        throw cRuntimeError("Packet-level VHT SU NDP mode derivation currently supports only 20 MHz");
    if (numberOfSpaceTimeStreams != 2)
        throw cRuntimeError("Packet-level VHT SU NDP mode derivation currently requires two space-time streams");
    auto vhtMode = check_and_cast<const Ieee80211VhtMode *>(referenceMode);
    return Ieee80211VhtCompliantModes::getCompliantMode(
            &Ieee80211VhtmcsTable::vhtMcs0BW20MHzNss2,
            vhtMode->getCenterFrequencyMode(),
            vhtMode->getPreambleMode()->getPreambleFormat(),
            Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG);
}

const IIeee80211Mode *Ieee80211ModeSet::findMode(bps bitrate, Hz bandwidth, int numSpatialStreams) const
{
    return findMode(bitrate - Mbps(0.05), bitrate + Mbps(0.05), bandwidth, numSpatialStreams);
}

const IIeee80211Mode *Ieee80211ModeSet::findMode(bps minBitrate, bps maxBitrate, Hz bandwidth, int numSpatialStreams) const
{
    const IIeee80211Mode *bestMode = nullptr;
    for (size_t index = 0; index < entries.size(); index++) {
        auto mode = entries[index].mode;
        auto dataMode = mode->getDataMode();
        auto bitrate = dataMode->getNetBitrate();
        if (minBitrate <= bitrate && bitrate <= maxBitrate &&
            (std::isnan(bandwidth.get()) || dataMode->getBandwidth() == bandwidth) &&
            (numSpatialStreams == -1 || dataMode->getNumberOfSpatialStreams() == numSpatialStreams))
        {
            if (bestMode == nullptr || dataMode->getNumberOfSpatialStreams() < bestMode->getDataMode()->getNumberOfSpatialStreams())
                bestMode = mode;
        }
    }
    return bestMode;
}

const IIeee80211Mode *Ieee80211ModeSet::getMode(bps bitrate, Hz bandwidth, int numSpatialStreams) const
{
    const IIeee80211Mode *mode = getMode(bitrate - Mbps(0.05), bitrate + Mbps(0.05), bandwidth, numSpatialStreams);
    if (mode == nullptr)
        throw cRuntimeError("Unknown bitrate: %g in operation mode: '%s'", bitrate.get(), getName());
    else
        return mode;
}

const IIeee80211Mode *Ieee80211ModeSet::getMode(bps minBitrate, bps maxBitrate, Hz bandwidth, int numSpatialStreams) const
{
    const IIeee80211Mode *mode = findMode(minBitrate, maxBitrate, bandwidth, numSpatialStreams);
    if (mode == nullptr)
        throw cRuntimeError("Unknown bitrate: (%g - %g) in operation mode: '%s'", minBitrate.get(), maxBitrate.get(), getName());
    else
        return mode;
}

const IIeee80211Mode *Ieee80211ModeSet::getSlowestMode(Hz bandwidth) const
{
    if (std::isnan(bandwidth.get()))
        return entries.front().mode;
    for (size_t i = 0; i < entries.size(); i++)
        if (entries[i].mode->getDataMode()->getBandwidth() == bandwidth)
            return entries[i].mode;
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getFastestMode(Hz bandwidth) const
{
    if (std::isnan(bandwidth.get()))
        return entries.back().mode;
    for (int i = (int)entries.size() - 1; i >= 0; i--)
        if (entries[i].mode->getDataMode()->getBandwidth() == bandwidth)
            return entries[i].mode;
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getSlowerMode(const IIeee80211Mode *mode) const
{
    int index = findModeIndex(mode);
    if (index > 0)
        return entries[index - 1].mode;
    else
        return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getFasterMode(const IIeee80211Mode *mode) const
{
    int index = findModeIndex(mode);
    if (index >= 0 && index < (int)entries.size() - 1)
        return entries[index + 1].mode;
    else
        return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getSlowestMandatoryMode(Hz bandwidth) const
{
    for (size_t i = 0; i < entries.size(); i++)
        if (entries[i].isMandatory && (std::isnan(bandwidth.get()) || entries[i].mode->getDataMode()->getBandwidth() == bandwidth))
            return entries[i].mode;
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getFastestMandatoryMode(Hz bandwidth) const
{
    for (int i = (int)entries.size() - 1; i >= 0; i--)
        if (entries[i].isMandatory && (std::isnan(bandwidth.get()) || entries[i].mode->getDataMode()->getBandwidth() == bandwidth))
            return entries[i].mode;
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getFastestBasicMode(Hz operatingChannelWidth) const
{
    return getFastestMandatoryMode(channelWidthScopedBasicRates ? operatingChannelWidth : Hz(NaN));
}

const IIeee80211Mode *Ieee80211ModeSet::getFastestLegacyBasicMode() const
{
    for (int i = (int)entries.size() - 1; i >= 0; i--)
        if (entries[i].isMandatory &&
                dynamic_cast<const Ieee80211HtMode *>(entries[i].mode) == nullptr &&
                dynamic_cast<const Ieee80211VhtMode *>(entries[i].mode) == nullptr &&
                dynamic_cast<const Ieee80211HeMode *>(entries[i].mode) == nullptr &&
                dynamic_cast<const Ieee80211EhtMode *>(entries[i].mode) == nullptr)
            return entries[i].mode;
    return nullptr;
}

bps Ieee80211ModeSet::getNonHtReferenceRate(const IIeee80211Mode *mode) const
{
    if (mode == nullptr)
        throw cRuntimeError("Non-HT reference-rate mapping requires a mode");
    // IEEE Std 802.11-2024, Table 10-10: non-HT reference rates for HT/VHT MCSs.
    auto mapModulationAndCoding = [](const Ieee80211OfdmModulation *modulation,
            double codeRate) {
        if (modulation == nullptr)
            throw cRuntimeError("HT/VHT non-HT reference-rate mapping requires stream-1 modulation and coding");
        int constellationBits = modulation->getSubcarrierModulation()->getCodeWordSize();
        if (constellationBits <= 1) return codeRate <= 0.51 ? Mbps(6) : Mbps(9);
        if (constellationBits == 2) return codeRate <= 0.51 ? Mbps(12) : Mbps(18);
        if (constellationBits == 4) return codeRate <= 0.51 ? Mbps(24) : Mbps(36);
        if (constellationBits == 6) return codeRate <= 0.68 ? Mbps(48) : Mbps(54);
        return Mbps(54);
    };
    if (auto htMode = dynamic_cast<const Ieee80211HtMode *>(mode)) {
        auto dataMode = htMode->getDataMode();
        return mapModulationAndCoding(dataMode->getModulation(),
                dataMode->getCode()->getForwardErrorCorrection()->getCodeRate());
    }
    if (auto vhtMode = dynamic_cast<const Ieee80211VhtMode *>(mode)) {
        auto dataMode = vhtMode->getDataMode();
        return mapModulationAndCoding(dataMode->getModulation(),
                dataMode->getCode()->getForwardErrorCorrection()->getCodeRate());
    }
    if (!containsMode(mode))
        throw cRuntimeError("Legacy non-HT reference-rate mapping requires a mode from this mode set");
    return mode->getDataMode()->getNetBitrate();
}

const IIeee80211Mode *Ieee80211ModeSet::getSlowerMandatoryMode(const IIeee80211Mode *mode) const
{
    int index = findModeIndex(mode);
    if (index > 0)
        for (int i = index - 1; i >= 0; i--)
            if (entries[i].isMandatory)
                return entries[i].mode;
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getFasterMandatoryMode(const IIeee80211Mode *mode) const
{
    int index = findModeIndex(mode);
    if (index >= 0)
        for (size_t i = index + 1; i < entries.size(); i++)
            if (entries[i].isMandatory)
                return entries[i].mode;
    return nullptr;
}

const Ieee80211ModeSet *Ieee80211ModeSet::findModeSet(const char *mode)
{
    // The compatibility spelling resolves to the canonical 5 GHz profile;
    // configured radios use the band-aware overload below.
    if (!strcmp(mode, "ax"))
        mode = "ax-5GHz";
    for (size_t index = 0; index < (&modeSets)->size(); index++) {
        const Ieee80211ModeSet *modeSet = &(&modeSets)->at(index);
        if (!strcmp(modeSet->getProfileName(), "ax-catalog"))
            continue;
        if (strcmp(modeSet->getProfileName(), mode) == 0)
            return modeSet;
    }
    return nullptr;
}

const Ieee80211ModeSet *Ieee80211ModeSet::findModeSet(const char *mode, const IIeee80211Band *band)
{
    if (strcmp(mode, "ax") || band == nullptr)
        return findModeSet(mode);
    switch (band->getBandFamily()) {
        case Ieee80211BandFamily::BAND_2_4_GHZ: return findModeSet("ax-2.4GHz");
        case Ieee80211BandFamily::BAND_5_GHZ:
        case Ieee80211BandFamily::BAND_5_9_GHZ: return findModeSet("ax-5GHz");
        case Ieee80211BandFamily::BAND_6_GHZ: return findModeSet("ax-6GHz");
        default: throw cRuntimeError("The standards-oriented ax profile is not defined for band '%s'", band->getName());
    }
}

const Ieee80211ModeSet *Ieee80211ModeSet::getModeSet(const char *mode)
{
    const Ieee80211ModeSet *modeSet = findModeSet(mode);
    if (modeSet == nullptr) {
        std::string validModeSets;
        for (size_t index = 0; index < (&modeSets)->size(); index++) {
            const Ieee80211ModeSet *modeSet = &(&modeSets)->at(index);
            if (!strcmp(modeSet->getProfileName(), "ax-catalog"))
                continue;
            validModeSets += std::string("'") + modeSet->getName() + "' ";
        }
        throw cRuntimeError("Unknown 802.11 operational mode: '%s', valid modes are: %s", mode, validModeSets.c_str());
    }
    else
        return modeSet;
}

const Ieee80211ModeSet *Ieee80211ModeSet::getModeSet(const char *mode, const IIeee80211Band *band)
{
    const Ieee80211ModeSet *modeSet = findModeSet(mode, band);
    if (modeSet == nullptr)
        throw cRuntimeError("Unknown 802.11 operational mode '%s' for band '%s'", mode,
                band == nullptr ? "<unspecified>" : band->getName());
    return modeSet;
}

Hz Ieee80211ModeSet::getChannelWidth(const IIeee80211Band *band, Hz configuredBandwidth)
{
    if (band == nullptr)
        return configuredBandwidth;
    if (band->getChannelTopology() == Ieee80211ChannelTopology::NONCONTIGUOUS)
        throw cRuntimeError("80+80 MHz topology cannot be represented by a scalar channel width");
    if (!std::isnan(configuredBandwidth.get()))
        return configuredBandwidth;
    if (!std::isnan(band->getChannelWidth().get()))
        return band->getChannelWidth();
    return MHz(20);
}

Hz Ieee80211ModeSet::getModeBandwidth(const IIeee80211Band *band, Hz configuredBandwidth) const
{
    return bandAware ? getChannelWidth(band, configuredBandwidth) :
            band != nullptr ? band->getSpacing() : configuredBandwidth;
}

bool Ieee80211ModeSet::supportsChannel(const IIeee80211Band *band, Hz configuredBandwidth) const
{
    if (band == nullptr || band->getChannelTopology() == Ieee80211ChannelTopology::NONCONTIGUOUS)
        return false;
    bool matchingBand = operatingBand == Ieee80211OperatingBand::BAND_2_4_GHZ ?
            band->getBandFamily() == Ieee80211BandFamily::BAND_2_4_GHZ :
            operatingBand == Ieee80211OperatingBand::BAND_5_GHZ ?
                    band->getBandFamily() == Ieee80211BandFamily::BAND_5_GHZ ||
                    band->getBandFamily() == Ieee80211BandFamily::BAND_5_9_GHZ :
                    band->getBandFamily() == Ieee80211BandFamily::BAND_6_GHZ;
    if (!matchingBand)
        return false;
    auto width = getChannelWidth(band, configuredBandwidth);
    uint8_t widthMask = width == MHz(20) ? IEEE80211_WIDTH_20 :
            width == MHz(40) ? IEEE80211_WIDTH_40 :
            width == MHz(80) ? IEEE80211_WIDTH_80 :
            width == MHz(160) ? IEEE80211_WIDTH_160 :
            width == MHz(320) ? IEEE80211_WIDTH_320 : 0;
    return widthMask != 0 && (supportedChannelWidths & widthMask) != 0;
}

void Ieee80211ModeSet::validateChannel(const IIeee80211Band *band, Hz configuredBandwidth) const
{
    if (band == nullptr)
        throw cRuntimeError("802.11 mode profile '%s' requires an operating band", profileName.c_str());
    if (band->getChannelTopology() == Ieee80211ChannelTopology::NONCONTIGUOUS)
        throw cRuntimeError("802.11 mode profile '%s' does not support noncontiguous band '%s'",
                profileName.c_str(), band->getName());
    auto width = getChannelWidth(band, configuredBandwidth);
    if (!supportsChannel(band, configuredBandwidth))
        throw cRuntimeError("802.11 mode profile '%s' does not support channel width %g MHz on band '%s'",
                profileName.c_str(), width.get<MHz>(), band->getName());
}

} // namespace physicallayer

} // namespace inet
