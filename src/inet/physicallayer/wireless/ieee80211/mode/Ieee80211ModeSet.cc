//
// Copyright (C) 2012 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

#include <algorithm>

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ErpOfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211FhssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HrDsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211IrMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"

namespace inet {

namespace physicallayer {

namespace {

const IIeee80211Mode *resolveFecVariant(const IIeee80211Mode *mode, Ieee80211FecType fecType)
{
    if (mode == nullptr)
        return nullptr;
    if (mode->getDataMode()->getFecType() == fecType)
        return mode;
    if (auto htMode = dynamic_cast<const Ieee80211HtMode *>(mode)) {
        const auto *dataMode = htMode->getDataMode();
        return Ieee80211HtCompliantModes::getCompliantMode(dataMode->getModulationAndCodingScheme(),
                htMode->getCenterFrequencyMode(), htMode->getPreambleMode()->getPreambleFormat(),
                dataMode->getGuardIntervalType(), fecType, 0);
    }
    if (auto vhtMode = dynamic_cast<const Ieee80211VhtMode *>(mode)) {
        const auto *dataMode = vhtMode->getDataMode();
        return Ieee80211VhtCompliantModes::getCompliantMode(dataMode->getModulationAndCodingScheme(),
                vhtMode->getCenterFrequencyMode(), vhtMode->getPreambleMode()->getPreambleFormat(),
                dataMode->getGuardIntervalType(), fecType, 0);
    }
    return nullptr;
}

} // namespace

Register_Abstract_Class(Ieee80211ModeSet);

const DelayedInitializer<std::vector<Ieee80211ModeSet>> Ieee80211ModeSet::modeSets([]() { return new std::vector<Ieee80211ModeSet> {
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
    Ieee80211ModeSet("n(mixed-2.4Ghz)", { // This table is not complete; it only contains 2.4GHz homogeneous spatial streams, all mandatory and optional modes
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs0BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs1BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs2BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs3BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs4BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs5BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs6BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { true, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs7BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs8BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs9BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs10BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs11BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs12BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs13BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs14BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs15BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs16BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs17BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs18BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs19BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs20BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs21BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs22BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs23BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs24BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs25BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs26BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs27BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs28BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs29BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs30BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs31BW20MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs0BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs1BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs2BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs3BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs4BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs5BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs6BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs7BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs8BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs9BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs10BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs11BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs12BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs13BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs14BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs15BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs16BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs17BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs18BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs19BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs20BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs21BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs22BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs23BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs24BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs25BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs26BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs27BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs28BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs29BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs30BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) },
        { false, Ieee80211HtCompliantModes::getCompliantMode(&Ieee80211HtmcsTable::htMcs31BW40MHz, Ieee80211HtMode::BAND_2_4GHZ, Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED, Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT) }
    }),
    Ieee80211ModeSet("ac", {
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
}),}; });

Ieee80211ModeSet::Ieee80211ModeSet(const char *name, const std::vector<Entry> entries) :
    name(name),
    entries(entries)
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
}

int Ieee80211ModeSet::findModeIndex(const IIeee80211Mode *mode) const
{
    for (size_t index = 0; index < entries.size(); index++)
        if (entries[index].mode == mode)
            return index;
    if (mode != nullptr && mode->getDataMode()->getFecType() != Ieee80211FecType::BCC) {
        const IIeee80211Mode *bccMode = resolveFecVariant(mode, Ieee80211FecType::BCC);
        for (size_t index = 0; index < entries.size(); index++)
            if (entries[index].mode == bccMode)
                return index;
    }
    return -1;
}

int Ieee80211ModeSet::findCompatibleMetadataIndex(const IIeee80211Mode *mode) const
{
    int exactIndex = findModeIndex(mode);
    if (exactIndex >= 0)
        return exactIndex;
    if (mode != nullptr && mode->getDataMode()->getPhyFormat() == Ieee80211PhyFormat::HT) {
        const auto *htDataMode = mode->getDataMode();
        // Initial VHT support covers only the equal-modulation HT MCS0-31
        // capability mask. MCS32 and unequal-modulation MCS33-76 have no
        // compatible VHT-SU metadata tuple.
        if (htDataMode->getMcsIndex() > 31)
            return -1;
        int htBaseMcs = htDataMode->getMcsIndex() % 8;
        for (size_t index = 0; index < entries.size(); index++) {
            const auto *dataMode = entries[index].mode->getDataMode();
            if (dataMode->getPhyFormat() == Ieee80211PhyFormat::VHT_SU &&
                dataMode->getBandwidth() == htDataMode->getBandwidth() &&
                dataMode->getNumberOfSpatialStreams() == htDataMode->getNumberOfSpatialStreams() &&
                static_cast<int>(dataMode->getMcsIndex()) == htBaseMcs)
                return index;
        }
    }
    // A mode set entry may advertise the same legal PHY tuple with the
    // opposite GI. GI is selected from the received header, so capability
    // membership must compare the tuple without treating that entry's GI as
    // an authority.
    if (mode != nullptr) {
        const auto *dataMode = mode->getDataMode();
        for (size_t index = 0; index < entries.size(); index++) {
            const auto *entryDataMode = entries[index].mode->getDataMode();
            if (entryDataMode->getPhyFormat() == dataMode->getPhyFormat() &&
                entryDataMode->getMcsIndex() == dataMode->getMcsIndex() &&
                entryDataMode->getBandwidth() == dataMode->getBandwidth() &&
                entryDataMode->getNumberOfSpatialStreams() == dataMode->getNumberOfSpatialStreams() &&
                resolveFecVariant(entries[index].mode, dataMode->getFecType()) != nullptr)
                return index;
        }
    }
    return -1;
}

const IIeee80211Mode *Ieee80211ModeSet::resolveEntryMode(const IIeee80211Mode *entryMode,
        const IIeee80211Mode *referenceMode) const
{
    if (entryMode == nullptr || referenceMode == nullptr)
        return nullptr;
    if (referenceMode->getDataMode()->getPhyFormat() == Ieee80211PhyFormat::HT &&
        entryMode->getDataMode()->getPhyFormat() == Ieee80211PhyFormat::VHT_SU) {
        const auto *referenceHtMode = check_and_cast<const Ieee80211HtMode *>(referenceMode);
        const auto *entryDataMode = entryMode->getDataMode();
        unsigned int baseMcs = entryDataMode->getMcsIndex();
        unsigned int numberOfSpatialStreams = entryDataMode->getNumberOfSpatialStreams();
        Hz bandwidth = entryDataMode->getBandwidth();
        if (baseMcs > 7 || numberOfSpatialStreams < 1 || numberOfSpatialStreams > 4 ||
            (bandwidth != MHz(20) && bandwidth != MHz(40)))
            return nullptr;
        unsigned int htMcsIndex = 8 * (numberOfSpatialStreams - 1) + baseMcs;
        const auto *htMcs = Ieee80211HtmcsTable::getMcs(htMcsIndex, bandwidth);
        const auto *referenceDataMode = referenceHtMode->getDataMode();
        return Ieee80211HtCompliantModes::getCompliantMode(htMcs,
                referenceHtMode->getCenterFrequencyMode(), referenceHtMode->getPreambleMode()->getPreambleFormat(),
                referenceDataMode->getGuardIntervalType(), referenceDataMode->getFecType(), referenceDataMode->getFecVariant());
    }
    return resolveFecVariant(entryMode, referenceMode->getDataMode()->getFecType());
}

bool Ieee80211ModeSet::containsMode(const IIeee80211Mode *mode) const
{
    return findCompatibleMetadataIndex(mode) != -1;
}

bool Ieee80211ModeSet::getIsMandatory(const IIeee80211Mode *mode) const
{
    int index = findCompatibleMetadataIndex(mode);
    if (index < 0)
        throw cRuntimeError("Unknown mode");
    return entries[index].isMandatory;
}

const IIeee80211Mode *Ieee80211ModeSet::getFecVariant(const IIeee80211Mode *mode, Ieee80211FecType fecType) const
{
    return resolveFecVariant(mode, fecType);
}

const IIeee80211Mode *Ieee80211ModeSet::findMode(bps bitrate, Hz bandwidth, int numSpatialStreams, Ieee80211FecType fecType) const
{
    return findMode(bitrate - Mbps(0.05), bitrate + Mbps(0.05), bandwidth, numSpatialStreams, fecType);
}

const IIeee80211Mode *Ieee80211ModeSet::findMode(bps minBitrate, bps maxBitrate, Hz bandwidth, int numSpatialStreams, Ieee80211FecType fecType) const
{
    for (size_t index = 0; index < entries.size(); index++) {
        auto mode = resolveFecVariant(entries[index].mode, fecType);
        if (mode == nullptr)
            continue;
        auto dataMode = mode->getDataMode();
        auto bitrate = dataMode->getNetBitrate();
        if (minBitrate <= bitrate && bitrate <= maxBitrate &&
            (std::isnan(bandwidth.get()) || dataMode->getBandwidth() == bandwidth) &&
            (numSpatialStreams == -1 || dataMode->getNumberOfSpatialStreams() == numSpatialStreams))
        {
            return mode;
        }
    }
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getMode(bps bitrate, Hz bandwidth, int numSpatialStreams, Ieee80211FecType fecType) const
{
    const IIeee80211Mode *mode = getMode(bitrate - Mbps(0.05), bitrate + Mbps(0.05), bandwidth, numSpatialStreams, fecType);
    if (mode == nullptr)
        throw cRuntimeError("Unknown bitrate: %g in operation mode: '%s'", bitrate.get(), getName());
    else
        return mode;
}

const IIeee80211Mode *Ieee80211ModeSet::getMode(bps minBitrate, bps maxBitrate, Hz bandwidth, int numSpatialStreams, Ieee80211FecType fecType) const
{
    const IIeee80211Mode *mode = findMode(minBitrate, maxBitrate, bandwidth, numSpatialStreams, fecType);
    if (mode == nullptr)
        throw cRuntimeError("Unknown bitrate: (%g - %g) in operation mode: '%s'", minBitrate.get(), maxBitrate.get(), getName());
    else
        return mode;
}

const IIeee80211Mode *Ieee80211ModeSet::getSlowestMode(Ieee80211FecType fecType) const
{
    return resolveFecVariant(entries.front().mode, fecType);
}

const IIeee80211Mode *Ieee80211ModeSet::getFastestMode(Ieee80211FecType fecType) const
{
    return resolveFecVariant(entries.back().mode, fecType);
}

const IIeee80211Mode *Ieee80211ModeSet::getSlowerMode(const IIeee80211Mode *mode) const
{
    int index = findCompatibleMetadataIndex(mode);
    for (int i = index - 1; i >= 0; i--)
        if (auto resolved = resolveEntryMode(entries[i].mode, mode))
            return resolved;
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getFasterMode(const IIeee80211Mode *mode) const
{
    int index = findCompatibleMetadataIndex(mode);
    if (index >= 0)
        for (size_t i = index + 1; i < entries.size(); i++)
            if (auto resolved = resolveEntryMode(entries[i].mode, mode))
                return resolved;
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getSlowestMandatoryMode(Ieee80211FecType fecType) const
{
    for (size_t i = 0; i < entries.size(); i++)
        if (entries[i].isMandatory)
            return resolveFecVariant(entries[i].mode, fecType);
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getFastestMandatoryMode(Ieee80211FecType fecType) const
{
    for (int i = (int)entries.size() - 1; i >= 0; i--)
        if (entries[i].isMandatory)
            return resolveFecVariant(entries[i].mode, fecType);
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getSlowerMandatoryMode(const IIeee80211Mode *mode) const
{
    int index = findCompatibleMetadataIndex(mode);
    if (index > 0)
        for (int i = index - 1; i >= 0; i--)
            if (entries[i].isMandatory)
                if (auto resolved = resolveEntryMode(entries[i].mode, mode))
                    return resolved;
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::getFasterMandatoryMode(const IIeee80211Mode *mode) const
{
    int index = findCompatibleMetadataIndex(mode);
    if (index >= 0)
        for (size_t i = index + 1; i < entries.size(); i++)
            if (entries[i].isMandatory)
                if (auto resolved = resolveEntryMode(entries[i].mode, mode))
                    return resolved;
    return nullptr;
}

const Ieee80211ModeSet *Ieee80211ModeSet::findModeSet(const char *mode)
{
    for (size_t index = 0; index < (&modeSets)->size(); index++) {
        const Ieee80211ModeSet *modeSet = &(&modeSets)->at(index);
        if (strcmp(modeSet->getName(), mode) == 0)
            return modeSet;
    }
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::findMode(Ieee80211PhyFormat phyFormat,
        unsigned int mcsIndex, Hz bandwidth, unsigned int numberOfSpatialStreams,
        Ieee80211FecType fecType)
{
    const Ieee80211ModeSet *modeSet;
    if (phyFormat == Ieee80211PhyFormat::HT)
        modeSet = getModeSet("n(mixed-2.4Ghz)");
    else if (phyFormat == Ieee80211PhyFormat::VHT_SU)
        modeSet = getModeSet("ac");
    else
        return nullptr;

    for (int i = 0; i < modeSet->getNumModes(); i++) {
        const auto *mode = modeSet->getMode(i);
        const auto *dataMode = mode->getDataMode();
        if (dataMode->getPhyFormat() == phyFormat && dataMode->getMcsIndex() == mcsIndex &&
            dataMode->getBandwidth() == bandwidth &&
            dataMode->getNumberOfSpatialStreams() == static_cast<int>(numberOfSpatialStreams))
            return modeSet->getFecVariant(mode, fecType);
    }
    return nullptr;
}

const IIeee80211Mode *Ieee80211ModeSet::findMode(Ieee80211PhyFormat phyFormat,
        unsigned int mcsIndex, Hz bandwidth, unsigned int numberOfSpatialStreams,
        Ieee80211FecType fecType, bool shortGi)
{
    const Ieee80211ModeSet *modeSet;
    if (phyFormat == Ieee80211PhyFormat::HT)
        modeSet = getModeSet("n(mixed-2.4Ghz)");
    else if (phyFormat == Ieee80211PhyFormat::VHT_SU)
        modeSet = getModeSet("ac");
    else
        return nullptr;

    const IIeee80211Mode *referenceMode = nullptr;
    for (int i = 0; i < modeSet->getNumModes(); i++) {
        const auto *candidate = modeSet->getMode(i);
        const auto *dataMode = candidate->getDataMode();
        if (dataMode->getPhyFormat() == phyFormat && dataMode->getMcsIndex() == mcsIndex &&
            dataMode->getBandwidth() == bandwidth &&
            dataMode->getNumberOfSpatialStreams() == static_cast<int>(numberOfSpatialStreams) &&
            modeSet->getFecVariant(candidate, fecType) != nullptr) {
            referenceMode = candidate;
            break;
        }
    }
    if (referenceMode == nullptr)
        return nullptr;

    if (phyFormat == Ieee80211PhyFormat::HT) {
        const auto *htMode = dynamic_cast<const Ieee80211HtMode *>(referenceMode);
        if (htMode == nullptr)
            return nullptr;
        const auto *fecMode = modeSet->getFecVariant(referenceMode, fecType);
        if (fecMode == nullptr)
            return nullptr;
        const auto *dataMode = check_and_cast<const Ieee80211HtDataMode *>(fecMode->getDataMode());
        return Ieee80211HtCompliantModes::getCompliantMode(
                dataMode->getModulationAndCodingScheme(), htMode->getCenterFrequencyMode(),
                htMode->getPreambleMode()->getPreambleFormat(),
                shortGi ? Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT : Ieee80211HtModeBase::HT_GUARD_INTERVAL_LONG,
                fecType, dataMode->getFecVariant());
    }

    const auto *vhtMode = dynamic_cast<const Ieee80211VhtMode *>(referenceMode);
    if (vhtMode == nullptr)
        return nullptr;
    const auto *fecMode = modeSet->getFecVariant(referenceMode, fecType);
    if (fecMode == nullptr)
        return nullptr;
    const auto *dataMode = check_and_cast<const Ieee80211VhtDataMode *>(fecMode->getDataMode());
    return Ieee80211VhtCompliantModes::getCompliantMode(
            dataMode->getModulationAndCodingScheme(), vhtMode->getCenterFrequencyMode(),
            vhtMode->getPreambleMode()->getPreambleFormat(),
            shortGi ? Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT : Ieee80211VhtModeBase::HT_GUARD_INTERVAL_LONG,
            fecType, dataMode->getFecVariant());
}

const Ieee80211ModeSet *Ieee80211ModeSet::getModeSet(const char *mode)
{
    const Ieee80211ModeSet *modeSet = findModeSet(mode);
    if (modeSet == nullptr) {
        std::string validModeSets;
        for (size_t index = 0; index < (&modeSets)->size(); index++) {
            const Ieee80211ModeSet *modeSet = &(&modeSets)->at(index);
            validModeSets += std::string("'") + modeSet->getName() + "' ";
        }
        throw cRuntimeError("Unknown 802.11 operational mode: '%s', valid modes are: %s", mode, validModeSets.c_str());
    }
    else
        return modeSet;
}

} // namespace physicallayer

} // namespace inet
