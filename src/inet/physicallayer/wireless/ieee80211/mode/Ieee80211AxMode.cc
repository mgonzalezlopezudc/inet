//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211AxMode.h"

#include <algorithm>
#include <cmath>

#include "inet/physicallayer/wireless/common/modulation/BpskModulation.h"
#include "inet/physicallayer/wireless/common/modulation/QpskModulation.h"
#include "inet/physicallayer/wireless/common/modulation/Qam16Modulation.h"
#include "inet/physicallayer/wireless/common/modulation/Qam64Modulation.h"
#include "inet/physicallayer/wireless/common/modulation/Qam256Modulation.h"
#include "inet/physicallayer/wireless/common/modulation/Qam1024Modulation.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmCode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtCode.h"

namespace inet {
namespace physicallayer {

Ieee80211AxMode::Ieee80211AxMode(const char *name, const Ieee80211AxPreambleMode *preambleMode, const Ieee80211AxDataMode *dataMode, BandMode centerFrequencyMode) :
    Ieee80211ModeBase(name),
    preambleMode(preambleMode),
    dataMode(dataMode),
    centerFrequencyMode(centerFrequencyMode)
{
}

const simtime_t Ieee80211AxMode::getSlotTime() const
{
    return 9E-6; // Slot time is 9µs for 802.11ax
}

const simtime_t Ieee80211AxMode::getSifsTime() const
{
    return 16E-6; // SIFS is 16µs for 802.11ax 5GHz
}

Ieee80211AxModeBase::Ieee80211AxModeBase(unsigned int modulationAndCodingScheme, unsigned int numberOfSpatialStreams, const Hz bandwidth, GuardIntervalType guardIntervalType) :
    bandwidth(bandwidth),
    guardIntervalType(guardIntervalType),
    mcsIndex(modulationAndCodingScheme),
    numberOfSpatialStreams(numberOfSpatialStreams),
    netBitrate(NaN),
    grossBitrate(NaN)
{
}

bps Ieee80211AxModeBase::getNetBitrate() const
{
    if (std::isnan(netBitrate.get()))
        netBitrate = computeNetBitrate();
    return netBitrate;
}

bps Ieee80211AxModeBase::getGrossBitrate() const
{
    if (std::isnan(grossBitrate.get()))
        grossBitrate = computeGrossBitrate();
    return grossBitrate;
}

int Ieee80211AxModeBase::getNumberOfDataSubcarriers() const
{
    if (bandwidth == MHz(20))
        return 234;
    else if (bandwidth == MHz(40))
        return 468;
    else if (bandwidth == MHz(80))
        return 980;
    else if (bandwidth == MHz(160))
        return 1960;
    else
        throw cRuntimeError("Unsupported bandwidth");
}

int Ieee80211AxModeBase::getNumberOfPilotSubcarriers() const
{
    if (bandwidth == MHz(20))
        return 8;
    else if (bandwidth == MHz(40))
        return 16;
    else if (bandwidth == MHz(80))
        return 16;
    else if (bandwidth == MHz(160))
        return 32;
    else
        throw cRuntimeError("Unsupported bandwidth");
}

// Signal Mode
Ieee80211AxSignalMode::Ieee80211AxSignalMode(unsigned int modulationAndCodingScheme, const Ieee80211OfdmModulation *modulation, const Ieee80211VhtCode *code, const Hz bandwidth, GuardIntervalType guardIntervalType) :
    Ieee80211AxModeBase(modulationAndCodingScheme, 1, bandwidth, guardIntervalType),
    modulation(modulation),
    code(code)
{
}

Ieee80211AxSignalMode::Ieee80211AxSignalMode(unsigned int modulationAndCodingScheme, const Ieee80211OfdmModulation *modulation, const Ieee80211ConvolutionalCode *convolutionalCode, const Hz bandwidth, GuardIntervalType guardIntervalType) :
    Ieee80211AxModeBase(modulationAndCodingScheme, 1, bandwidth, guardIntervalType),
    modulation(modulation)
{
    code = Ieee80211VhtCompliantCodes::getCompliantCode(convolutionalCode, modulation, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, bandwidth, false);
}

Ieee80211AxSignalMode::~Ieee80211AxSignalMode()
{
    delete code;
}

bps Ieee80211AxSignalMode::computeGrossBitrate() const
{
    unsigned int numberOfCodedBitsPerSymbol = modulation->getSubcarrierModulation()->getCodeWordSize() * getNumberOfDataSubcarriers();
    if (guardIntervalType == AX_GUARD_INTERVAL_LONG)
        return bps(numberOfCodedBitsPerSymbol / getSymbolInterval());
    else
        return bps(numberOfCodedBitsPerSymbol / getShortGISymbolInterval());
}

bps Ieee80211AxSignalMode::computeNetBitrate() const
{
    return computeGrossBitrate() * code->getForwardErrorCorrection()->getCodeRate();
}

// Preamble Mode
Ieee80211AxPreambleMode::Ieee80211AxPreambleMode(const Ieee80211AxSignalMode *highEfficiencySignalMode, const Ieee80211OfdmSignalMode *legacySignalMode, HighEfficiencyPreambleFormat preambleFormat, unsigned int numberOfSpatialStreams) :
    highEfficiencySignalMode(highEfficiencySignalMode),
    legacySignalMode(legacySignalMode),
    preambleFormat(preambleFormat),
    numberOfHELongTrainings(computeNumberOfHELongTrainings(numberOfSpatialStreams))
{
}

unsigned int Ieee80211AxPreambleMode::computeNumberOfHELongTrainings(unsigned int numberOfSpatialStreams) const
{
    if (numberOfSpatialStreams == 1) return 1;
    if (numberOfSpatialStreams == 2) return 2;
    return 4;
}

const simtime_t Ieee80211AxPreambleMode::getDuration() const
{
    // Return standard HE-SU/MU preamble duration
    return getNonHTShortTrainingSequenceDuration() +
           getNonHTLongTrainingFieldDuration() +
           getNonHTSignalField() +
           getHeSignalFieldA() +
           getHeShortTrainingFieldDuration() +
           numberOfHELongTrainings * 4E-6;
}

static int getNumberOfTotalSubcarriers(Hz bandwidth) {
    if (bandwidth == MHz(20))
        return 242;
    else if (bandwidth == MHz(40))
        return 484;
    else if (bandwidth == MHz(80))
        return 996;
    else if (bandwidth == MHz(160))
        return 1992;
    else
        throw cRuntimeError("Unsupported bandwidth");
}

// MCS Table representation
Ieee80211Axmcs::Ieee80211Axmcs(unsigned int mcsIndex, const ApskModulationBase *stream1SubcarrierModulation, const Ieee80211ConvolutionalCode* convolutionalCode, Hz bandwidth) :
    mcsIndex(mcsIndex),
    bandwidth(bandwidth)
{
    stream1Modulation = new Ieee80211OfdmModulation(getNumberOfTotalSubcarriers(bandwidth), stream1SubcarrierModulation);
    code = Ieee80211VhtCompliantCodes::getCompliantCode(convolutionalCode, stream1Modulation, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, bandwidth, true);
}

Ieee80211Axmcs::Ieee80211Axmcs(unsigned int mcsIndex, const ApskModulationBase *stream1SubcarrierModulation, const ApskModulationBase *stream2SubcarrierModulation, const Ieee80211ConvolutionalCode* convolutionalCode, Hz bandwidth) :
    mcsIndex(mcsIndex),
    bandwidth(bandwidth)
{
    stream1Modulation = new Ieee80211OfdmModulation(getNumberOfTotalSubcarriers(bandwidth), stream1SubcarrierModulation);
    stream2Modulation = new Ieee80211OfdmModulation(getNumberOfTotalSubcarriers(bandwidth), stream2SubcarrierModulation);
    code = Ieee80211VhtCompliantCodes::getCompliantCode(convolutionalCode, stream1Modulation, stream2Modulation, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, bandwidth, true);
}

Ieee80211Axmcs::~Ieee80211Axmcs()
{
    delete code;
    delete stream1Modulation;
    delete stream2Modulation;
}

// Data Mode
Ieee80211AxDataMode::Ieee80211AxDataMode(const Ieee80211Axmcs *modulationAndCodingScheme, const Hz bandwidth, GuardIntervalType guardIntervalType) :
    Ieee80211AxModeBase(modulationAndCodingScheme->getMcsIndex(), modulationAndCodingScheme->getNumNss(), bandwidth, guardIntervalType),
    modulationAndCodingScheme(modulationAndCodingScheme),
    numberOfBccEncoders(computeNumberOfBccEncoders())
{
}

bps Ieee80211AxDataMode::computeGrossBitrate() const
{
    unsigned int numberOfCodedBitsPerSubcarrierSum = computeNumberOfCodedBitsPerSubcarrierSum();
    unsigned int numberOfCodedBitsPerSymbol = numberOfCodedBitsPerSubcarrierSum * getNumberOfDataSubcarriers();
    if (guardIntervalType == AX_GUARD_INTERVAL_LONG)
        return bps(numberOfCodedBitsPerSymbol / getSymbolInterval());
    else
        return bps(numberOfCodedBitsPerSymbol / getShortGISymbolInterval());
}

bps Ieee80211AxDataMode::computeNetBitrate() const
{
    return getGrossBitrate() * getCode()->getForwardErrorCorrection()->getCodeRate();
}

unsigned int Ieee80211AxDataMode::computeNumberOfSpatialStreams(const Ieee80211Axmcs *mcs) const
{
    return mcs->getNumNss();
}

unsigned int Ieee80211AxDataMode::computeNumberOfCodedBitsPerSubcarrierSum() const
{
    return (modulationAndCodingScheme->getModulation() ? modulationAndCodingScheme->getModulation()->getSubcarrierModulation()->getCodeWordSize() : 0) +
           (modulationAndCodingScheme->getStreamExtension1Modulation() ? modulationAndCodingScheme->getStreamExtension1Modulation()->getSubcarrierModulation()->getCodeWordSize() : 0);
}

unsigned int Ieee80211AxDataMode::computeNumberOfBccEncoders() const
{
    return 1; // standard simplification
}

b Ieee80211AxDataMode::getCompleteLength(b dataLength) const
{
    return getServiceFieldLength() + getTailFieldLength() + dataLength;
}

const simtime_t Ieee80211AxDataMode::getDuration(b dataLength) const
{
    unsigned int numberOfCodedBitsPerSubcarrierSum = computeNumberOfCodedBitsPerSubcarrierSum();
    unsigned int numberOfCodedBitsPerSymbol = numberOfCodedBitsPerSubcarrierSum * getNumberOfDataSubcarriers();
    const IForwardErrorCorrection *forwardErrorCorrection = getCode() ? getCode()->getForwardErrorCorrection() : nullptr;
    unsigned int dataBitsPerSymbol = forwardErrorCorrection ? forwardErrorCorrection->getDecodedLength(numberOfCodedBitsPerSymbol) : numberOfCodedBitsPerSymbol;
    int numberOfSymbols = lrint(ceil((double)getCompleteLength(dataLength).get<b>() / dataBitsPerSymbol));
    if (guardIntervalType == AX_GUARD_INTERVAL_LONG)
        return numberOfSymbols * getSymbolInterval();
    else
        return numberOfSymbols * getShortGISymbolInterval();
}

// Compliant Modes Cache
OPP_THREAD_LOCAL const Ieee80211AxCompliantModes Ieee80211AxCompliantModes::singleton;

Ieee80211AxCompliantModes::Ieee80211AxCompliantModes()
{
}

Ieee80211AxCompliantModes::~Ieee80211AxCompliantModes()
{
    for (auto& entry : modeCache)
        delete entry.second;
}

const Ieee80211AxMode *Ieee80211AxCompliantModes::getCompliantMode(const Ieee80211Axmcs *mcsMode, Ieee80211AxMode::BandMode centerFrequencyMode, Ieee80211AxPreambleMode::HighEfficiencyPreambleFormat preambleFormat, Ieee80211AxModeBase::GuardIntervalType guardIntervalType)
{
    const char *name = "";
    unsigned int nss = mcsMode->getNumNss();
    auto modeId = std::make_tuple(mcsMode->getBandwidth(), mcsMode->getMcsIndex(), guardIntervalType, nss);
    auto mode = singleton.modeCache.find(modeId);
    if (mode == singleton.modeCache.end()) {
        const Ieee80211OfdmSignalMode *legacySignal = &Ieee80211OfdmCompliantModes::ofdmHeaderMode6MbpsRate13;
        const Ieee80211AxSignalMode *axSignal = new Ieee80211AxSignalMode(mcsMode->getMcsIndex(), &Ieee80211OfdmCompliantModulations::subcarriers52QbpskModulation, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, mcsMode->getBandwidth(), guardIntervalType);
        const Ieee80211AxDataMode *dataMode = new Ieee80211AxDataMode(mcsMode, mcsMode->getBandwidth(), guardIntervalType);
        const Ieee80211AxPreambleMode *preambleMode = new Ieee80211AxPreambleMode(axSignal, legacySignal, preambleFormat, dataMode->getNumberOfSpatialStreams());
        const Ieee80211AxMode *axMode = new Ieee80211AxMode(name, preambleMode, dataMode, centerFrequencyMode);
        singleton.modeCache.insert(std::pair<std::tuple<Hz, unsigned int, Ieee80211AxModeBase::GuardIntervalType, unsigned int>, const Ieee80211AxMode *>(modeId, axMode));
        return axMode;
    }
    return mode->second;
}

// DI tables
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs0BW20MHzNss1([](){ return new Ieee80211Axmcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs1BW20MHzNss1([](){ return new Ieee80211Axmcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs2BW20MHzNss1([](){ return new Ieee80211Axmcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs3BW20MHzNss1([](){ return new Ieee80211Axmcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs4BW20MHzNss1([](){ return new Ieee80211Axmcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs5BW20MHzNss1([](){ return new Ieee80211Axmcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs6BW20MHzNss1([](){ return new Ieee80211Axmcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs7BW20MHzNss1([](){ return new Ieee80211Axmcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs8BW20MHzNss1([](){ return new Ieee80211Axmcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs9BW20MHzNss1([](){ return new Ieee80211Axmcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs10BW20MHzNss1([](){ return new Ieee80211Axmcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs11BW20MHzNss1([](){ return new Ieee80211Axmcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20)); });

const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs0BW20MHzNss2([](){ return new Ieee80211Axmcs(0, &BpskModulation::singleton, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs1BW20MHzNss2([](){ return new Ieee80211Axmcs(1, &QpskModulation::singleton, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs2BW20MHzNss2([](){ return new Ieee80211Axmcs(2, &QpskModulation::singleton, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs3BW20MHzNss2([](){ return new Ieee80211Axmcs(3, &Qam16Modulation::singleton, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs4BW20MHzNss2([](){ return new Ieee80211Axmcs(4, &Qam16Modulation::singleton, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs5BW20MHzNss2([](){ return new Ieee80211Axmcs(5, &Qam64Modulation::singleton, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs6BW20MHzNss2([](){ return new Ieee80211Axmcs(6, &Qam64Modulation::singleton, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs7BW20MHzNss2([](){ return new Ieee80211Axmcs(7, &Qam64Modulation::singleton, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs8BW20MHzNss2([](){ return new Ieee80211Axmcs(8, &Qam256Modulation::singleton, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs9BW20MHzNss2([](){ return new Ieee80211Axmcs(9, &Qam256Modulation::singleton, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs10BW20MHzNss2([](){ return new Ieee80211Axmcs(10, &Qam1024Modulation::singleton, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Axmcs> Ieee80211AxmcsTable::axMcs11BW20MHzNss2([](){ return new Ieee80211Axmcs(11, &Qam1024Modulation::singleton, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20)); });

} // namespace physicallayer
} // namespace inet
