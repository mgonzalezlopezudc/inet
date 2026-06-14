//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"

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

Ieee80211HeMode::Ieee80211HeMode(const char *name, const Ieee80211HePreambleMode *preambleMode, const Ieee80211HeDataMode *dataMode, BandMode centerFrequencyMode) :
    Ieee80211ModeBase(name),
    preambleMode(preambleMode),
    dataMode(dataMode),
    centerFrequencyMode(centerFrequencyMode)
{
}

const simtime_t Ieee80211HeMode::getSlotTime() const
{
    return 9E-6; // Slot time is 9µs for 802.11ax/HE
}

const simtime_t Ieee80211HeMode::getSifsTime() const
{
    return 16E-6; // SIFS is 16µs for 802.11ax/HE 5GHz
}

Ieee80211HeModeBase::Ieee80211HeModeBase(unsigned int modulationAndCodingScheme, unsigned int numberOfSpatialStreams, const Hz bandwidth, GuardIntervalType guardIntervalType) :
    bandwidth(bandwidth),
    guardIntervalType(guardIntervalType),
    mcsIndex(modulationAndCodingScheme),
    numberOfSpatialStreams(numberOfSpatialStreams),
    netBitrate(NaN),
    grossBitrate(NaN)
{
}

bps Ieee80211HeModeBase::getNetBitrate() const
{
    if (std::isnan(netBitrate.get()))
        netBitrate = computeNetBitrate();
    return netBitrate;
}

bps Ieee80211HeModeBase::getGrossBitrate() const
{
    if (std::isnan(grossBitrate.get()))
        grossBitrate = computeGrossBitrate();
    return grossBitrate;
}

int Ieee80211HeModeBase::getNumberOfDataSubcarriers() const
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

int Ieee80211HeModeBase::getNumberOfPilotSubcarriers() const
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
Ieee80211HeSignalMode::Ieee80211HeSignalMode(unsigned int modulationAndCodingScheme, const Ieee80211OfdmModulation *modulation, const Ieee80211VhtCode *code, const Hz bandwidth, GuardIntervalType guardIntervalType) :
    Ieee80211HeModeBase(modulationAndCodingScheme, 1, bandwidth, guardIntervalType),
    modulation(modulation),
    code(code)
{
}

Ieee80211HeSignalMode::Ieee80211HeSignalMode(unsigned int modulationAndCodingScheme, const Ieee80211OfdmModulation *modulation, const Ieee80211ConvolutionalCode *convolutionalCode, const Hz bandwidth, GuardIntervalType guardIntervalType) :
    Ieee80211HeModeBase(modulationAndCodingScheme, 1, bandwidth, guardIntervalType),
    modulation(modulation)
{
    code = Ieee80211VhtCompliantCodes::getCompliantCode(convolutionalCode, modulation, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, bandwidth, false);
}

Ieee80211HeSignalMode::~Ieee80211HeSignalMode()
{
    delete code;
}

bps Ieee80211HeSignalMode::computeGrossBitrate() const
{
    unsigned int numberOfCodedBitsPerSymbol = modulation->getSubcarrierModulation()->getCodeWordSize() * getNumberOfDataSubcarriers();
    if (guardIntervalType == HE_GUARD_INTERVAL_LONG)
        return bps(numberOfCodedBitsPerSymbol / getSymbolInterval());
    else
        return bps(numberOfCodedBitsPerSymbol / getShortGISymbolInterval());
}

bps Ieee80211HeSignalMode::computeNetBitrate() const
{
    return computeGrossBitrate() * code->getForwardErrorCorrection()->getCodeRate();
}

// Preamble Mode
Ieee80211HePreambleMode::Ieee80211HePreambleMode(const Ieee80211HeSignalMode *highEfficiencySignalMode, const Ieee80211OfdmSignalMode *legacySignalMode, HighEfficiencyPreambleFormat preambleFormat, unsigned int numberOfSpatialStreams) :
    highEfficiencySignalMode(highEfficiencySignalMode),
    legacySignalMode(legacySignalMode),
    preambleFormat(preambleFormat),
    numberOfHELongTrainings(computeNumberOfHELongTrainings(numberOfSpatialStreams))
{
}

unsigned int Ieee80211HePreambleMode::computeNumberOfHELongTrainings(unsigned int numberOfSpatialStreams) const
{
    if (numberOfSpatialStreams == 1) return 1;
    if (numberOfSpatialStreams == 2) return 2;
    return 4;
}

const simtime_t Ieee80211HePreambleMode::getDuration() const
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
Ieee80211Hemcs::Ieee80211Hemcs(unsigned int mcsIndex, const ApskModulationBase *stream1SubcarrierModulation, const Ieee80211ConvolutionalCode* convolutionalCode, Hz bandwidth) :
    mcsIndex(mcsIndex),
    bandwidth(bandwidth)
{
    stream1Modulation = new Ieee80211OfdmModulation(getNumberOfTotalSubcarriers(bandwidth), stream1SubcarrierModulation);
    code = Ieee80211VhtCompliantCodes::getCompliantCode(convolutionalCode, stream1Modulation, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, bandwidth, true);
}

Ieee80211Hemcs::Ieee80211Hemcs(unsigned int mcsIndex, const ApskModulationBase *stream1SubcarrierModulation, const ApskModulationBase *stream2SubcarrierModulation, const Ieee80211ConvolutionalCode* convolutionalCode, Hz bandwidth) :
    mcsIndex(mcsIndex),
    bandwidth(bandwidth)
{
    stream1Modulation = new Ieee80211OfdmModulation(getNumberOfTotalSubcarriers(bandwidth), stream1SubcarrierModulation);
    stream2Modulation = new Ieee80211OfdmModulation(getNumberOfTotalSubcarriers(bandwidth), stream2SubcarrierModulation);
    code = Ieee80211VhtCompliantCodes::getCompliantCode(convolutionalCode, stream1Modulation, stream2Modulation, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, bandwidth, true);
}

Ieee80211Hemcs::~Ieee80211Hemcs()
{
    delete code;
    delete stream1Modulation;
    delete stream2Modulation;
}

// Data Mode
Ieee80211HeDataMode::Ieee80211HeDataMode(const Ieee80211Hemcs *modulationAndCodingScheme, const Hz bandwidth, GuardIntervalType guardIntervalType) :
    Ieee80211HeModeBase(modulationAndCodingScheme->getMcsIndex(), modulationAndCodingScheme->getNumNss(), bandwidth, guardIntervalType),
    modulationAndCodingScheme(modulationAndCodingScheme),
    numberOfBccEncoders(computeNumberOfBccEncoders())
{
}

bps Ieee80211HeDataMode::computeGrossBitrate() const
{
    unsigned int numberOfCodedBitsPerSubcarrierSum = computeNumberOfCodedBitsPerSubcarrierSum();
    unsigned int numberOfCodedBitsPerSymbol = numberOfCodedBitsPerSubcarrierSum * getNumberOfDataSubcarriers();
    if (guardIntervalType == HE_GUARD_INTERVAL_LONG)
        return bps(numberOfCodedBitsPerSymbol / getSymbolInterval());
    else
        return bps(numberOfCodedBitsPerSymbol / getShortGISymbolInterval());
}

bps Ieee80211HeDataMode::computeNetBitrate() const
{
    return getGrossBitrate() * getCode()->getForwardErrorCorrection()->getCodeRate();
}

unsigned int Ieee80211HeDataMode::computeNumberOfSpatialStreams(const Ieee80211Hemcs *mcs) const
{
    return mcs->getNumNss();
}

unsigned int Ieee80211HeDataMode::computeNumberOfCodedBitsPerSubcarrierSum() const
{
    return (modulationAndCodingScheme->getModulation() ? modulationAndCodingScheme->getModulation()->getSubcarrierModulation()->getCodeWordSize() : 0) +
           (modulationAndCodingScheme->getStreamExtension1Modulation() ? modulationAndCodingScheme->getStreamExtension1Modulation()->getSubcarrierModulation()->getCodeWordSize() : 0);
}

unsigned int Ieee80211HeDataMode::computeNumberOfBccEncoders() const
{
    return 1; // standard simplification
}

b Ieee80211HeDataMode::getCompleteLength(b dataLength) const
{
    return getServiceFieldLength() + getTailFieldLength() + dataLength;
}

const simtime_t Ieee80211HeDataMode::getDuration(b dataLength) const
{
    unsigned int numberOfCodedBitsPerSubcarrierSum = computeNumberOfCodedBitsPerSubcarrierSum();
    unsigned int numberOfCodedBitsPerSymbol = numberOfCodedBitsPerSubcarrierSum * getNumberOfDataSubcarriers();
    const IForwardErrorCorrection *forwardErrorCorrection = getCode() ? getCode()->getForwardErrorCorrection() : nullptr;
    unsigned int dataBitsPerSymbol = forwardErrorCorrection ? forwardErrorCorrection->getDecodedLength(numberOfCodedBitsPerSymbol) : numberOfCodedBitsPerSymbol;
    int numberOfSymbols = lrint(ceil((double)getCompleteLength(dataLength).get<b>() / dataBitsPerSymbol));
    if (guardIntervalType == HE_GUARD_INTERVAL_LONG)
        return numberOfSymbols * getSymbolInterval();
    else
        return numberOfSymbols * getShortGISymbolInterval();
}

// Compliant Modes Cache
OPP_THREAD_LOCAL const Ieee80211HeCompliantModes Ieee80211HeCompliantModes::singleton;

Ieee80211HeCompliantModes::Ieee80211HeCompliantModes()
{
}

Ieee80211HeCompliantModes::~Ieee80211HeCompliantModes()
{
    for (auto& entry : modeCache)
        delete entry.second;
}

const Ieee80211HeMode *Ieee80211HeCompliantModes::getCompliantMode(const Ieee80211Hemcs *mcsMode, Ieee80211HeMode::BandMode centerFrequencyMode, Ieee80211HePreambleMode::HighEfficiencyPreambleFormat preambleFormat, Ieee80211HeModeBase::GuardIntervalType guardIntervalType)
{
    const char *name = "";
    unsigned int nss = mcsMode->getNumNss();
    auto modeId = std::make_tuple(mcsMode->getBandwidth(), mcsMode->getMcsIndex(), guardIntervalType, nss);
    auto mode = singleton.modeCache.find(modeId);
    if (mode == singleton.modeCache.end()) {
        const Ieee80211OfdmSignalMode *legacySignal = &Ieee80211OfdmCompliantModes::ofdmHeaderMode6MbpsRate13;
        const Ieee80211HeSignalMode *heSignal = new Ieee80211HeSignalMode(mcsMode->getMcsIndex(), &Ieee80211OfdmCompliantModulations::subcarriers52QbpskModulation, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, mcsMode->getBandwidth(), guardIntervalType);
        const Ieee80211HeDataMode *dataMode = new Ieee80211HeDataMode(mcsMode, mcsMode->getBandwidth(), guardIntervalType);
        const Ieee80211HePreambleMode *preambleMode = new Ieee80211HePreambleMode(heSignal, legacySignal, preambleFormat, dataMode->getNumberOfSpatialStreams());
        const Ieee80211HeMode *heMode = new Ieee80211HeMode(name, preambleMode, dataMode, centerFrequencyMode);
        singleton.modeCache.insert(std::pair<std::tuple<Hz, unsigned int, Ieee80211HeModeBase::GuardIntervalType, unsigned int>, const Ieee80211HeMode *>(modeId, heMode));
        return heMode;
    }
    return mode->second;
}

// DI tables
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW20MHzNss1([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW20MHzNss1([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW20MHzNss1([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW20MHzNss1([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW20MHzNss1([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW20MHzNss1([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW20MHzNss1([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW20MHzNss1([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW20MHzNss1([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW20MHzNss1([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW20MHzNss1([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW20MHzNss1([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20)); });

const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW20MHzNss2([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW20MHzNss2([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW20MHzNss2([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW20MHzNss2([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW20MHzNss2([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW20MHzNss2([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW20MHzNss2([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW20MHzNss2([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW20MHzNss2([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW20MHzNss2([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW20MHzNss2([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20)); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW20MHzNss2([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20)); });

} // namespace physicallayer
} // namespace inet
