//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

// HE PHY parameter and duration calculator.
//
// Computes per-user and common PHY parameters for HE SU, HE ER SU, HE MU and
// HE TB PPDUs based on IEEE 802.11-2024:
//   - Clause 27.3.2: RU tone sizes, data/pilot subcarriers.
//   - Clause 27.3.4: HE PPDU formats and preamble field ordering.
//   - Clause 27.3.11.8: HE-SIG-B field sizing and content channels.
//   - Clause 27.3.12: modulation, coding, number of symbols, padding.
//   - Clause 27.3.12.5: BCC/LDPC coding rules and constraints.
//
// The FEC procedure is represented analytically: no coded bit string is
// constructed, but every standard-defined capacity, padding, shortening,
// puncturing, and repetition count is retained in the canonical result.

namespace inet {
namespace physicallayer {

namespace {

constexpr int HE_SIG_B_USER_FIELD_BITS_PER_USER = 21;
constexpr int HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK = 10;

int64_t ceilDivide(int64_t numerator, int64_t denominator)
{
    if (numerator < 0 || denominator <= 0)
        throw cRuntimeError("Invalid nonnegative ceiling division");
    return numerator / denominator + (numerator % denominator != 0);
}

int getHeShortDataSubcarrierCount(int toneSize, bool dcm)
{
    switch (toneSize) {
        case 26: return dcm ? 2 : 6;
        case 52: return dcm ? 6 : 12;
        case 106: return dcm ? 12 : 24;
        case 242: return dcm ? 30 : 60;
        case 484: return dcm ? 60 : 120;
        case 996: return dcm ? 120 : 240;
        case 1992: return dcm ? 246 : 492;
        default: throw cRuntimeError("Invalid HE RU tone size: %d", toneSize);
    }
}

int calculateHeNominalPacketExtensionDurationUs(int preFecPaddingFactor,
        int nominalPacketPaddingDurationUs)
{
    static const int values[4][3] = {
        {0, 0, 4},
        {0, 0, 8},
        {0, 4, 12},
        {0, 8, 16},
    };
    int column = nominalPacketPaddingDurationUs == 0 ? 0 :
            nominalPacketPaddingDurationUs == 8 ? 1 :
            nominalPacketPaddingDurationUs == 16 ? 2 : -1;
    if (preFecPaddingFactor < 1 || preFecPaddingFactor > 4 || column < 0)
        return -1;
    return values[preFecPaddingFactor - 1][column];
}

void setHePhyValidationError(Ieee80211HePhyValidationResult& result,
        Ieee80211HeValidationErrorCode errorCode, const char *fieldName,
        const std::string& detail, std::optional<size_t> userIndex = {},
        std::optional<size_t> physicalRuIndex = {})
{
    result.valid = false;
    result.errorCode = errorCode;
    result.context.userIndex = userIndex;
    result.context.physicalRuIndex = physicalRuIndex;
    result.context.fieldName = fieldName;
    result.context.detail = detail;
    result.error = detail;
}

template<typename Result>
bool setHeSignalingError(Result& result, Ieee80211HeSigCodecErrorCode errorCode, const char *error)
{
    result.valid = false;
    result.errorCode = errorCode;
    result.error = error;
    return false;
}

template<typename Result, typename Source>
bool copyHeSignalingError(Result& result, const Source& source)
{
    result.valid = false;
    result.errorCode = source.errorCode;
    result.error = source.error;
    return false;
}

template<typename Result>
bool resolveHeSignalExtension(const Ieee80211HeSuErSigASemantics& semantics,
        Result& result, uint32_t& signalExtensionNs)
{
    if (!semantics.noSignalExtension)
        return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_SIGNAL_EXTENSION,
                "HE SU signaling requires an explicit NO_SIG_EXTN policy");
    switch (semantics.operatingBand) {
        case Ieee80211HeOperatingBand::BAND_2_4_GHZ:
            signalExtensionNs = *semantics.noSignalExtension ? 0 : 6000;
            return true;
        case Ieee80211HeOperatingBand::BAND_5_GHZ:
        case Ieee80211HeOperatingBand::BAND_6_GHZ:
            signalExtensionNs = 0;
            return true;
        default:
            return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_SIGNAL_EXTENSION,
                    "HE SU signaling has an unknown operating band");
    }
}

template<typename Result>
bool encodeHeTxopDuration(const Ieee80211HeTxopDuration& duration, Result& result, uint8_t& raw)
{
    if (duration.unspecified) {
        if (duration.durationUs != 0)
            return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                    "unspecified HE TXOP duration must not carry a numeric duration");
        raw = 127;
        return true;
    }
    if (duration.durationUs > 8448)
        return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::FIELD_OUT_OF_RANGE,
                "HE TXOP duration exceeds 8448 us");
    raw = duration.durationUs < 512 ?
            (duration.durationUs / 8) << 1 :
            1 | ((duration.durationUs - 512) / 128) << 1;
    return true;
}

template<typename Result, typename Fields>
bool encodeHeGiLtfSelection(const Ieee80211HeSuErSigASemantics& semantics,
        Result& result, Fields& fields)
{
    if (!semantics.guardInterval || !semantics.ltfType ||
            !semantics.dcmApplied || !semantics.stbcApplied)
        return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                "HE SU signaling requires explicit GI, LTF, DCM, and STBC semantics");
    const bool dcmApplied = *semantics.dcmApplied;
    const bool stbcApplied = *semantics.stbcApplied;
    if (dcmApplied && stbcApplied)
        return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                "HE SU signaling cannot apply DCM and STBC together");

    const auto guardInterval = *semantics.guardInterval;
    const auto ltfType = *semantics.ltfType;
    if (ltfType == HE_LTF_4X && guardInterval == HE_GI_0_8_US) {
        if (dcmApplied || stbcApplied)
            return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                    "4x HE-LTF with 0.8 us GI uses the special no-DCM/no-STBC encoding");
        fields.dcm = true;
        fields.stbc = true;
        fields.giLtfSize = 3;
        return true;
    }

    if (ltfType == HE_LTF_1X && guardInterval == HE_GI_0_8_US)
        fields.giLtfSize = 0;
    else if (ltfType == HE_LTF_2X && guardInterval == HE_GI_0_8_US)
        fields.giLtfSize = 1;
    else if (ltfType == HE_LTF_2X && guardInterval == HE_GI_1_6_US)
        fields.giLtfSize = 2;
    else if (ltfType == HE_LTF_4X && guardInterval == HE_GI_3_2_US)
        fields.giLtfSize = 3;
    else
        return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                "HE SU signaling GI and HE-LTF selection is not representable");
    fields.dcm = dcmApplied;
    fields.stbc = stbcApplied;
    return true;
}

template<typename Result, typename Fields>
bool buildHeSuErCommonSignaling(const Ieee80211HeSuErSigASemantics& semantics,
        Ieee80211HeSigFormat format, Result& result, Fields& fields)
{
    if (!semantics.beamChange || !semantics.uplink || !semantics.beamformed ||
            !semantics.txopDuration || !semantics.fec)
        return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                "HE SU signaling is missing an explicit policy or FEC outcome");
    if (semantics.mcs < 0 || semantics.mcs > 11 || semantics.bssColor < 0 || semantics.bssColor > 63 ||
            semantics.spatialReuse < 0 || semantics.spatialReuse > 15 ||
            semantics.numberOfSpaceTimeStreams < 1 || semantics.numberOfSpaceTimeStreams > 8 ||
            semantics.preFecPaddingFactor < 1 || semantics.preFecPaddingFactor > 4)
        return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::FIELD_OUT_OF_RANGE,
                "HE SU signaling semantic field is out of range");
    if (semantics.packetExtensionNs != 0 && semantics.packetExtensionNs != 4000 &&
            semantics.packetExtensionNs != 8000 && semantics.packetExtensionNs != 12000 &&
            semantics.packetExtensionNs != 16000)
        return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::FIELD_OUT_OF_RANGE,
                "HE packet extension must be 0, 4000, 8000, 12000, or 16000 ns");

    uint32_t signalExtensionNs;
    if (!resolveHeSignalExtension(semantics, result, signalExtensionNs))
        return false;
    auto lSig = buildHeLSig(format, semantics.txTimeNs, signalExtensionNs);
    if (!lSig)
        return copyHeSignalingError(result, lSig);
    result.value.lSig = lSig.value;
    result.value.signalExtensionNs = signalExtensionNs;

    fields.beamChange = *semantics.beamChange;
    fields.uplink = *semantics.uplink;
    fields.mcs = semantics.mcs;
    fields.bssColor = semantics.bssColor;
    fields.spatialReuse = semantics.spatialReuse;
    fields.numberOfSpaceTimeStreams = semantics.numberOfSpaceTimeStreams;
    fields.beamformed = *semantics.beamformed;
    fields.preFecPaddingFactor = semantics.preFecPaddingFactor % 4;
    if (!encodeHeGiLtfSelection(semantics, result, fields))
        return false;
    if (!encodeHeTxopDuration(*semantics.txopDuration, result, fields.txop))
        return false;

    if (semantics.midamblePeriodicity) {
        fields.doppler = true;
        switch (*semantics.midamblePeriodicity) {
            case Ieee80211HeMidamblePeriodicity::SYMBOLS_10:
                fields.midamblePeriodicity = 10;
                break;
            case Ieee80211HeMidamblePeriodicity::SYMBOLS_20:
                fields.midamblePeriodicity = 20;
                break;
            default:
                return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                        "HE midamble periodicity is invalid");
        }
    }
    else {
        fields.doppler = false;
        fields.midamblePeriodicity = 0;
    }

    switch (semantics.fec->coding) {
        case HE_CODING_BCC:
            if (semantics.fec->ldpcExtraSymbolSegment)
                return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                        "BCC signaling must not carry an LDPC extra-symbol outcome");
            fields.ldpcCoding = false;
            fields.ldpcExtraSymbolSegment = false;
            break;
        case HE_CODING_LDPC:
            if (!semantics.fec->ldpcExtraSymbolSegment)
                return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                        "LDPC signaling requires a calculated extra-symbol outcome");
            fields.ldpcCoding = true;
            fields.ldpcExtraSymbolSegment = *semantics.fec->ldpcExtraSymbolSegment;
            break;
        default:
            return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::FIELD_OUT_OF_RANGE,
                    "HE signaling has an invalid FEC coding value");
    }

    const uint64_t elapsedNs = semantics.txTimeNs - signalExtensionNs - 20000;
    const uint64_t roundedNs = (elapsedNs / 4000 + (elapsedNs % 4000 != 0)) * 4000;
    const uint64_t roundingSlackNs = roundedNs - elapsedNs;
    uint32_t symbolDurationNs;
    switch (*semantics.guardInterval) {
        case HE_GI_0_8_US: symbolDurationNs = 13600; break;
        case HE_GI_1_6_US: symbolDurationNs = 14400; break;
        case HE_GI_3_2_US: symbolDurationNs = 16000; break;
        default:
            return setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                    "HE signaling has an invalid guard interval");
    }
    fields.peDisambiguity = semantics.packetExtensionNs + roundingSlackNs >= symbolDurationNs;
    return true;
}

int getHeSigBUserBlockCount(int numberOfUsers)
{
    return (numberOfUsers + 1) / 2;
}

bool samePhysicalHeRu(const Ieee80211HeRu& left, const Ieee80211HeRu& right)
{
    bool sameCenterFrequency = left.centerFrequency == right.centerFrequency ||
            (std::isnan(left.centerFrequency.get()) && std::isnan(right.centerFrequency.get()));
    return sameCenterFrequency && left.toneSize == right.toneSize &&
            left.toneOffset == right.toneOffset;
}

bool isHeLtfGiCombinationAllowed(Ieee80211HePpduFormat ppduFormat,
        Ieee80211HeLtfType ltfType, Ieee80211HeGuardInterval guardInterval,
        bool isFeedbackNdp, bool isFullBandwidthUlMuMimo)
{
    if (ppduFormat == HE_SINGLE_USER || ppduFormat == HE_EXTENDED_RANGE_SU)
        return (ltfType == HE_LTF_1X && guardInterval == HE_GI_0_8_US) ||
                (ltfType == HE_LTF_2X && (guardInterval == HE_GI_0_8_US || guardInterval == HE_GI_1_6_US)) ||
                (ltfType == HE_LTF_4X && (guardInterval == HE_GI_0_8_US || guardInterval == HE_GI_3_2_US));
    if (ppduFormat == HE_MU_DOWNLINK)
        return (ltfType == HE_LTF_2X && (guardInterval == HE_GI_0_8_US || guardInterval == HE_GI_1_6_US)) ||
                (ltfType == HE_LTF_4X && (guardInterval == HE_GI_0_8_US || guardInterval == HE_GI_3_2_US));
    if (isFeedbackNdp)
        return ltfType == HE_LTF_4X && guardInterval == HE_GI_3_2_US;
    return (ltfType == HE_LTF_2X && guardInterval == HE_GI_1_6_US) ||
            (ltfType == HE_LTF_4X && guardInterval == HE_GI_3_2_US) ||
            (isFullBandwidthUlMuMimo && ltfType == HE_LTF_1X && guardInterval == HE_GI_1_6_US);
}

} // namespace

Ieee80211HeLdpcCalculationResult computeHeLdpcParameters(int64_t payloadBits,
        int64_t availableBits, int rateNumerator, int rateDenominator)
{
    using Wide = __int128;
    Ieee80211HeLdpcCalculationResult result;
    const bool supportedRate =
            (rateNumerator == 1 && rateDenominator == 2) ||
            (rateNumerator == 2 && rateDenominator == 3) ||
            (rateNumerator == 3 && rateDenominator == 4) ||
            (rateNumerator == 5 && rateDenominator == 6);
    if (payloadBits <= 0 || availableBits <= 0 || !supportedRate) {
        result.error = "invalid HE LDPC payload, capacity, or code rate";
        return result;
    }
    if (availableBits <= 648) {
        result.codewordCount = 1;
        result.codewordLength = (Wide)availableBits * rateDenominator >=
                (Wide)payloadBits * rateDenominator + 912LL * (rateDenominator - rateNumerator) ? 1296 : 648;
    }
    else if (availableBits <= 1296) {
        result.codewordCount = 1;
        result.codewordLength = (Wide)availableBits * rateDenominator >=
                (Wide)payloadBits * rateDenominator + 1464LL * (rateDenominator - rateNumerator) ? 1944 : 1296;
    }
    else if (availableBits <= 1944) {
        result.codewordCount = 1;
        result.codewordLength = 1944;
    }
    else if (availableBits <= 2592) {
        result.codewordCount = 2;
        result.codewordLength = (Wide)availableBits * rateDenominator >=
                (Wide)payloadBits * rateDenominator + 2916LL * (rateDenominator - rateNumerator) ? 1944 : 1296;
    }
    else {
        result.codewordLength = 1944;
        const Wide numerator = (Wide)payloadBits * rateDenominator;
        const Wide denominator = 1944LL * rateNumerator;
        const Wide count = numerator / denominator + (numerator % denominator != 0);
        if (count > std::numeric_limits<int>::max()) {
            result.error = "HE PSDU requires too many LDPC codewords";
            return result;
        }
        result.codewordCount = static_cast<int>(count);
    }
    const Wide codedBits = (Wide)result.codewordCount * result.codewordLength;
    const Wide informationCapacity = codedBits * rateNumerator / rateDenominator;
    if ((Wide)payloadBits > informationCapacity) {
        result.error = "HE LDPC payload exceeds the selected information capacity";
        return result;
    }
    const Wide shortening = std::max<Wide>(0, informationCapacity - (Wide)payloadBits);
    const Wide puncturing = std::max<Wide>(0,
            codedBits - (Wide)availableBits - shortening);
    if (informationCapacity > std::numeric_limits<int64_t>::max() ||
            shortening > std::numeric_limits<int>::max() ||
            puncturing > std::numeric_limits<int>::max()) {
        result.error = "HE LDPC bit counts exceed the model range";
        return result;
    }
    result.shorteningBits = shortening;
    result.puncturingBits = puncturing;
    const Wide parityRate = rateDenominator - rateNumerator;
    const Wide scaledPuncturing = (Wide)10 * rateDenominator * result.puncturingBits;
    result.primaryExtraSymbolCondition =
            scaledPuncturing > codedBits * parityRate &&
            (Wide)5 * parityRate * result.shorteningBits <
                    (Wide)6 * rateNumerator * result.puncturingBits;
    result.extremePuncturingCondition =
            scaledPuncturing > (Wide)3 * codedBits * parityRate;
    result.extraSymbolRequired = result.primaryExtraSymbolCondition ||
            result.extremePuncturingCondition;
    result.valid = true;
    return result;
}

Ieee80211HeSuSignalingResult buildIeee80211HeSuSignaling(const Ieee80211HeSuSignalingRequest& request)
{
    Ieee80211HeSuSignalingResult result;
    auto& fields = result.value.sigA;
    switch (request.bandwidth) {
        case Ieee80211HeSuBandwidth::MHZ_20: fields.bandwidth = 0; break;
        case Ieee80211HeSuBandwidth::MHZ_40: fields.bandwidth = 1; break;
        case Ieee80211HeSuBandwidth::MHZ_80: fields.bandwidth = 2; break;
        case Ieee80211HeSuBandwidth::MHZ_160:
        case Ieee80211HeSuBandwidth::MHZ_80P80:
            fields.bandwidth = 3;
            break;
        default:
            setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::FIELD_OUT_OF_RANGE,
                    "HE SU signaling has an invalid bandwidth selection");
            return result;
    }
    if (!buildHeSuErCommonSignaling(request.common, Ieee80211HeSigFormat::SU, result, fields))
        return result;
    auto encoded = encodeHeSuSigA(fields);
    if (!encoded) {
        copyHeSignalingError(result, encoded);
        return result;
    }
    result.valid = true;
    result.errorCode = Ieee80211HeSigCodecErrorCode::NONE;
    result.error.clear();
    return result;
}

Ieee80211HeErSuSignalingResult buildIeee80211HeErSuSignaling(const Ieee80211HeErSuSignalingRequest& request)
{
    Ieee80211HeErSuSignalingResult result;
    auto& fields = result.value.sigA;
    switch (request.ruMode) {
        case Ieee80211HeErSuRuMode::PRIMARY_242_TONE: fields.bandwidth = 0; break;
        case Ieee80211HeErSuRuMode::PRIMARY_UPPER_106_TONE: fields.bandwidth = 1; break;
        default:
            setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::FIELD_OUT_OF_RANGE,
                    "HE ER SU signaling has an invalid RU mode");
            return result;
    }
    if (!buildHeSuErCommonSignaling(request.common, Ieee80211HeSigFormat::ER_SU, result, fields))
        return result;
    auto encoded = encodeHeErSuSigA(fields);
    if (!encoded) {
        copyHeSignalingError(result, encoded);
        return result;
    }
    result.valid = true;
    result.errorCode = Ieee80211HeSigCodecErrorCode::NONE;
    result.error.clear();
    return result;
}

Ieee80211HeLSigResult buildIeee80211HeTriggerUlLength(simtime_t txTime, uint32_t signalExtensionNs)
{
    if (txTime <= SIMTIME_ZERO) {
        Ieee80211HeLSigResult result;
        result.errorCode = Ieee80211HeSigCodecErrorCode::INVALID_TXTIME;
        result.error = "HE Trigger UL Length requires a positive solicited HE TB TXTIME";
        return result;
    }
    auto txTimeNs = txTime.inUnit(SIMTIME_NS);
    if (SimTime(txTimeNs, SIMTIME_NS) != txTime) {
        Ieee80211HeLSigResult result;
        result.errorCode = Ieee80211HeSigCodecErrorCode::INVALID_TXTIME;
        result.error = "HE Trigger UL Length TXTIME is not an exact integer number of nanoseconds";
        return result;
    }
    // Equation 27-11 uses m=2 for the solicited HE TB duration projection.
    // Once selected, the resulting integral value is carried independently as
    // Trigger UL Length and TXVECTOR L_LENGTH; serializers never infer it from
    // runtime duration metadata.
    return buildHeLSig(Ieee80211HeSigFormat::SU, txTimeNs, signalExtensionNs);
}

Ieee80211HeTxTimeResult getIeee80211HeTriggerTxTimeUpperBound(
        uint16_t lLength, uint32_t signalExtensionNs)
{
    Ieee80211HeTxTimeResult result;
    if (signalExtensionNs != 0 && signalExtensionNs != 6000) {
        setHeSignalingError(result, Ieee80211HeSigCodecErrorCode::INVALID_SIGNAL_EXTENSION,
                "HE Trigger signal extension must be 0 us or 6 us");
        return result;
    }
    auto lSig = buildHeTbLSig(lLength);
    if (!lSig) {
        setHeSignalingError(result, lSig.errorCode, lSig.error.c_str());
        return result;
    }
    uint64_t q = (lLength + 5) / 3;
    result.txTime = SimTime(20000 + 4000 * q + signalExtensionNs, SIMTIME_NS);
    result.valid = true;
    result.errorCode = Ieee80211HeSigCodecErrorCode::NONE;
    return result;
}

uint32_t getIeee80211HeSignalExtensionNs(Ieee80211HeOperatingBand operatingBand,
        bool noSignalExtension)
{
    switch (operatingBand) {
        case Ieee80211HeOperatingBand::BAND_2_4_GHZ:
            return noSignalExtension ? 0 : 6000;
        case Ieee80211HeOperatingBand::BAND_5_GHZ:
        case Ieee80211HeOperatingBand::BAND_6_GHZ:
            return 0;
        default:
            throw cRuntimeError("HE signal-extension selection requires a known operating band");
    }
}

uint32_t getIeee80211HeSignalExtensionNs(Hz centerFrequency, bool noSignalExtension)
{
    if (!std::isfinite(centerFrequency.get()) || centerFrequency <= Hz(0))
        throw cRuntimeError("HE signal-extension selection requires a positive finite center frequency");
    // HE operation below 3 GHz is the 2.4 GHz operating band. Table 27-61
    // applies the 6 us signal extension there unless TXVECTOR NO_SIG_EXTN is set.
    return getIeee80211HeSignalExtensionNs(centerFrequency < Hz(3e9) ?
            Ieee80211HeOperatingBand::BAND_2_4_GHZ :
            Ieee80211HeOperatingBand::BAND_5_GHZ, noSignalExtension);
}

int getHeRuDataSubcarrierCount(int toneSize)
{
    switch (toneSize) {
        case 26: return 24;
        case 52: return 48;
        case 106: return 102;
        case 242: return 234;
        case 484: return 468;
        case 996: return 980;
        case 1992: return 1960;
        default: throw std::invalid_argument("Unsupported IEEE 802.11ax RU tone size");
    }
}

int getHeRuPilotSubcarrierCount(int toneSize)
{
    switch (toneSize) {
        case 26: return 2;
        case 52: return 4;
        case 106: return 4;
        case 242: return 8;
        case 484: return 16;
        case 996: return 16;
        case 1992: return 32;
        default: throw std::invalid_argument("Unsupported IEEE 802.11ax RU tone size");
    }
}

static int getHeSigBDataBitsPerSymbol(int mcs, bool dcm)
{
    static const int noDcm[] = {26, 52, 78, 104, 156, 208};
    static const int withDcm[] = {13, 26, 0, 52, 78, 0};
    if (mcs < 0 || mcs > 5 || (dcm && withDcm[mcs] == 0))
        throw cRuntimeError("Invalid HE-SIG-B MCS/DCM combination");
    return dcm ? withDcm[mcs] : noDcm[mcs];
}

int getHeSigBSymbolCount(Hz channelBandwidth, int numberOfUsers,
        bool compression, int mcs, bool dcm)
{
    int widthMhz = std::lround(channelBandwidth.get() / 1e6);
    int contentChannels = getHeSigBContentChannelCount(channelBandwidth);
    int twentyMhzChannels = widthMhz / 20;
    int commonBitsPerContentChannel = compression ? 0 :
            8 * ((twentyMhzChannels + contentChannels - 1) / contentChannels) +
            (channelBandwidth > MHz(40) ? 1 : 0) + HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK;
    int usersPerContentChannel = (numberOfUsers + contentChannels - 1) / contentChannels;
    int userBitsPerContentChannel = usersPerContentChannel * HE_SIG_B_USER_FIELD_BITS_PER_USER
            + getHeSigBUserBlockCount(usersPerContentChannel) * HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK;
    int dataBitsPerSymbol = getHeSigBDataBitsPerSymbol(mcs, dcm);
    return std::max(1, (commonBitsPerContentChannel + userBitsPerContentChannel
            + dataBitsPerSymbol - 1) / dataBitsPerSymbol);
}

int getHeSigBSymbolCount(const Ieee80211HeSigBCommonField& commonField,
        Hz channelBandwidth, int mcs, bool dcm)
{
    int dataBitsPerSymbol = getHeSigBDataBitsPerSymbol(mcs, dcm);
    size_t maximumBits = 0;
    for (const auto& channel : commonField.contentChannels) {
        size_t commonBits = channel.ruAllocationSubfields.size() * 8 +
                (channelBandwidth > MHz(40) ? 1 : 0) +
                HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK;
        size_t users = channel.plannedUsers.size();
        size_t userBits = users * HE_SIG_B_USER_FIELD_BITS_PER_USER +
                getHeSigBUserBlockCount(users) * HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK;
        maximumBits = std::max(maximumBits, commonBits + userBits);
    }
    return std::max<int>(1, (maximumBits + dataBitsPerSymbol - 1) / dataBitsPerSymbol);
}

Ieee80211HePhyValidationResult computeHePpduParameters(
        const std::vector<Ieee80211HeUserPhyParameters>& requestedUsers,
        Hz channelBandwidth,
        Ieee80211HePpduFormat ppduFormat,
        Ieee80211HeGuardInterval guardInterval,
        Ieee80211HeLtfType ltfType,
        int packetExtensionDurationUs,
        bool enforceDurationLimit,
        const std::optional<Ieee80211HeTbCalculationContext>& tbContext)
{
    Ieee80211HePhyValidationResult result;
    if (ppduFormat != HE_MU_DOWNLINK && ppduFormat != HE_TRIGGER_BASED_UPLINK &&
            ppduFormat != HE_SINGLE_USER && ppduFormat != HE_EXTENDED_RANGE_SU) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_PPDU_FORMAT,
                "ppduFormat", "invalid HE PPDU format");
        return result;
    }
    if (channelBandwidth != MHz(20) && channelBandwidth != MHz(40) &&
            channelBandwidth != MHz(80) && channelBandwidth != MHz(160)) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_CHANNEL_BANDWIDTH,
                "channelBandwidth", "unsupported HE channel bandwidth");
        return result;
    }
    if (guardInterval != HE_GI_0_8_US && guardInterval != HE_GI_1_6_US &&
            guardInterval != HE_GI_3_2_US) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_GUARD_INTERVAL,
                "guardInterval", "invalid HE guard interval");
        return result;
    }
    if (ltfType != HE_LTF_1X && ltfType != HE_LTF_2X && ltfType != HE_LTF_4X) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_LTF_TYPE,
                "ltfType", "invalid HE-LTF type");
        return result;
    }
    try {
        getHeLtfSymbolDuration(ltfType, guardInterval);
    }
    catch (const omnetpp::cRuntimeError& error) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_GI_LTF_COMBINATION,
                "guardInterval/ltfType", error.what());
        return result;
    }
    if (packetExtensionDurationUs != 0 && packetExtensionDurationUs != 4 &&
            packetExtensionDurationUs != 8 && packetExtensionDurationUs != 12 && packetExtensionDurationUs != 16) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_PACKET_EXTENSION,
                "packetExtensionDurationUs", "invalid HE packet extension duration");
        return result;
    }
    if (tbContext) {
        if (ppduFormat != HE_TRIGGER_BASED_UPLINK ||
                tbContext->triggerMethod != Ieee80211HeTriggerMethod::TRIGGER_FRAME ||
                tbContext->ulLength > 4095 || tbContext->ulLength % 3 != 1 ||
                tbContext->preFecPaddingFactor < 1 || tbContext->preFecPaddingFactor > 4 ||
                (tbContext->numberOfHeLtfSymbols != 1 && tbContext->numberOfHeLtfSymbols != 2 &&
                 tbContext->numberOfHeLtfSymbols != 4 && tbContext->numberOfHeLtfSymbols != 6 &&
                 tbContext->numberOfHeLtfSymbols != 8)) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                    "tbContext", "invalid or unsupported HE-TB Trigger-frame calculation context");
            return result;
        }
    }
    if (requestedUsers.empty()) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::EMPTY_USER_LIST,
                "users", "HE PPDU has no users");
        return result;
    }
    if ((ppduFormat == HE_SINGLE_USER || ppduFormat == HE_EXTENDED_RANGE_SU) &&
            requestedUsers.size() != 1) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_USER_COUNT,
                "users", "HE SU and HE ER SU PPDUs require exactly one user");
        return result;
    }

    // Validate all externally supplied scalar values before calling the
    // throwing calculation helpers below. This API is also used as a public
    // feasibility check by schedulers, so ordinary malformed input must be
    // reported in the result in both release and debug builds. Allocation
    // failures are not part of that malformed-input guarantee.
    for (size_t userIndex = 0; userIndex < requestedUsers.size(); userIndex++) {
        const auto& requested = requestedUsers[userIndex];
        if (requested.mcs < 0 || requested.mcs > 11) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_MCS,
                    "mcs", "invalid HE MCS", userIndex);
            return result;
        }
        if (requested.ru.toneSize != 26 && requested.ru.toneSize != 52 &&
                requested.ru.toneSize != 106 && requested.ru.toneSize != 242 &&
                requested.ru.toneSize != 484 && requested.ru.toneSize != 996 &&
                requested.ru.toneSize != 1992) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                    "ru.toneSize", "unsupported HE RU tone size", userIndex);
            return result;
        }
        const int expectedDataSubcarriers = getHeRuDataSubcarrierCount(requested.ru.toneSize);
        if (requested.ru.dataSubcarriers != 0 &&
                requested.ru.dataSubcarriers != expectedDataSubcarriers) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                    "ru.dataSubcarriers", "HE RU data-subcarrier count disagrees with its tone size", userIndex);
            return result;
        }
        const int expectedPilotSubcarriers = getHeRuPilotSubcarrierCount(requested.ru.toneSize);
        if (requested.ru.pilotSubcarriers != 0 &&
                requested.ru.pilotSubcarriers != expectedPilotSubcarriers) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                    "ru.pilotSubcarriers", "HE RU pilot-subcarrier count disagrees with its tone size", userIndex);
            return result;
        }
        if (requested.numberOfSpatialStreams < 1 || requested.numberOfSpatialStreams > 8) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_SPATIAL_STREAMS,
                    "numberOfSpatialStreams", "invalid HE number of spatial streams", userIndex);
            return result;
        }
        if (requested.streamStartIndex < 0 ||
                requested.streamStartIndex > 8 - requested.numberOfSpatialStreams) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_STREAM_MAPPING,
                    "streamStartIndex", "HE RU spatial stream range exceeds 8 streams", userIndex);
            return result;
        }
        if (requested.staId > 2047) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_STA_ID,
                    "staId", "HE STA ID exceeds the 11-bit field width", userIndex);
            return result;
        }
        if (requested.coding != HE_CODING_BCC && requested.coding != HE_CODING_LDPC) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_CODING,
                    "coding", "invalid HE coding type", userIndex);
            return result;
        }
        if (requested.psduLength < B(0)) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_PSDU_LENGTH,
                    "psduLength", "negative HE PSDU length", userIndex);
            return result;
        }
        if (requested.nominalPacketPaddingDurationUs != 0 &&
                requested.nominalPacketPaddingDurationUs != 8 &&
                requested.nominalPacketPaddingDurationUs != 16) {
            setHePhyValidationError(result,
                    Ieee80211HeValidationErrorCode::INVALID_NOMINAL_PACKET_PADDING,
                    "nominalPacketPaddingDurationUs",
                    "HE nominal packet padding must be 0, 8, or 16 us", userIndex);
            return result;
        }
    }

    const auto& singleUser = requestedUsers.front();
    if (ppduFormat == HE_SINGLE_USER &&
            (singleUser.ru.toneSize != getHeChannelToneCount(channelBandwidth) ||
             singleUser.ru.toneOffset != 0)) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                "ru", "HE SU requires the full-channel RU allocation", 0);
        return result;
    }
    if (ppduFormat == HE_EXTENDED_RANGE_SU) {
        if (channelBandwidth != MHz(20)) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_CHANNEL_BANDWIDTH,
                    "channelBandwidth", "HE ER SU is modeled only for 20 MHz channels");
            return result;
        }
        const bool primary242Tone = singleUser.ru.toneSize == 242 && singleUser.ru.toneOffset == 0;
        const bool primaryUpper106Tone = singleUser.ru.toneSize == 106 && singleUser.ru.toneOffset == 136;
        if (!primary242Tone && !primaryUpper106Tone) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                    "ru", "HE ER SU requires the primary 242-tone or primary upper 106-tone RU", 0);
            return result;
        }
        if ((primary242Tone && singleUser.mcs > 2) ||
                (primaryUpper106Tone && singleUser.mcs != 0)) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_MCS,
                    "mcs", "HE ER SU MCS is not valid for the selected RU mode", 0);
            return result;
        }
    }

    // Group users by physical RU identity to detect and validate MU-MIMO.
    // The model-local RU index is deliberately excluded: equivalent physical
    // RUs from different allocation catalogs may have different indices, and
    // the same numeric index may identify RUs in different channel segments.
    // IEEE 802.11-2024 Clause 27.3.11.8 links HE-SIG-B User fields to an RU
    // allocation; Clause 27.3.12.5 constrains per-user and per-RU FEC choices.
    // Validates standard spatial stream limits:
    // - Maximum MU-MIMO group size is 8 users.
    // - Maximum spatial streams per user is 4.
    // - Total spatial streams (N_STS) in a group cannot exceed 8.
    // - User spatial streams must be contiguous (no gaps or overlapping indices).
    std::vector<std::pair<Ieee80211HeRu, std::vector<Ieee80211HeUserPhyParameters>>> ruGroups;
    for (const auto& requested : requestedUsers) {
        auto group = std::find_if(ruGroups.begin(), ruGroups.end(),
                [&] (const auto& candidate) { return samePhysicalHeRu(candidate.first, requested.ru); });
        if (group == ruGroups.end())
            ruGroups.push_back({requested.ru, {requested}});
        else
            group->second.push_back(requested);
    }
    int maximumSpaceTimeStreamsPerRu = 0;
    for (size_t physicalRuIndex = 0; physicalRuIndex < ruGroups.size(); physicalRuIndex++) {
        const auto& group = ruGroups[physicalRuIndex].second;
        int groupTotalNsts = 0;
        for (const auto& user : group)
            groupTotalNsts += user.numberOfSpatialStreams;
        if (groupTotalNsts > 8) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_STREAM_MAPPING,
                    "numberOfSpatialStreams", "HE MU-MIMO group total spatial streams exceeds 8",
                    {}, physicalRuIndex);
            return result;
        }
        maximumSpaceTimeStreamsPerRu = std::max(maximumSpaceTimeStreamsPerRu, groupTotalNsts);
        if (group.size() > 1) {
            if (group.size() > 8) {
                setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_STREAM_MAPPING,
                        "users", "HE MU-MIMO group has too many users (max 8)",
                        {}, physicalRuIndex);
                return result;
            }
            std::set<uint16_t> staIds;
            std::vector<std::pair<int, int>> streams; // {startIndex, nss}
            for (const auto& user : group) {
                if (staIds.count(user.staId) > 0) {
                    setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_STREAM_MAPPING,
                            "staId", "HE MU-MIMO group contains duplicate STA IDs",
                            {}, physicalRuIndex);
                    return result;
                }
                staIds.insert(user.staId);
                if (user.numberOfSpatialStreams > 4) {
                    setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_SPATIAL_STREAMS,
                            "numberOfSpatialStreams", "HE MU-MIMO user cannot have more than 4 spatial streams",
                            {}, physicalRuIndex);
                    return result;
                }
                streams.push_back({user.streamStartIndex, user.numberOfSpatialStreams});
            }
            std::sort(streams.begin(), streams.end());
            int expectedStart = 0;
            for (const auto& stream : streams) {
                if (stream.first != expectedStart) {
                    setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_STREAM_MAPPING,
                            "streamStartIndex", "HE MU-MIMO spatial streams are not contiguous or have gaps/overlaps",
                            {}, physicalRuIndex);
                    return result;
                }
                expectedStart += stream.second;
            }
        }
    }


    bool isFeedbackNdp = ppduFormat == HE_TRIGGER_BASED_UPLINK &&
            std::all_of(requestedUsers.begin(), requestedUsers.end(),
                    [] (const auto& requested) { return requested.psduLength == B(0); });
    if (tbContext && !isFeedbackNdp && tbContext->numberOfHeLtfSymbols <
            getHeNumberOfLtfSymbols(maximumSpaceTimeStreamsPerRu)) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                "tbContext.numberOfHeLtfSymbols",
                "Trigger HE-LTF count is below the minimum required by the solicited spatial streams");
        return result;
    }
    bool isFullBandwidthUlMuMimo = ppduFormat == HE_TRIGGER_BASED_UPLINK &&
            ruGroups.size() == 1 && ruGroups.front().second.size() >= 2 &&
            ruGroups.front().first.toneSize == getHeChannelToneCount(channelBandwidth);
    // IEEE 802.11-2024 Table 27-32 marks several otherwise meaningful
    // HE-LTF/GI duration pairs N/A for particular PPDU formats. CM3 permits
    // 1x HE-LTF with 1.6 us GI only for full-bandwidth UL MU-MIMO.
    if (!isHeLtfGiCombinationAllowed(ppduFormat, ltfType, guardInterval,
            isFeedbackNdp, isFullBandwidthUlMuMimo)) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_GI_LTF_COMBINATION,
                "guardInterval/ltfType", "HE-LTF/GI combination is not supported for the HE PPDU format");
        return result;
    }

    auto& parameters = result.parameters;
    parameters.common.ppduFormat = ppduFormat;
    parameters.common.channelBandwidth = channelBandwidth;
    parameters.common.guardInterval = guardInterval;
    parameters.common.ltfType = ltfType;
    parameters.common.packetExtensionDurationUs = packetExtensionDurationUs;
    parameters.common.heSigADuration = ppduFormat == HE_EXTENDED_RANGE_SU ?
            SimTime(16, SIMTIME_US) : SimTime(8, SIMTIME_US);
    parameters.common.sigA.ppduFormat = ppduFormat;
    parameters.common.sigA.uplink = ppduFormat == HE_TRIGGER_BASED_UPLINK;
    parameters.common.sigB.compression = ppduFormat == HE_MU_DOWNLINK &&
            ruGroups.size() == 1 && ruGroups.front().first.toneSize == 1992;
    parameters.common.sigB.numberOfSymbols = 0;
    if (ppduFormat == HE_MU_DOWNLINK) {
        int contentChannels = getHeSigBContentChannelCount(channelBandwidth);
        int numberOfUsers = requestedUsers.size();
        if (parameters.common.sigB.compression) {
            parameters.common.sigB.numberOfSymbols = getHeSigBSymbolCount(channelBandwidth,
                    numberOfUsers, true, parameters.common.sigB.mcs, false);
            parameters.common.sigB.commonFieldBits = 0;
            parameters.common.sigB.userFieldBits =
                    HE_SIG_B_USER_FIELD_BITS_PER_USER * numberOfUsers;
            int usersPerContentChannel = numberOfUsers / contentChannels;
            int channelsWithExtraUser = numberOfUsers % contentChannels;
            for (int channel = 0; channel < contentChannels; channel++) {
                int channelUsers = usersPerContentChannel + (channel < channelsWithExtraUser ? 1 : 0);
                parameters.common.sigB.userFieldBits +=
                        getHeSigBUserBlockCount(channelUsers) * HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK;
            }
        }
        else {
            std::vector<Ieee80211HeRu> rus;
            for (const auto& user : requestedUsers)
                rus.push_back(user.ru);
            auto commonField = encodeHeSigBCommonField(rus, channelBandwidth);
            if (!commonField) {
                setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_RU_LAYOUT,
                        "users[].ru", commonField.error);
                return result;
            }
            parameters.common.sigB.numberOfSymbols = getHeSigBSymbolCount(
                    commonField.commonField, channelBandwidth, parameters.common.sigB.mcs, false);
            parameters.common.sigB.commonFieldBits = 0;
            parameters.common.sigB.userFieldBits = 0;
            for (const auto& channel : commonField.commonField.contentChannels) {
                parameters.common.sigB.commonFieldBits += channel.ruAllocationSubfields.size() * 8 +
                        (channelBandwidth > MHz(40) ? 1 : 0) + HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK;
                parameters.common.sigB.userFieldBits +=
                        channel.plannedUsers.size() * HE_SIG_B_USER_FIELD_BITS_PER_USER +
                        getHeSigBUserBlockCount(channel.plannedUsers.size()) *
                                HE_SIG_B_CRC_AND_TAIL_BITS_PER_BLOCK;
            }
        }
    }
    parameters.common.heSigBDuration =
            parameters.common.sigB.numberOfSymbols * SimTime(4, SIMTIME_US);

    if (isFeedbackNdp) {
        parameters.common.numberOfHeLtfSymbols = 2;
        parameters.common.heStfDuration = SimTime(8, SIMTIME_US);
    } else {
        // Deterministic calculator policy: select the minimum legal HE-LTF
        // count for the maximum initial N_STS on any physical RU. This avoids
        // summing concurrent OFDMA RUs; it is not a standard requirement that
        // a transmitter always choose exactly this HE-LTF count.
        parameters.common.numberOfHeLtfSymbols = getHeNumberOfLtfSymbols(maximumSpaceTimeStreamsPerRu);
        parameters.common.heStfDuration = ppduFormat == HE_TRIGGER_BASED_UPLINK ?
                SimTime(8, SIMTIME_US) : SimTime(4, SIMTIME_US);
    }
    if (tbContext && !isFeedbackNdp)
        parameters.common.numberOfHeLtfSymbols = tbContext->numberOfHeLtfSymbols;
    parameters.common.heLtfDuration =
            parameters.common.numberOfHeLtfSymbols * getHeLtfSymbolDuration(ltfType, guardInterval);
    parameters.common.commonPreambleDuration =
            parameters.common.legacyPreambleDuration +
            parameters.common.rlSigDuration +
            parameters.common.heSigADuration +
            parameters.common.heSigBDuration +
            parameters.common.heStfDuration +
            parameters.common.heLtfDuration;

    const auto symbolDuration = SimTime(12800, SIMTIME_NS) + getHeGuardIntervalDuration(guardInterval);
    for (size_t userIndex = 0; userIndex < requestedUsers.size(); userIndex++) {
        const auto& requested = requestedUsers[userIndex];
        auto user = requested;
        user.ru.dataSubcarriers = getHeRuDataSubcarrierCount(user.ru.toneSize);
        user.ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(user.ru.toneSize);
        // IEEE Std 802.11-2024 Clause 27.3.12.5 ("Coding"):
        // "LDPC is the only FEC coding scheme in the HE PPDU Data field for a 484-, 996-, and 2x996-tone RU."
        // "LDPC is the only FEC coding scheme in the HE PPDU Data field for HE-MCSs 10 and 11."
        // "Support for BCC coding is limited to less than or equal to four spatial streams..."
        if (user.coding == HE_CODING_BCC) {
            if (user.mcs == 10 || user.mcs == 11) {
                setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_FEC_COMBINATION,
                        "coding", "HE BCC coding is not supported for MCS 10 or 11", userIndex);
                return result;
            }
            // The LDPC-only rule applies to the HE PPDU Data field. An HE TB
            // feedback NDP has APEP_LENGTH=0 and no Data field; 26.5.7.2
            // explicitly requires BCC while assigning the maximum RU.
            if (user.ru.toneSize >= 484 && user.psduLength != B(0)) {
                setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_FEC_COMBINATION,
                        "coding", "HE BCC coding is not supported for 484-tone RUs or larger", userIndex);
                return result;
            }
            if (user.numberOfSpatialStreams > 4) {
                setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_FEC_COMBINATION,
                        "coding", "HE BCC coding is limited to less than or equal to 4 spatial streams", userIndex);
                return result;
            }
        }
        if (user.dcm && !isHeDcmCombinationSupported(user.mcs, user.numberOfSpatialStreams)) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_DCM_COMBINATION,
                    "dcm", "unsupported HE DCM combination", userIndex);
            return result;
        }
        // IEEE 802.11-2024 Tables 27-62..27-117: reject N/A (MCS, Nss, RU) triples.
        if (!isHeValidMcsNssCombination(user.mcs, user.numberOfSpatialStreams, user.ru.toneSize)) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_MCS_NSS_COMBINATION,
                    "mcs/numberOfSpatialStreams",
                    std::string("HE MCS ") + std::to_string(user.mcs)
                            + ", Nss=" + std::to_string(user.numberOfSpatialStreams)
                            + ", RU=" + std::to_string(user.ru.toneSize)
                            + "-tone is N/A per IEEE 802.11-2024 Tables 27-62..27-117",
                    userIndex);
            return result;
        }
        const int dataSubcarriers = user.ru.dataSubcarriers;
        const auto codeRate = getHeMcsCodeRate(user.mcs);
        const int bitsPerSubcarrier = getHeMcsBitsPerSubcarrier(user.mcs);
        user.guardInterval = guardInterval;
        // IEEE 802.11-2024 data-rate tables apply DCM by halving N_SD before
        // calculating N_CBPS. This matters for odd effective capacities such
        // as 106-tone MCS 0 DCM (N_CBPS=51).
        const int effectiveDataSubcarriers = user.dcm ? dataSubcarriers / 2 : dataSubcarriers;
        user.codedBitsPerSymbol = effectiveDataSubcarriers * bitsPerSubcarrier *
                user.numberOfSpatialStreams;
        user.dataBitsPerSymbol = user.codedBitsPerSymbol * codeRate.first / codeRate.second;
        user.shortDataSubcarriers = getHeShortDataSubcarrierCount(user.ru.toneSize, user.dcm);
        user.shortCodedBitsPerSymbol = user.shortDataSubcarriers * bitsPerSubcarrier *
                user.numberOfSpatialStreams;
        user.shortDataBitsPerSymbol = user.shortCodedBitsPerSymbol *
                codeRate.first / codeRate.second;
        if (user.psduLength == B(0)) {
            user.numberOfDataSymbols = 0;
            user.numberOfSymbols = 0;
            user.preFecPaddingFactor = 0;
            user.postFecPaddingBits = 0;
            user.ldpcCodewordLength = 0;
            user.ldpcCodewordCount = 0;
            user.ldpcShorteningBits = 0;
            user.ldpcPuncturingBits = 0;
            user.ldpcRepetitionBits = 0;
            user.numberOfEncoders = 0;
            user.tailBits = 0;
            parameters.users.push_back(user);
            continue;
        }
        if (user.dataBitsPerSymbol <= 0) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_DATA_RATE,
                    "dataBitsPerSymbol", "HE user has no data bits per symbol", userIndex);
            return result;
        }
        if (user.shortCodedBitsPerSymbol <= 0 || user.shortDataBitsPerSymbol <= 0) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_DATA_RATE,
                    "shortDataBitsPerSymbol", "HE user has no data bits in a short symbol segment", userIndex);
            return result;
        }
        // IEEE Std 802.11-2024 27.3.12.5.1: HE BCC always uses one encoder.
        if (user.coding == HE_CODING_BCC)
            user.numberOfEncoders = 1;
        else
            user.numberOfEncoders = std::max(1, (user.dataBitsPerSymbol + 647) / 648);
        user.tailBits = user.coding == HE_CODING_LDPC ? 0 : 6 * user.numberOfEncoders;
        const int64_t psduBytes = user.psduLength.get<B>();
        const int64_t fixedBits = (int64_t)user.serviceBits + user.tailBits;
        if (user.serviceBits < 0 ||
                psduBytes > (std::numeric_limits<int64_t>::max() - fixedBits) / 8) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_PSDU_LENGTH,
                    "psduLength", "HE PSDU length overflows packet-level bit accounting", userIndex);
            return result;
        }
        user.payloadAndServiceBits = fixedBits + psduBytes * 8;
        user.excessBits = user.payloadAndServiceBits % user.dataBitsPerSymbol;
        user.individualInitialPreFecPaddingFactor = user.excessBits == 0 ? 4 :
                std::min<int64_t>(4, ceilDivide(user.excessBits, user.shortDataBitsPerSymbol));
        const int64_t individualSymbols = ceilDivide(user.payloadAndServiceBits,
                user.dataBitsPerSymbol);
        if (individualSymbols > std::numeric_limits<int>::max()) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_PSDU_LENGTH,
                    "psduLength", "HE PSDU requires more data symbols than the model can represent", userIndex);
            return result;
        }
        user.individualInitialNumberOfDataSymbols = individualSymbols;
        parameters.users.push_back(user);
    }

    std::vector<size_t> dataUsers;
    for (size_t i = 0; i < parameters.users.size(); ++i)
        if (parameters.users[i].psduLength != B(0))
            dataUsers.push_back(i);

    int commonInitialFactor = 0;
    int commonInitialSymbols = 0;
    int finalFactor = 0;
    int finalSymbols = 0;
    bool commonLdpcExtra = false;
    if (!dataUsers.empty() && !tbContext) {
        // Equation 27-75 compared exactly in quarter-symbol units. The metric
        // is 4*(N_SYM,init-1)+a_init for the supported non-STBC path.
        int64_t maximumMetric = -1;
        for (size_t userIndex : dataUsers) {
            const auto& user = parameters.users[userIndex];
            const int64_t metric = 4LL * (user.individualInitialNumberOfDataSymbols - 1) +
                    user.individualInitialPreFecPaddingFactor;
            if (metric > maximumMetric) {
                maximumMetric = metric;
                commonInitialFactor = user.individualInitialPreFecPaddingFactor;
                commonInitialSymbols = user.individualInitialNumberOfDataSymbols;
            }
        }
    }
    else if (!dataUsers.empty()) {
        finalFactor = tbContext->preFecPaddingFactor;
        const int64_t hePreambleNs = (parameters.common.commonPreambleDuration -
                SimTime(20, SIMTIME_US)).inUnit(SIMTIME_NS);
        const int64_t legacyEnvelopeNs = ((tbContext->ulLength + 5) / 3) * 4000LL;
        const int64_t symbolDurationNs = symbolDuration.inUnit(SIMTIME_NS);
        const int64_t availableNs = legacyEnvelopeNs - hePreambleNs;
        const int64_t derivedSymbols = availableNs / symbolDurationNs -
                (tbContext->peDisambiguity ? 1 : 0);
        if (availableNs < 0 || derivedSymbols < 1 || derivedSymbols > std::numeric_limits<int>::max()) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                    "tbContext.ulLength", "HE Trigger UL Length cannot represent a positive Data field");
            return result;
        }
        finalSymbols = derivedSymbols;
        const int64_t residualNs = availableNs - derivedSymbols * symbolDurationNs;
        // Equation 27-114 intentionally rounds the residual down to a whole
        // 4 us unit; the residual itself need not be divisible by 4 us because
        // T_SYM can be 13.6 us or 14.4 us.
        const int derivedPacketExtensionUs = residualNs / 4000 * 4;
        if (derivedPacketExtensionUs != 0 && derivedPacketExtensionUs != 4 &&
                derivedPacketExtensionUs != 8 && derivedPacketExtensionUs != 12 &&
                derivedPacketExtensionUs != 16) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                    "tbContext.peDisambiguity", "HE Trigger timing derives an invalid packet extension");
            return result;
        }
        parameters.common.packetExtensionDurationUs = derivedPacketExtensionUs;
        commonLdpcExtra = tbContext->ldpcExtraSymbolSegment;
    }

    auto initializeUserAtBoundary = [&] (Ieee80211HeUserPhyParameters& user,
            int initialFactor, int initialSymbols, size_t userIndex) -> bool {
        user.initialPreFecPaddingFactor = initialFactor;
        user.initialNumberOfDataSymbols = initialSymbols;
        user.initialLastDataBitsPerSymbol = initialFactor < 4 ?
                initialFactor * user.shortDataBitsPerSymbol : user.dataBitsPerSymbol;
        user.initialLastCodedBitsPerSymbol = initialFactor < 4 ?
                initialFactor * user.shortCodedBitsPerSymbol : user.codedBitsPerSymbol;
        const int64_t payloadCapacity = (int64_t)(initialSymbols - 1) * user.dataBitsPerSymbol +
                user.initialLastDataBitsPerSymbol;
        const int64_t codedCapacity = (int64_t)(initialSymbols - 1) * user.codedBitsPerSymbol +
                user.initialLastCodedBitsPerSymbol;
        const int64_t preFecPadding = payloadCapacity - user.payloadAndServiceBits;
        if (initialSymbols < 1 || preFecPadding < 0 ||
                payloadCapacity > std::numeric_limits<int>::max() ||
                codedCapacity > std::numeric_limits<int>::max() ||
                preFecPadding > std::numeric_limits<int>::max()) {
            setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                    "preFecPaddingFactor", "HE common padding boundary is shorter than a user payload", userIndex);
            return false;
        }
        user.preFecPaddingBits = preFecPadding;
        user.macPreFecPaddingBits = preFecPadding / 8 * 8;
        user.phyPreFecPaddingBits = preFecPadding % 8;
        if (user.coding == HE_CODING_LDPC) {
            user.ldpcPayloadBits = payloadCapacity;
            user.ldpcAvailableBits = codedCapacity;
            const auto codeRate = getHeMcsCodeRate(user.mcs);
            auto ldpc = computeHeLdpcParameters(payloadCapacity, codedCapacity,
                    codeRate.first, codeRate.second);
            if (!ldpc) {
                setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INTERNAL_ERROR,
                        "ldpc", ldpc.error, userIndex);
                return false;
            }
            user.ldpcCodewordLength = ldpc.codewordLength;
            user.ldpcCodewordCount = ldpc.codewordCount;
            user.ldpcShorteningBits = ldpc.shorteningBits;
            user.ldpcPuncturingBits = ldpc.puncturingBits;
            user.numberOfEncoders = ldpc.codewordCount;
        }
        return true;
    };

    // First establish the common (MU/SU) or Trigger-reversed (TB LDPC)
    // initial boundary, then evaluate each LDPC user's extra-segment need.
    for (size_t userIndex : dataUsers) {
        auto& user = parameters.users[userIndex];
        int initialFactor = commonInitialFactor;
        int initialSymbols = commonInitialSymbols;
        if (tbContext) {
            initialFactor = finalFactor;
            initialSymbols = finalSymbols;
            if (user.coding == HE_CODING_LDPC && commonLdpcExtra) {
                if (finalFactor == 1) {
                    initialFactor = 4;
                    initialSymbols = finalSymbols - 1;
                }
                else
                    initialFactor = finalFactor - 1;
            }
        }
        if (!initializeUserAtBoundary(user, initialFactor, initialSymbols, userIndex))
            return result;
        if (!tbContext && user.coding == HE_CODING_LDPC) {
            const auto codeRate = getHeMcsCodeRate(user.mcs);
            auto ldpc = computeHeLdpcParameters(user.ldpcPayloadBits,
                    user.ldpcAvailableBits, codeRate.first, codeRate.second);
            if (!ldpc) {
                setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INTERNAL_ERROR,
                        "ldpc", ldpc.error, userIndex);
                return result;
            }
            commonLdpcExtra |= ldpc.extraSymbolRequired;
        }
    }

    if (!dataUsers.empty() && !tbContext) {
        if (commonLdpcExtra && commonInitialFactor == 4) {
            finalFactor = 1;
            finalSymbols = commonInitialSymbols + 1;
        }
        else {
            finalFactor = commonInitialFactor + (commonLdpcExtra ? 1 : 0);
            finalSymbols = commonInitialSymbols;
        }
    }
    parameters.common.preFecPaddingFactor = finalFactor;
    parameters.common.ldpcExtraSymbol = commonLdpcExtra;
    parameters.commonNumberOfDataSymbols = finalSymbols;

    for (size_t userIndex : dataUsers) {
        auto& user = parameters.users[userIndex];
        const auto codeRate = getHeMcsCodeRate(user.mcs);
        if (user.coding == HE_CODING_LDPC && commonLdpcExtra) {
            const int increment = user.initialPreFecPaddingFactor == 3 ?
                    user.codedBitsPerSymbol - 3 * user.shortCodedBitsPerSymbol :
                    user.shortCodedBitsPerSymbol;
            if (increment < 0 || user.ldpcAvailableBits > std::numeric_limits<int>::max() - increment) {
                setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INTERNAL_ERROR,
                        "ldpcAvailableBits", "HE LDPC extra segment overflows the model bit count", userIndex);
                return result;
            }
            user.ldpcAvailableBits += increment;
            const int64_t puncturing = std::max<int64_t>(0,
                    (int64_t)user.ldpcCodewordCount * user.ldpcCodewordLength -
                    user.ldpcAvailableBits - user.ldpcShorteningBits);
            if (puncturing > std::numeric_limits<int>::max()) {
                setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INTERNAL_ERROR,
                        "ldpcPuncturingBits", "HE LDPC puncturing exceeds the model bit count", userIndex);
                return result;
            }
            user.ldpcPuncturingBits = puncturing;
        }
        user.finalLastCodedBitsPerSymbol = finalFactor < 4 ?
                finalFactor * user.shortCodedBitsPerSymbol : user.codedBitsPerSymbol;
        user.finalLastDataBitsPerSymbol = user.coding == HE_CODING_LDPC ?
                user.initialLastDataBitsPerSymbol : finalFactor < 4 ?
                finalFactor * user.shortDataBitsPerSymbol : user.dataBitsPerSymbol;
        if (user.coding == HE_CODING_BCC) {
            const int64_t finalPayloadCapacity = (int64_t)(finalSymbols - 1) * user.dataBitsPerSymbol +
                    user.finalLastDataBitsPerSymbol;
            const int64_t preFecPadding = finalPayloadCapacity - user.payloadAndServiceBits;
            if (preFecPadding < 0 || preFecPadding > std::numeric_limits<int>::max()) {
                setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                        "preFecPaddingFactor", "HE final BCC padding boundary is shorter than the payload", userIndex);
                return result;
            }
            user.preFecPaddingBits = preFecPadding;
            user.macPreFecPaddingBits = preFecPadding / 8 * 8;
            user.phyPreFecPaddingBits = preFecPadding % 8;
            if (user.dcm && user.mcs == 0 && user.numberOfSpatialStreams == 1 &&
                    (user.ru.toneSize == 106 || user.ru.toneSize == 242)) {
                const int64_t fecInputBits = user.payloadAndServiceBits + preFecPadding;
                const int64_t codedBits = fecInputBits * codeRate.second / codeRate.first;
                user.bccCodedPaddingBits = codedBits / (2 * user.dataBitsPerSymbol);
            }
        }
        else {
            const int64_t repetition = std::max<int64_t>(0,
                    user.ldpcAvailableBits -
                    (int64_t)user.ldpcCodewordCount * user.ldpcCodewordLength *
                            (codeRate.second - codeRate.first) / codeRate.second -
                    user.ldpcPayloadBits);
            if (repetition > std::numeric_limits<int>::max() ||
                    (repetition > 0 && user.ldpcPuncturingBits > 0)) {
                setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INTERNAL_ERROR,
                        "ldpcRepetitionBits", "HE LDPC puncturing and repetition outcomes are inconsistent", userIndex);
                return result;
            }
            user.ldpcRepetitionBits = repetition;
        }
        user.preFecPaddingFactor = finalFactor;
        user.postFecPaddingBits = user.codedBitsPerSymbol - user.finalLastCodedBitsPerSymbol;
        user.numberOfDataSymbols = finalSymbols;
        user.numberOfSymbols = finalSymbols;
        const int nominalPe = calculateHeNominalPacketExtensionDurationUs(finalFactor,
                user.nominalPacketPaddingDurationUs);
        parameters.common.nominalPacketExtensionDurationUs = std::max(
                parameters.common.nominalPacketExtensionDurationUs, nominalPe);
    }

    if (isFeedbackNdp) {
        parameters.common.preFecPaddingFactor = 0;
        parameters.common.ldpcExtraSymbol = false;
        parameters.common.nominalPacketExtensionDurationUs = 0;
        parameters.common.packetExtensionDurationUs = 0;
        parameters.commonNumberOfDataSymbols = 0;
    }
    if (parameters.common.packetExtensionDurationUs <
            parameters.common.nominalPacketExtensionDurationUs) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::INVALID_PACKET_EXTENSION,
                "packetExtensionDurationUs", "HE packet extension is shorter than the nominal value");
        return result;
    }

    parameters.duration = parameters.common.commonPreambleDuration +
            parameters.commonNumberOfDataSymbols * symbolDuration +
            SimTime(parameters.common.packetExtensionDurationUs, SIMTIME_US);
    
    // IEEE 802.11-2024 Table 27-61 defines aPPDUMaxTime = 5.484 ms; Clause 10.12
    // forbids transmitting an HE PPDU whose PLME-TXTIME exceeds that limit.
    if (enforceDurationLimit && parameters.duration > SimTime(5.484, SIMTIME_MS)) {
        setHePhyValidationError(result, Ieee80211HeValidationErrorCode::PPDU_DURATION_EXCEEDED,
                "duration", "HE PPDU exceeds the 5.484 ms duration limit");
        return result;
    }
    for (auto& user : parameters.users) {
        user.dataDuration = parameters.commonNumberOfDataSymbols * symbolDuration;
        user.preambleDuration = parameters.common.commonPreambleDuration;
        user.headerDuration = SIMTIME_ZERO;
        user.duration = parameters.duration;
    }
    result.valid = true;
    result.errorCode = Ieee80211HeValidationErrorCode::NONE;
    result.context = {};
    result.error.clear();
    return result;
}

Ieee80211HeTriggerResponseFinalizationResult finalizeHeTriggerResponse(
        const Ieee80211HeTriggerResponseFinalizationRequest& request)
{
    Ieee80211HeTriggerResponseFinalizationResult result;
    auto fail = [&] (Ieee80211HeValidationErrorCode errorCode,
            const char *fieldName, const std::string& detail) {
        result.valid = false;
        result.errorCode = errorCode;
        result.context.fieldName = fieldName;
        result.context.detail = detail;
        result.error = detail;
    };
    if (request.durationBudget && *request.durationBudget <= SIMTIME_ZERO) {
        fail(Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                "durationBudget", "HE Trigger response has no positive duration budget");
        return result;
    }

    uint32_t signalExtensionNs;
    try {
        signalExtensionNs = getIeee80211HeSignalExtensionNs(
                request.centerFrequency, request.noSignalExtension);
    }
    catch (const cRuntimeError& error) {
        fail(Ieee80211HeValidationErrorCode::INVALID_CENTER_FREQUENCY,
                "centerFrequency", error.what());
        return result;
    }

    if (request.fixedBoundary) {
        const auto& boundary = *request.fixedBoundary;
        if (boundary.channelBandwidth != request.channelBandwidth ||
                boundary.guardInterval != request.guardInterval ||
                boundary.ltfType != request.ltfType ||
                boundary.packetExtensionDurationUs != request.packetExtensionDurationUs ||
                boundary.ulLength == 0 ||
                boundary.preFecPaddingFactor < 1 ||
                boundary.preFecPaddingFactor > 4 ||
                boundary.numberOfHeLtfSymbols <= 0) {
            fail(Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                    "fixedBoundary", "HE Trigger fixed boundary does not match the finalization request");
            return result;
        }
        auto envelope = getIeee80211HeTriggerTxTimeUpperBound(
                boundary.ulLength, signalExtensionNs);
        if (!envelope || (request.durationBudget && envelope.txTime > *request.durationBudget)) {
            fail(Ieee80211HeValidationErrorCode::PPDU_DURATION_EXCEEDED,
                    "fixedBoundary", "HE Trigger fixed boundary exceeds the duration budget");
            return result;
        }
        Ieee80211HeTbCalculationContext tbContext;
        tbContext.triggerMethod = Ieee80211HeTriggerMethod::TRIGGER_FRAME;
        tbContext.ulLength = boundary.ulLength;
        tbContext.preFecPaddingFactor = boundary.preFecPaddingFactor;
        tbContext.ldpcExtraSymbolSegment = boundary.ldpcExtraSymbolSegment;
        tbContext.peDisambiguity = boundary.peDisambiguity;
        tbContext.numberOfHeLtfSymbols = boundary.numberOfHeLtfSymbols;
        auto candidate = computeHePpduParameters(request.users,
                request.channelBandwidth, HE_TRIGGER_BASED_UPLINK,
                request.guardInterval, request.ltfType,
                request.packetExtensionDurationUs, false, tbContext);
        if (!candidate) {
            result.errorCode = candidate.errorCode;
            result.context = candidate.context;
            result.error = std::string("Cannot validate HE Trigger fixed boundary: ") +
                    candidate.context.detail;
            return result;
        }
        const auto resolvedTxTime = candidate.parameters.duration +
                SimTime(signalExtensionNs, SIMTIME_NS);
        auto projected = buildIeee80211HeTriggerUlLength(
                resolvedTxTime, signalExtensionNs);
        if (!projected) {
            fail(Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                    "fixedBoundary", "HE Trigger fixed-boundary TXTIME cannot be projected to UL Length");
            return result;
        }
        if (projected.value.length != boundary.ulLength) {
            fail(Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                    "fixedBoundary", "HE Trigger users resolve to a different UL Length bucket");
            return result;
        }
        if (candidate.parameters.common.preFecPaddingFactor !=
                boundary.preFecPaddingFactor) {
            fail(Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                    "fixedBoundary", "HE Trigger users do not preserve the finalized padding factor");
            return result;
        }
        if (candidate.parameters.common.ldpcExtraSymbol !=
                boundary.ldpcExtraSymbolSegment) {
            fail(Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                    "fixedBoundary", "HE Trigger users do not preserve the finalized LDPC extra-symbol flag");
            return result;
        }
        if (candidate.parameters.common.numberOfHeLtfSymbols !=
                boundary.numberOfHeLtfSymbols) {
            fail(Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                    "fixedBoundary", "HE Trigger users do not preserve the finalized HE-LTF count");
            return result;
        }
        if (candidate.parameters.common.packetExtensionDurationUs !=
                boundary.packetExtensionDurationUs) {
            fail(Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT,
                    "fixedBoundary", "HE Trigger users do not preserve the finalized packet extension");
            return result;
        }
        result.ulLength = boundary.ulLength;
        result.commonDuration = envelope.txTime;
        result.peDisambiguity = boundary.peDisambiguity;
        result.resolvedTxTime = resolvedTxTime;
        result.parameters = candidate.parameters;
        result.errorCode = Ieee80211HeValidationErrorCode::NONE;
        result.valid = true;
        return result;
    }

    auto minimum = computeHePpduParameters(request.users, request.channelBandwidth,
            HE_TRIGGER_BASED_UPLINK, request.guardInterval, request.ltfType,
            request.packetExtensionDurationUs, false);
    if (!minimum) {
        result.errorCode = minimum.errorCode;
        result.context = minimum.context;
        result.error = std::string("Cannot calculate HE Trigger response timing: ") +
                minimum.context.detail;
        return result;
    }

    const auto maximumPpduDuration = SimTime(5.484, SIMTIME_MS);
    auto durationCap = request.durationBudget ?
            std::min(*request.durationBudget, maximumPpduDuration) : maximumPpduDuration;
    durationCap = SimTime(durationCap.inUnit(SIMTIME_NS), SIMTIME_NS);
    const auto signalExtensionDuration = SimTime(signalExtensionNs, SIMTIME_NS);
    const auto minimumTxTime = minimum.parameters.duration + signalExtensionDuration;
    if (minimumTxTime > durationCap) {
        fail(Ieee80211HeValidationErrorCode::PPDU_DURATION_EXCEEDED,
                "durationBudget",
                "HE Trigger response duration budget cannot contain the selected preamble and payload");
        return result;
    }

    auto projectedMinimum = buildIeee80211HeTriggerUlLength(minimumTxTime,
            signalExtensionNs);
    if (!projectedMinimum) {
        fail(Ieee80211HeValidationErrorCode::INVALID_L_SIG_LENGTH, "ulLength",
                std::string("Cannot select an HE Trigger UL Length: ") +
                        projectedMinimum.error);
        return result;
    }

    const bool feedbackNdp = std::all_of(request.users.begin(), request.users.end(),
            [] (const auto& user) { return user.psduLength == B(0); });
    if (feedbackNdp) {
        auto envelope = getIeee80211HeTriggerTxTimeUpperBound(
                projectedMinimum.value.length, signalExtensionNs);
        if (!envelope || envelope.txTime > durationCap) {
            fail(Ieee80211HeValidationErrorCode::PPDU_DURATION_EXCEEDED,
                    "durationBudget",
                    "HE feedback NDP UL Length exceeds the duration budget");
            return result;
        }
        result.ulLength = projectedMinimum.value.length;
        result.commonDuration = envelope.txTime;
        result.resolvedTxTime = minimumTxTime;
        result.parameters = minimum.parameters;
        result.errorCode = Ieee80211HeValidationErrorCode::NONE;
        result.valid = true;
        return result;
    }

    int firstUlLength = projectedMinimum.value.length;
    int lastUlLength = 4093;
    int increment = 3;
    if (request.durationBudget) {
        auto highest = buildIeee80211HeTriggerUlLength(durationCap,
                signalExtensionNs);
        if (!highest) {
            fail(Ieee80211HeValidationErrorCode::INVALID_L_SIG_LENGTH, "ulLength",
                    std::string("Cannot select an HE Trigger UL Length: ") +
                            highest.error);
            return result;
        }
        firstUlLength = highest.value.length;
        lastUlLength = projectedMinimum.value.length;
        increment = -3;
    }

    for (int ulLength = firstUlLength;
            increment > 0 ? ulLength <= lastUlLength : ulLength >= lastUlLength;
            ulLength += increment) {
        auto envelope = getIeee80211HeTriggerTxTimeUpperBound(
                ulLength, signalExtensionNs);
        if (!envelope || envelope.txTime > durationCap)
            continue;
        for (bool ldpcExtraSymbolSegment : {false, true}) {
            for (bool peDisambiguity : {false, true}) {
                Ieee80211HeTbCalculationContext tbContext;
                tbContext.triggerMethod = Ieee80211HeTriggerMethod::TRIGGER_FRAME;
                tbContext.ulLength = ulLength;
                tbContext.preFecPaddingFactor =
                        minimum.parameters.common.preFecPaddingFactor;
                tbContext.ldpcExtraSymbolSegment = ldpcExtraSymbolSegment;
                tbContext.peDisambiguity = peDisambiguity;
                tbContext.numberOfHeLtfSymbols =
                        minimum.parameters.common.numberOfHeLtfSymbols;
                auto candidate = computeHePpduParameters(request.users,
                        request.channelBandwidth, HE_TRIGGER_BASED_UPLINK,
                        request.guardInterval, request.ltfType,
                        request.packetExtensionDurationUs, false, tbContext);
                if (!candidate)
                    continue;
                if (!ldpcExtraSymbolSegment) {
                    bool shouldSetLdpcExtra = false;
                    for (const auto& user : candidate.parameters.users) {
                        if (user.coding != HE_CODING_LDPC)
                            continue;
                        const auto codeRate = getHeMcsCodeRate(user.mcs);
                        auto ldpc = computeHeLdpcParameters(user.ldpcPayloadBits,
                                user.ldpcAvailableBits, codeRate.first, codeRate.second);
                        if (!ldpc || ldpc.extraSymbolRequired) {
                            shouldSetLdpcExtra = true;
                            break;
                        }
                    }
                    // Clause 27.3.12.5.5 permits a received flag0, but an AP
                    // finalizing a new Trigger should set flag1 whenever any
                    // solicited user's reversed LDPC boundary requires it.
                    if (shouldSetLdpcExtra)
                        continue;
                }
                const auto resolvedTxTime = candidate.parameters.duration +
                        signalExtensionDuration;
                if (resolvedTxTime > durationCap || resolvedTxTime < minimumTxTime)
                    continue;
                auto projected = buildIeee80211HeTriggerUlLength(resolvedTxTime,
                        signalExtensionNs);
                if (!projected || projected.value.length != ulLength)
                    continue;

                result.ulLength = ulLength;
                result.commonDuration = envelope.txTime;
                result.peDisambiguity = peDisambiguity;
                result.resolvedTxTime = resolvedTxTime;
                result.parameters = candidate.parameters;
                result.errorCode = Ieee80211HeValidationErrorCode::NONE;
                result.valid = true;
                return result;
            }
        }
    }

    fail(Ieee80211HeValidationErrorCode::INVALID_L_SIG_LENGTH, "ulLength",
            "HE Trigger response has no L_LENGTH bucket with a legal full-symbol TXTIME");
    return result;
}

Ieee80211HePsduCapacityResult getHeTbPsduCapacity(
        const Ieee80211HeTbCapacityBoundary& boundary,
        const Ieee80211HeRu& ru, int mcs, int numberOfSpatialStreams,
        Ieee80211HeCoding coding)
{
    Ieee80211HePsduCapacityResult result;
    if (boundary.ulLength == 0 || boundary.ulLength > 4095 ||
            boundary.preFecPaddingFactor < 1 || boundary.preFecPaddingFactor > 4 ||
            boundary.numberOfHeLtfSymbols < 1) {
        result.errorCode = Ieee80211HeValidationErrorCode::INVALID_TRIGGER_CONTEXT;
        result.context.fieldName = "boundary";
        result.context.detail = "HE-TB capacity boundary is not finalized";
        result.error = result.context.detail;
        return result;
    }

    Ieee80211HeTbCalculationContext tbContext;
    tbContext.triggerMethod = Ieee80211HeTriggerMethod::TRIGGER_FRAME;
    tbContext.ulLength = boundary.ulLength;
    tbContext.preFecPaddingFactor = boundary.preFecPaddingFactor;
    tbContext.ldpcExtraSymbolSegment = boundary.ldpcExtraSymbolSegment;
    tbContext.peDisambiguity = boundary.peDisambiguity;
    tbContext.numberOfHeLtfSymbols = boundary.numberOfHeLtfSymbols;

    auto fits = [&] (int64_t bytes, Ieee80211HePhyValidationResult *calculation = nullptr) {
        Ieee80211HeUserPhyParameters user;
        user.ru = ru;
        user.mcs = mcs;
        user.numberOfSpatialStreams = numberOfSpatialStreams;
        user.coding = coding;
        user.psduLength = B(bytes);
        auto candidate = computeHePpduParameters({user}, boundary.channelBandwidth,
                HE_TRIGGER_BASED_UPLINK, boundary.guardInterval, boundary.ltfType,
                boundary.packetExtensionDurationUs, true, tbContext);
        if (calculation != nullptr)
            *calculation = candidate;
        return static_cast<bool>(candidate);
    };

    Ieee80211HePhyValidationResult minimum;
    if (!fits(1, &minimum)) {
        result.errorCode = minimum.errorCode;
        result.context = minimum.context;
        result.error = minimum.error;
        return result;
    }

    // IEEE 802.11's HE A-MPDU length is below this conservative search cap.
    int64_t low = 1;
    int64_t high = 16 * 1024 * 1024;
    while (low < high) {
        int64_t middle = low + (high - low + 1) / 2;
        if (fits(middle))
            low = middle;
        else
            high = middle - 1;
    }
    result.maximumPsduLength = B(low);
    result.valid = true;
    return result;
}

} // namespace physicallayer
} // namespace inet
