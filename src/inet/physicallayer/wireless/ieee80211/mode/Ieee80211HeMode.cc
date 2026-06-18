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
#include "inet/physicallayer/wireless/common/radio/bitlevel/ConvolutionalCode.h"

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
    switch (guardIntervalType) {
        case HE_GUARD_INTERVAL_SHORT: return bps(numberOfCodedBitsPerSymbol / getShortGISymbolInterval());
        case HE_GUARD_INTERVAL_MEDIUM: return bps(numberOfCodedBitsPerSymbol / getMediumGISymbolInterval());
        case HE_GUARD_INTERVAL_LONG: return bps(numberOfCodedBitsPerSymbol / getSymbolInterval());
        default: throw cRuntimeError("Unknown HE guard interval");
    }
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

Ieee80211Hemcs::Ieee80211Hemcs(unsigned int mcsIndex, const ApskModulationBase *stream1SubcarrierModulation, const Ieee80211ConvolutionalCode* convolutionalCode, Hz bandwidth, int nss) :
    mcsIndex(mcsIndex),
    stream1Modulation(new Ieee80211OfdmModulation(getNumberOfTotalSubcarriers(bandwidth), stream1SubcarrierModulation)),
    bandwidth(bandwidth)
{
    if (nss > 1)
        stream2Modulation = static_cast<Ieee80211OfdmModulation *>(stream1Modulation->dup());
    if (nss > 2)
        stream3Modulation = static_cast<Ieee80211OfdmModulation *>(stream1Modulation->dup());
    if (nss > 3)
        stream4Modulation = static_cast<Ieee80211OfdmModulation *>(stream1Modulation->dup());
    if (nss > 4)
        stream5Modulation = static_cast<Ieee80211OfdmModulation *>(stream1Modulation->dup());
    if (nss > 5)
        stream6Modulation = static_cast<Ieee80211OfdmModulation *>(stream1Modulation->dup());
    if (nss > 6)
        stream7Modulation = static_cast<Ieee80211OfdmModulation *>(stream1Modulation->dup());
    if (nss > 7)
        stream8Modulation = static_cast<Ieee80211OfdmModulation *>(stream1Modulation->dup());
    code = Ieee80211VhtCompliantCodes::getCompliantCode(convolutionalCode, stream1Modulation, stream2Modulation, stream3Modulation, stream4Modulation, stream5Modulation, stream6Modulation, stream7Modulation, stream8Modulation, bandwidth, true);
}

Ieee80211Hemcs::~Ieee80211Hemcs()
{
    delete code;
    delete stream1Modulation;
    delete stream2Modulation;
    delete stream3Modulation;
    delete stream4Modulation;
    delete stream5Modulation;
    delete stream6Modulation;
    delete stream7Modulation;
    delete stream8Modulation;
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
    switch (guardIntervalType) {
        case HE_GUARD_INTERVAL_SHORT: return bps(numberOfCodedBitsPerSymbol / getShortGISymbolInterval());
        case HE_GUARD_INTERVAL_MEDIUM: return bps(numberOfCodedBitsPerSymbol / getMediumGISymbolInterval());
        case HE_GUARD_INTERVAL_LONG: return bps(numberOfCodedBitsPerSymbol / getSymbolInterval());
        default: throw cRuntimeError("Unknown HE guard interval");
    }
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
           (modulationAndCodingScheme->getStreamExtension1Modulation() ? modulationAndCodingScheme->getStreamExtension1Modulation()->getSubcarrierModulation()->getCodeWordSize() : 0) +
           (modulationAndCodingScheme->getStreamExtension2Modulation() ? modulationAndCodingScheme->getStreamExtension2Modulation()->getSubcarrierModulation()->getCodeWordSize() : 0) +
           (modulationAndCodingScheme->getStreamExtension3Modulation() ? modulationAndCodingScheme->getStreamExtension3Modulation()->getSubcarrierModulation()->getCodeWordSize() : 0) +
           (modulationAndCodingScheme->getStreamExtension4Modulation() ? modulationAndCodingScheme->getStreamExtension4Modulation()->getSubcarrierModulation()->getCodeWordSize() : 0) +
           (modulationAndCodingScheme->getStreamExtension5Modulation() ? modulationAndCodingScheme->getStreamExtension5Modulation()->getSubcarrierModulation()->getCodeWordSize() : 0) +
           (modulationAndCodingScheme->getStreamExtension6Modulation() ? modulationAndCodingScheme->getStreamExtension6Modulation()->getSubcarrierModulation()->getCodeWordSize() : 0) +
           (modulationAndCodingScheme->getStreamExtension7Modulation() ? modulationAndCodingScheme->getStreamExtension7Modulation()->getSubcarrierModulation()->getCodeWordSize() : 0);
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
    if (forwardErrorCorrection) {
        // We can check the parameters of ConvolutionalCode
        if (auto convCode = dynamic_cast<const ConvolutionalCode *>(forwardErrorCorrection)) {
            int n = convCode->getCodeRatePuncturingN();
            if (numberOfCodedBitsPerSymbol % n != 0) {
                std::cerr << "CRASH DIAGNOSTIC from getDuration: "
                          << "bandwidth=" << bandwidth.get() << " Hz, "
                          << "mcsIndex=" << mcsIndex << ", "
                          << "numberOfSpatialStreams=" << numberOfSpatialStreams << ", "
                          << "numberOfCodedBitsPerSubcarrierSum=" << numberOfCodedBitsPerSubcarrierSum << ", "
                          << "getNumberOfDataSubcarriers()=" << getNumberOfDataSubcarriers() << ", "
                          << "numberOfCodedBitsPerSymbol=" << numberOfCodedBitsPerSymbol << ", "
                          << "puncturingN=" << n << std::endl;
            }
        }
    }
    unsigned int dataBitsPerSymbol = forwardErrorCorrection ? forwardErrorCorrection->getDecodedLength(numberOfCodedBitsPerSymbol) : numberOfCodedBitsPerSymbol;
    int numberOfSymbols = lrint(ceil((double)getCompleteLength(dataLength).get<b>() / dataBitsPerSymbol));
    switch (guardIntervalType) {
        case HE_GUARD_INTERVAL_SHORT: return numberOfSymbols * getShortGISymbolInterval();
        case HE_GUARD_INTERVAL_MEDIUM: return numberOfSymbols * getMediumGISymbolInterval();
        case HE_GUARD_INTERVAL_LONG: return numberOfSymbols * getSymbolInterval();
        default: throw cRuntimeError("Unknown HE guard interval");
    }
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
    auto modeId = std::make_tuple(mcsMode->getBandwidth(), mcsMode->getMcsIndex(),
            guardIntervalType, nss, centerFrequencyMode, preambleFormat);
    auto mode = singleton.modeCache.find(modeId);
    if (mode == singleton.modeCache.end()) {
        const Ieee80211OfdmSignalMode *legacySignal = &Ieee80211OfdmCompliantModes::ofdmHeaderMode6MbpsRate13;
        const Ieee80211HeSignalMode *heSignal = new Ieee80211HeSignalMode(mcsMode->getMcsIndex(), &Ieee80211OfdmCompliantModulations::subcarriers52QbpskModulation, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, mcsMode->getBandwidth(), guardIntervalType);
        const Ieee80211HeDataMode *dataMode = new Ieee80211HeDataMode(mcsMode, mcsMode->getBandwidth(), guardIntervalType);
        const Ieee80211HePreambleMode *preambleMode = new Ieee80211HePreambleMode(heSignal, legacySignal, preambleFormat, dataMode->getNumberOfSpatialStreams());
        const Ieee80211HeMode *heMode = new Ieee80211HeMode(name, preambleMode, dataMode, centerFrequencyMode);
        singleton.modeCache.insert({modeId, heMode});
        return heMode;
    }
    return mode->second;
}

// DI tables
// BW=20MHz, NSS=1
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW20MHzNss1([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW20MHzNss1([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW20MHzNss1([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW20MHzNss1([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW20MHzNss1([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW20MHzNss1([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(20), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW20MHzNss1([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW20MHzNss1([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW20MHzNss1([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW20MHzNss1([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW20MHzNss1([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW20MHzNss1([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 1); });
// BW=20MHz, NSS=2
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW20MHzNss2([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW20MHzNss2([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW20MHzNss2([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW20MHzNss2([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW20MHzNss2([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW20MHzNss2([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(20), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW20MHzNss2([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW20MHzNss2([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW20MHzNss2([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW20MHzNss2([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW20MHzNss2([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW20MHzNss2([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 2); });
// BW=20MHz, NSS=3
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW20MHzNss3([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW20MHzNss3([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW20MHzNss3([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW20MHzNss3([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW20MHzNss3([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW20MHzNss3([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(20), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW20MHzNss3([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW20MHzNss3([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW20MHzNss3([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW20MHzNss3([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW20MHzNss3([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW20MHzNss3([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 3); });
// BW=20MHz, NSS=4
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW20MHzNss4([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW20MHzNss4([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW20MHzNss4([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW20MHzNss4([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW20MHzNss4([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW20MHzNss4([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(20), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW20MHzNss4([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW20MHzNss4([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW20MHzNss4([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW20MHzNss4([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW20MHzNss4([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW20MHzNss4([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 4); });
// BW=20MHz, NSS=5
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW20MHzNss5([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW20MHzNss5([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW20MHzNss5([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW20MHzNss5([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW20MHzNss5([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW20MHzNss5([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(20), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW20MHzNss5([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW20MHzNss5([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW20MHzNss5([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW20MHzNss5([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW20MHzNss5([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW20MHzNss5([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 5); });
// BW=20MHz, NSS=6
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW20MHzNss6([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW20MHzNss6([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW20MHzNss6([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW20MHzNss6([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW20MHzNss6([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW20MHzNss6([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(20), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW20MHzNss6([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW20MHzNss6([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW20MHzNss6([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW20MHzNss6([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW20MHzNss6([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW20MHzNss6([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 6); });
// BW=20MHz, NSS=7
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW20MHzNss7([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW20MHzNss7([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW20MHzNss7([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW20MHzNss7([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW20MHzNss7([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW20MHzNss7([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(20), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW20MHzNss7([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW20MHzNss7([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW20MHzNss7([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW20MHzNss7([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW20MHzNss7([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW20MHzNss7([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 7); });
// BW=20MHz, NSS=8
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW20MHzNss8([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW20MHzNss8([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW20MHzNss8([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW20MHzNss8([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(20), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW20MHzNss8([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW20MHzNss8([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(20), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW20MHzNss8([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW20MHzNss8([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW20MHzNss8([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW20MHzNss8([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW20MHzNss8([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(20), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW20MHzNss8([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(20), 8); });
// BW=40MHz, NSS=1
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW40MHzNss1([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW40MHzNss1([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW40MHzNss1([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW40MHzNss1([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW40MHzNss1([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW40MHzNss1([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(40), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW40MHzNss1([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW40MHzNss1([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW40MHzNss1([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW40MHzNss1([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW40MHzNss1([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW40MHzNss1([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 1); });
// BW=40MHz, NSS=2
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW40MHzNss2([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW40MHzNss2([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW40MHzNss2([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW40MHzNss2([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW40MHzNss2([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW40MHzNss2([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(40), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW40MHzNss2([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW40MHzNss2([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW40MHzNss2([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW40MHzNss2([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW40MHzNss2([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW40MHzNss2([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 2); });
// BW=40MHz, NSS=3
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW40MHzNss3([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW40MHzNss3([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW40MHzNss3([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW40MHzNss3([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW40MHzNss3([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW40MHzNss3([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(40), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW40MHzNss3([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW40MHzNss3([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW40MHzNss3([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW40MHzNss3([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW40MHzNss3([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW40MHzNss3([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 3); });
// BW=40MHz, NSS=4
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW40MHzNss4([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW40MHzNss4([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW40MHzNss4([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW40MHzNss4([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW40MHzNss4([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW40MHzNss4([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(40), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW40MHzNss4([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW40MHzNss4([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW40MHzNss4([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW40MHzNss4([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW40MHzNss4([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW40MHzNss4([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 4); });
// BW=40MHz, NSS=5
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW40MHzNss5([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW40MHzNss5([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW40MHzNss5([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW40MHzNss5([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW40MHzNss5([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW40MHzNss5([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(40), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW40MHzNss5([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW40MHzNss5([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW40MHzNss5([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW40MHzNss5([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW40MHzNss5([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW40MHzNss5([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 5); });
// BW=40MHz, NSS=6
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW40MHzNss6([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW40MHzNss6([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW40MHzNss6([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW40MHzNss6([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW40MHzNss6([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW40MHzNss6([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(40), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW40MHzNss6([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW40MHzNss6([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW40MHzNss6([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW40MHzNss6([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW40MHzNss6([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW40MHzNss6([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 6); });
// BW=40MHz, NSS=7
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW40MHzNss7([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW40MHzNss7([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW40MHzNss7([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW40MHzNss7([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW40MHzNss7([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW40MHzNss7([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(40), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW40MHzNss7([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW40MHzNss7([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW40MHzNss7([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW40MHzNss7([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW40MHzNss7([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW40MHzNss7([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 7); });
// BW=40MHz, NSS=8
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW40MHzNss8([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW40MHzNss8([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW40MHzNss8([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW40MHzNss8([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(40), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW40MHzNss8([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW40MHzNss8([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(40), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW40MHzNss8([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW40MHzNss8([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW40MHzNss8([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW40MHzNss8([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW40MHzNss8([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(40), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW40MHzNss8([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(40), 8); });
// BW=80MHz, NSS=1
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW80MHzNss1([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW80MHzNss1([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW80MHzNss1([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW80MHzNss1([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW80MHzNss1([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW80MHzNss1([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(80), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW80MHzNss1([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW80MHzNss1([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW80MHzNss1([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW80MHzNss1([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW80MHzNss1([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW80MHzNss1([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 1); });
// BW=80MHz, NSS=2
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW80MHzNss2([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW80MHzNss2([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW80MHzNss2([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW80MHzNss2([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW80MHzNss2([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW80MHzNss2([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(80), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW80MHzNss2([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW80MHzNss2([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW80MHzNss2([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW80MHzNss2([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW80MHzNss2([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW80MHzNss2([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 2); });
// BW=80MHz, NSS=3
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW80MHzNss3([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW80MHzNss3([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW80MHzNss3([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW80MHzNss3([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW80MHzNss3([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW80MHzNss3([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(80), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW80MHzNss3([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW80MHzNss3([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW80MHzNss3([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW80MHzNss3([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW80MHzNss3([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW80MHzNss3([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 3); });
// BW=80MHz, NSS=4
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW80MHzNss4([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW80MHzNss4([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW80MHzNss4([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW80MHzNss4([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW80MHzNss4([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW80MHzNss4([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(80), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW80MHzNss4([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW80MHzNss4([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW80MHzNss4([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW80MHzNss4([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW80MHzNss4([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW80MHzNss4([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 4); });
// BW=80MHz, NSS=5
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW80MHzNss5([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW80MHzNss5([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW80MHzNss5([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW80MHzNss5([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW80MHzNss5([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW80MHzNss5([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(80), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW80MHzNss5([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW80MHzNss5([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW80MHzNss5([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW80MHzNss5([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW80MHzNss5([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW80MHzNss5([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 5); });
// BW=80MHz, NSS=6
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW80MHzNss6([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW80MHzNss6([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW80MHzNss6([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW80MHzNss6([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW80MHzNss6([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW80MHzNss6([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(80), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW80MHzNss6([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW80MHzNss6([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW80MHzNss6([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW80MHzNss6([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW80MHzNss6([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW80MHzNss6([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 6); });
// BW=80MHz, NSS=7
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW80MHzNss7([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW80MHzNss7([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW80MHzNss7([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW80MHzNss7([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW80MHzNss7([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW80MHzNss7([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(80), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW80MHzNss7([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW80MHzNss7([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW80MHzNss7([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW80MHzNss7([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW80MHzNss7([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW80MHzNss7([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 7); });
// BW=80MHz, NSS=8
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW80MHzNss8([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW80MHzNss8([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW80MHzNss8([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW80MHzNss8([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(80), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW80MHzNss8([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW80MHzNss8([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(80), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW80MHzNss8([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW80MHzNss8([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW80MHzNss8([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW80MHzNss8([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW80MHzNss8([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(80), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW80MHzNss8([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(80), 8); });
// BW=160MHz, NSS=1
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW160MHzNss1([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW160MHzNss1([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW160MHzNss1([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW160MHzNss1([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW160MHzNss1([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW160MHzNss1([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(160), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW160MHzNss1([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW160MHzNss1([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW160MHzNss1([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW160MHzNss1([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW160MHzNss1([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 1); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW160MHzNss1([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 1); });
// BW=160MHz, NSS=2
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW160MHzNss2([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW160MHzNss2([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW160MHzNss2([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW160MHzNss2([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW160MHzNss2([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW160MHzNss2([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(160), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW160MHzNss2([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW160MHzNss2([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW160MHzNss2([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW160MHzNss2([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW160MHzNss2([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 2); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW160MHzNss2([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 2); });
// BW=160MHz, NSS=3
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW160MHzNss3([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW160MHzNss3([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW160MHzNss3([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW160MHzNss3([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW160MHzNss3([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW160MHzNss3([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(160), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW160MHzNss3([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW160MHzNss3([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW160MHzNss3([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW160MHzNss3([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW160MHzNss3([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 3); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW160MHzNss3([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 3); });
// BW=160MHz, NSS=4
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW160MHzNss4([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW160MHzNss4([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW160MHzNss4([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW160MHzNss4([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW160MHzNss4([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW160MHzNss4([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(160), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW160MHzNss4([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW160MHzNss4([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW160MHzNss4([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW160MHzNss4([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW160MHzNss4([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 4); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW160MHzNss4([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 4); });
// BW=160MHz, NSS=5
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW160MHzNss5([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW160MHzNss5([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW160MHzNss5([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW160MHzNss5([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW160MHzNss5([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW160MHzNss5([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(160), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW160MHzNss5([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW160MHzNss5([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW160MHzNss5([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW160MHzNss5([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW160MHzNss5([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 5); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW160MHzNss5([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 5); });
// BW=160MHz, NSS=6
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW160MHzNss6([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW160MHzNss6([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW160MHzNss6([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW160MHzNss6([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW160MHzNss6([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW160MHzNss6([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(160), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW160MHzNss6([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW160MHzNss6([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW160MHzNss6([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW160MHzNss6([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW160MHzNss6([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 6); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW160MHzNss6([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 6); });
// BW=160MHz, NSS=7
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW160MHzNss7([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW160MHzNss7([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW160MHzNss7([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW160MHzNss7([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW160MHzNss7([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW160MHzNss7([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(160), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW160MHzNss7([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW160MHzNss7([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW160MHzNss7([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW160MHzNss7([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW160MHzNss7([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 7); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW160MHzNss7([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 7); });
// BW=160MHz, NSS=8
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs0BW160MHzNss8([](){ return new Ieee80211Hemcs(0, &BpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs1BW160MHzNss8([](){ return new Ieee80211Hemcs(1, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs2BW160MHzNss8([](){ return new Ieee80211Hemcs(2, &QpskModulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs3BW160MHzNss8([](){ return new Ieee80211Hemcs(3, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, MHz(160), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs4BW160MHzNss8([](){ return new Ieee80211Hemcs(4, &Qam16Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs5BW160MHzNss8([](){ return new Ieee80211Hemcs(5, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3, MHz(160), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs6BW160MHzNss8([](){ return new Ieee80211Hemcs(6, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs7BW160MHzNss8([](){ return new Ieee80211Hemcs(7, &Qam64Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs8BW160MHzNss8([](){ return new Ieee80211Hemcs(8, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs9BW160MHzNss8([](){ return new Ieee80211Hemcs(9, &Qam256Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs10BW160MHzNss8([](){ return new Ieee80211Hemcs(10, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4, MHz(160), 8); });
const DI<Ieee80211Hemcs> Ieee80211HemcsTable::heMcs11BW160MHzNss8([](){ return new Ieee80211Hemcs(11, &Qam1024Modulation::singleton, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6, MHz(160), 8); });

} // namespace physicallayer
} // namespace inet
