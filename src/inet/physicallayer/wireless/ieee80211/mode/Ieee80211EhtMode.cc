//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211EhtMode.h"

#include <cmath>

// IEEE 802.11be EHT packet-level SU mode definitions.
//
// This first EHT slice mirrors the existing HE packet-level mode path and
// intentionally models only single-user full-bandwidth PSDU timing. MU/TB
// metadata, puncturing, MRUs, and MLO are separate integration steps.

#include "inet/physicallayer/wireless/common/modulation/BpskModulation.h"
#include "inet/physicallayer/wireless/common/modulation/QpskModulation.h"
#include "inet/physicallayer/wireless/common/modulation/Qam16Modulation.h"
#include "inet/physicallayer/wireless/common/modulation/Qam64Modulation.h"
#include "inet/physicallayer/wireless/common/modulation/Qam256Modulation.h"
#include "inet/physicallayer/wireless/common/modulation/Qam1024Modulation.h"
#include "inet/physicallayer/wireless/common/modulation/Qam4096Modulation.h"
#include "inet/physicallayer/wireless/common/radio/bitlevel/ConvolutionalCode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmCode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtCode.h"

#define DI DelayedInitializer

namespace inet {
namespace physicallayer {

namespace {

int getNumberOfTotalSubcarriers(Hz bandwidth)
{
    if (bandwidth == MHz(20))
        return 242;
    else if (bandwidth == MHz(40))
        return 484;
    else if (bandwidth == MHz(80))
        return 996;
    else if (bandwidth == MHz(160))
        return 1992;
    else if (bandwidth == MHz(320))
        return 3984;
    else
        throw cRuntimeError("Unsupported EHT bandwidth");
}

const ApskModulationBase *getEhtMcsModulation(int mcs)
{
    // 80211be-2024:chunk:00663 maps constellation index 6 to 4096-QAM.
    // Tables 36-76, 36-77, 36-79, 36-82, and 36-86 list EHT-MCS 12/13 as
    // 4096-QAM with coding rates 3/4 and 5/6 for 242/484/996/2x996/4x996 RUs.
    switch (mcs) {
        case 0: return &BpskModulation::singleton;
        case 1: return &QpskModulation::singleton;
        case 2: return &QpskModulation::singleton;
        case 3: return &Qam16Modulation::singleton;
        case 4: return &Qam16Modulation::singleton;
        case 5: return &Qam64Modulation::singleton;
        case 6: return &Qam64Modulation::singleton;
        case 7: return &Qam64Modulation::singleton;
        case 8: return &Qam256Modulation::singleton;
        case 9: return &Qam256Modulation::singleton;
        case 10: return &Qam1024Modulation::singleton;
        case 11: return &Qam1024Modulation::singleton;
        case 12: return &Qam4096Modulation::singleton;
        case 13: return &Qam4096Modulation::singleton;
        default: throw cRuntimeError("Invalid EHT MCS: %d", mcs);
    }
}

const Ieee80211ConvolutionalCode *getEhtMcsConvolutionalCode(int mcs)
{
    switch (mcs) {
        case 0: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2;
        case 1: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2;
        case 2: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4;
        case 3: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2;
        case 4: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4;
        case 5: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode2_3;
        case 6: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4;
        case 7: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6;
        case 8: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4;
        case 9: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6;
        case 10: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4;
        case 11: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6;
        case 12: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode3_4;
        case 13: return &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode5_6;
        default: throw cRuntimeError("Invalid EHT MCS: %d", mcs);
    }
}

} // namespace

Ieee80211EhtModeBase::Ieee80211EhtModeBase(unsigned int modulationAndCodingScheme, unsigned int numberOfSpatialStreams, const Hz bandwidth, GuardIntervalType guardIntervalType) :
    bandwidth(bandwidth),
    guardIntervalType(guardIntervalType),
    mcsIndex(modulationAndCodingScheme),
    numberOfSpatialStreams(numberOfSpatialStreams),
    netBitrate(NaN),
    grossBitrate(NaN)
{
}

bps Ieee80211EhtModeBase::getNetBitrate() const
{
    if (std::isnan(netBitrate.get()))
        netBitrate = computeNetBitrate();
    return netBitrate;
}

bps Ieee80211EhtModeBase::getGrossBitrate() const
{
    if (std::isnan(grossBitrate.get()))
        grossBitrate = computeGrossBitrate();
    return grossBitrate;
}

int Ieee80211EhtModeBase::getNumberOfDataSubcarriers() const
{
    // Full-bandwidth EHT RU data subcarrier counts from:
    // 80211be-2024:chunk:01884 (242-tone), :01886 (484-tone),
    // :01890 (996-tone), :01896 (2x996-tone), :01904 (4x996-tone).
    if (bandwidth == MHz(20))
        return 234;
    else if (bandwidth == MHz(40))
        return 468;
    else if (bandwidth == MHz(80))
        return 980;
    else if (bandwidth == MHz(160))
        return 1960;
    else if (bandwidth == MHz(320))
        return 3920;
    else
        throw cRuntimeError("Unsupported EHT bandwidth");
}

int Ieee80211EhtModeBase::getNumberOfPilotSubcarriers() const
{
    if (bandwidth == MHz(20))
        return 8;
    else if (bandwidth == MHz(40))
        return 16;
    else if (bandwidth == MHz(80))
        return 16;
    else if (bandwidth == MHz(160))
        return 32;
    else if (bandwidth == MHz(320))
        return 64;
    else
        throw cRuntimeError("Unsupported EHT bandwidth");
}

Ieee80211EhtSignalMode::Ieee80211EhtSignalMode(unsigned int modulationAndCodingScheme, const Ieee80211OfdmModulation *modulation, const Ieee80211VhtCode *code, const Hz bandwidth, GuardIntervalType guardIntervalType) :
    Ieee80211EhtModeBase(modulationAndCodingScheme, 1, bandwidth, guardIntervalType),
    modulation(modulation),
    code(code)
{
}

Ieee80211EhtSignalMode::Ieee80211EhtSignalMode(unsigned int modulationAndCodingScheme, const Ieee80211OfdmModulation *modulation, const Ieee80211ConvolutionalCode *convolutionalCode, const Hz bandwidth, GuardIntervalType guardIntervalType) :
    Ieee80211EhtModeBase(modulationAndCodingScheme, 1, bandwidth, guardIntervalType),
    modulation(modulation)
{
    code = Ieee80211VhtCompliantCodes::getCompliantCode(convolutionalCode, modulation, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, bandwidth, false);
}

Ieee80211EhtSignalMode::~Ieee80211EhtSignalMode()
{
    delete code;
}

const simtime_t Ieee80211EhtSignalMode::getSymbolInterval() const
{
    switch (guardIntervalType) {
        case EHT_GUARD_INTERVAL_SHORT: return getShortGISymbolInterval();
        case EHT_GUARD_INTERVAL_MEDIUM: return getMediumGISymbolInterval();
        case EHT_GUARD_INTERVAL_LONG: return Ieee80211EhtTimingRelatedParametersBase::getSymbolInterval();
        default: throw cRuntimeError("Unknown EHT guard interval");
    }
}

bps Ieee80211EhtSignalMode::computeGrossBitrate() const
{
    unsigned int numberOfCodedBitsPerSymbol = modulation->getSubcarrierModulation()->getCodeWordSize() * getNumberOfDataSubcarriers();
    return bps(numberOfCodedBitsPerSymbol / getSymbolInterval());
}

bps Ieee80211EhtSignalMode::computeNetBitrate() const
{
    return computeGrossBitrate() * code->getForwardErrorCorrection()->getCodeRate();
}

Ieee80211EhtPreambleMode::Ieee80211EhtPreambleMode(const Ieee80211EhtSignalMode *extremelyHighThroughputSignalMode, const Ieee80211OfdmSignalMode *legacySignalMode, ExtremelyHighThroughputPreambleFormat preambleFormat, unsigned int numberOfSpatialStreams) :
    extremelyHighThroughputSignalMode(extremelyHighThroughputSignalMode),
    legacySignalMode(legacySignalMode),
    preambleFormat(preambleFormat),
    numberOfEhtLongTrainings(computeNumberOfEhtLongTrainings(numberOfSpatialStreams))
{
}

unsigned int Ieee80211EhtPreambleMode::computeNumberOfEhtLongTrainings(unsigned int numberOfSpatialStreams) const
{
    if (numberOfSpatialStreams == 1) return 1;
    if (numberOfSpatialStreams == 2) return 2;
    return 4;
}

const simtime_t Ieee80211EhtPreambleMode::getDuration() const
{
    // Packet-level EHT SU preamble approximation: legacy L-STF/L-LTF/L-SIG,
    // U-SIG, EHT-SIG, EHT-STF, then simplified 4 us EHT-LTF symbols.
    return getNonHTShortTrainingSequenceDuration() +
           getNonHTLongTrainingFieldDuration() +
           getNonHTSignalField() +
           getUniversalSignalField() +
           getEhtSignalField() +
           getEhtShortTrainingFieldDuration() +
           numberOfEhtLongTrainings * 4E-6;
}

Ieee80211Ehtmcs::Ieee80211Ehtmcs(unsigned int mcsIndex, const ApskModulationBase *stream1SubcarrierModulation, const Ieee80211ConvolutionalCode *convolutionalCode, Hz bandwidth, int nss) :
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

Ieee80211Ehtmcs::~Ieee80211Ehtmcs()
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

unsigned int Ieee80211Ehtmcs::getNumNss() const
{
    return (stream1Modulation ? 1 : 0) + (stream2Modulation ? 1 : 0) +
           (stream3Modulation ? 1 : 0) + (stream4Modulation ? 1 : 0) +
           (stream5Modulation ? 1 : 0) + (stream6Modulation ? 1 : 0) +
           (stream7Modulation ? 1 : 0) + (stream8Modulation ? 1 : 0);
}

Ieee80211EhtDataMode::Ieee80211EhtDataMode(const Ieee80211Ehtmcs *modulationAndCodingScheme, const Hz bandwidth, GuardIntervalType guardIntervalType) :
    Ieee80211EhtModeBase(modulationAndCodingScheme->getMcsIndex(), modulationAndCodingScheme->getNumNss(), bandwidth, guardIntervalType),
    modulationAndCodingScheme(modulationAndCodingScheme),
    numberOfBccEncoders(computeNumberOfBccEncoders())
{
}

const simtime_t Ieee80211EhtDataMode::getSymbolInterval() const
{
    switch (guardIntervalType) {
        case EHT_GUARD_INTERVAL_SHORT: return getShortGISymbolInterval();
        case EHT_GUARD_INTERVAL_MEDIUM: return getMediumGISymbolInterval();
        case EHT_GUARD_INTERVAL_LONG: return Ieee80211EhtTimingRelatedParametersBase::getSymbolInterval();
        default: throw cRuntimeError("Unknown EHT guard interval");
    }
}

bps Ieee80211EhtDataMode::computeGrossBitrate() const
{
    unsigned int numberOfCodedBitsPerSubcarrierSum = computeNumberOfCodedBitsPerSubcarrierSum();
    unsigned int numberOfCodedBitsPerSymbol = numberOfCodedBitsPerSubcarrierSum * getNumberOfDataSubcarriers();
    return bps(numberOfCodedBitsPerSymbol / getSymbolInterval());
}

bps Ieee80211EhtDataMode::computeNetBitrate() const
{
    return getGrossBitrate() * getCode()->getForwardErrorCorrection()->getCodeRate();
}

unsigned int Ieee80211EhtDataMode::computeNumberOfCodedBitsPerSubcarrierSum() const
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

unsigned int Ieee80211EhtDataMode::computeNumberOfBccEncoders() const
{
    return 1;
}

b Ieee80211EhtDataMode::getCompleteLength(b dataLength) const
{
    return getServiceFieldLength() + getTailFieldLength() + dataLength;
}

const simtime_t Ieee80211EhtDataMode::getDuration(b dataLength) const
{
    unsigned int numberOfCodedBitsPerSubcarrierSum = computeNumberOfCodedBitsPerSubcarrierSum();
    unsigned int numberOfCodedBitsPerSymbol = numberOfCodedBitsPerSubcarrierSum * getNumberOfDataSubcarriers();
    const IForwardErrorCorrection *forwardErrorCorrection = getCode() ? getCode()->getForwardErrorCorrection() : nullptr;
    unsigned int dataBitsPerSymbol = numberOfCodedBitsPerSymbol;
    if (auto convCode = dynamic_cast<const ConvolutionalCode *>(forwardErrorCorrection))
        dataBitsPerSymbol = numberOfCodedBitsPerSymbol * convCode->getCodeRatePuncturingK() /
                convCode->getCodeRatePuncturingN();
    else if (forwardErrorCorrection != nullptr)
        dataBitsPerSymbol = forwardErrorCorrection->getDecodedLength(numberOfCodedBitsPerSymbol);
    int numberOfSymbols = lrint(ceil((double)getCompleteLength(dataLength).get<b>() / dataBitsPerSymbol));
    return numberOfSymbols * getSymbolInterval();
}

Ieee80211EhtMode::Ieee80211EhtMode(const char *name, const Ieee80211EhtPreambleMode *preambleMode, const Ieee80211EhtDataMode *dataMode, BandMode centerFrequencyMode) :
    Ieee80211ModeBase(name),
    preambleMode(preambleMode),
    dataMode(dataMode),
    centerFrequencyMode(centerFrequencyMode)
{
}

OPP_THREAD_LOCAL const Ieee80211EhtCompliantModes Ieee80211EhtCompliantModes::singleton;

Ieee80211EhtCompliantModes::Ieee80211EhtCompliantModes()
{
}

Ieee80211EhtCompliantModes::~Ieee80211EhtCompliantModes()
{
    for (auto& entry : modeCache)
        delete entry.second;
}

const Ieee80211EhtMode *Ieee80211EhtCompliantModes::getCompliantMode(const Ieee80211Ehtmcs *mcsMode, Ieee80211EhtMode::BandMode centerFrequencyMode, Ieee80211EhtPreambleMode::ExtremelyHighThroughputPreambleFormat preambleFormat, Ieee80211EhtModeBase::GuardIntervalType guardIntervalType)
{
    const char *name = "";
    unsigned int nss = mcsMode->getNumNss();
    auto modeId = std::make_tuple(mcsMode->getBandwidth(), mcsMode->getMcsIndex(),
            guardIntervalType, nss, centerFrequencyMode, preambleFormat);
    auto mode = singleton.modeCache.find(modeId);
    if (mode == singleton.modeCache.end()) {
        const Ieee80211OfdmSignalMode *legacySignal = &Ieee80211OfdmCompliantModes::ofdmHeaderMode6MbpsRate13;
        const Ieee80211EhtSignalMode *ehtSignal = new Ieee80211EhtSignalMode(mcsMode->getMcsIndex(), &Ieee80211OfdmCompliantModulations::subcarriers52QbpskModulation, &Ieee80211OfdmCompliantCodes::ofdmConvolutionalCode1_2, mcsMode->getBandwidth(), guardIntervalType);
        const Ieee80211EhtDataMode *dataMode = new Ieee80211EhtDataMode(mcsMode, mcsMode->getBandwidth(), guardIntervalType);
        const Ieee80211EhtPreambleMode *preambleMode = new Ieee80211EhtPreambleMode(ehtSignal, legacySignal, preambleFormat, dataMode->getNumberOfSpatialStreams());
        const Ieee80211EhtMode *ehtMode = new Ieee80211EhtMode(name, preambleMode, dataMode, centerFrequencyMode);
        singleton.modeCache.insert({modeId, ehtMode});
        return ehtMode;
    }
    return mode->second;
}

#define EHT_DEFINE_MCS(WIDTH, NSS, MCS) \
const DI<Ieee80211Ehtmcs> Ieee80211EhtmcsTable::ehtMcs##MCS##BW##WIDTH##MHzNss##NSS([](){ \
    return new Ieee80211Ehtmcs(MCS, getEhtMcsModulation(MCS), getEhtMcsConvolutionalCode(MCS), MHz(WIDTH), NSS); \
});
#define EHT_DEFINE_MCS_FOR_NSS(WIDTH, NSS) \
    EHT_DEFINE_MCS(WIDTH, NSS, 0) \
    EHT_DEFINE_MCS(WIDTH, NSS, 1) \
    EHT_DEFINE_MCS(WIDTH, NSS, 2) \
    EHT_DEFINE_MCS(WIDTH, NSS, 3) \
    EHT_DEFINE_MCS(WIDTH, NSS, 4) \
    EHT_DEFINE_MCS(WIDTH, NSS, 5) \
    EHT_DEFINE_MCS(WIDTH, NSS, 6) \
    EHT_DEFINE_MCS(WIDTH, NSS, 7) \
    EHT_DEFINE_MCS(WIDTH, NSS, 8) \
    EHT_DEFINE_MCS(WIDTH, NSS, 9) \
    EHT_DEFINE_MCS(WIDTH, NSS, 10) \
    EHT_DEFINE_MCS(WIDTH, NSS, 11) \
    EHT_DEFINE_MCS(WIDTH, NSS, 12) \
    EHT_DEFINE_MCS(WIDTH, NSS, 13)
#define EHT_DEFINE_MCS_FOR_BW(WIDTH) \
    EHT_DEFINE_MCS_FOR_NSS(WIDTH, 1) \
    EHT_DEFINE_MCS_FOR_NSS(WIDTH, 2) \
    EHT_DEFINE_MCS_FOR_NSS(WIDTH, 3) \
    EHT_DEFINE_MCS_FOR_NSS(WIDTH, 4) \
    EHT_DEFINE_MCS_FOR_NSS(WIDTH, 5) \
    EHT_DEFINE_MCS_FOR_NSS(WIDTH, 6) \
    EHT_DEFINE_MCS_FOR_NSS(WIDTH, 7) \
    EHT_DEFINE_MCS_FOR_NSS(WIDTH, 8)

EHT_DEFINE_MCS_FOR_BW(20)
EHT_DEFINE_MCS_FOR_BW(40)
EHT_DEFINE_MCS_FOR_BW(80)
EHT_DEFINE_MCS_FOR_BW(160)
EHT_DEFINE_MCS_FOR_BW(320)

#undef EHT_DEFINE_MCS_FOR_BW
#undef EHT_DEFINE_MCS_FOR_NSS
#undef EHT_DEFINE_MCS

} // namespace physicallayer
} // namespace inet

#undef DI
