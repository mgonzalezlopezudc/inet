//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeSigCodec.h"

#include <algorithm>
#include <map>

namespace inet {
namespace physicallayer {

namespace {

template<typename Result>
bool setError(Result& result, Ieee80211HeSigCodecErrorCode errorCode, const char *error)
{
    result.valid = false;
    result.errorCode = errorCode;
    result.error = error;
    return false;
}

template<typename Result>
void setValid(Result& result)
{
    result.valid = true;
    result.errorCode = Ieee80211HeSigCodecErrorCode::NONE;
    result.error.clear();
}

template<typename Target, typename Source>
void copyError(Target& target, const Source& source)
{
    target.valid = false;
    target.errorCode = source.errorCode;
    target.error = source.error;
}

uint32_t readInteger(const std::vector<bool>& bits, size_t offset, size_t width)
{
    uint32_t value = 0;
    for (size_t i = 0; i < width; ++i)
        if (bits[offset + i])
            value |= uint32_t(1) << i;
    return value;
}

void writeInteger(std::vector<bool>& bits, size_t offset, size_t width, uint32_t value)
{
    for (size_t i = 0; i < width; ++i)
        bits[offset + i] = (value >> i) & 1;
}

bool hasEvenParity(const std::vector<bool>& bits, size_t count, bool parity)
{
    bool expectedParity = false;
    for (size_t i = 0; i < count; ++i)
        expectedParity = expectedParity != bits[i];
    return expectedParity == parity;
}

bool isValidSpatialReuse(uint8_t value)
{
    return value == 0 || (value >= 13 && value <= 15);
}

Ieee80211HeSigALayout makeSigALayout(Ieee80211HeSigFormat format)
{
    Ieee80211HeSigALayout layout;
    layout.format = format;
    if (format == Ieee80211HeSigFormat::ER_SU) {
        layout.ofdmSymbolCount = 4;
        layout.repetitionCount = 2;
    }
    return layout;
}

template<typename Result>
bool validateLSigLength(Result& result, uint64_t length, Ieee80211HeSigFormat format)
{
    if (length > 4095)
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_LENGTH, "L-SIG LENGTH exceeds 12 bits");
    unsigned int requiredRemainder;
    switch (format) {
        case Ieee80211HeSigFormat::SU:
        case Ieee80211HeSigFormat::TB:
            requiredRemainder = 1;
            break;
        case Ieee80211HeSigFormat::ER_SU:
        case Ieee80211HeSigFormat::MU:
            requiredRemainder = 2;
            break;
        default:
            return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FORMAT, "unknown HE PPDU format for L-SIG");
    }
    if (length % 3 != requiredRemainder)
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_LENGTH, "L-SIG LENGTH has the wrong modulo-3 value for the HE PPDU format");
    return true;
}

template<typename Result>
bool validateSigABits(Result& result, const std::vector<bool>& bits)
{
    if (bits.size() != 52)
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_BIT_COUNT, "HE-SIG-A must contain exactly 52 logical bits");
    for (size_t i = 46; i < 52; ++i)
        if (bits[i])
            return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_TAIL, "HE-SIG-A tail must be zero");
    std::vector<bool> protectedBits(bits.begin(), bits.begin() + 42);
    auto crc = computeHeCrc4(protectedBits);
    for (size_t i = 0; i < crc.size(); ++i)
        if (bits[42 + i] != crc[i])
            return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_CRC, "HE-SIG-A CRC-4 mismatch");
    return true;
}

void finishSigA(std::vector<bool>& bits)
{
    std::vector<bool> protectedBits(bits.begin(), bits.begin() + 42);
    auto crc = computeHeCrc4(protectedBits);
    for (size_t i = 0; i < crc.size(); ++i)
        bits[42 + i] = crc[i];
}

template<typename Fields, typename Result>
bool validateSuSigA(Result& result, const Fields& value, bool extendedRange)
{
    if (value.bssColor > 63 || value.txop > 127 || value.preFecPaddingFactor > 3 || value.giLtfSize > 3)
        return setError(result, Ieee80211HeSigCodecErrorCode::FIELD_OUT_OF_RANGE, "HE SU SIG-A field exceeds its encoded width");
    if (!isValidSpatialReuse(value.spatialReuse))
        return setError(result, Ieee80211HeSigCodecErrorCode::RESERVED_FIELD_VALUE, "HE SU SIG-A spatial reuse value is reserved");
    if ((!extendedRange && (value.mcs > 11 || value.bandwidth > 3)) ||
            (extendedRange && (value.bandwidth > 1 || (value.bandwidth == 0 && value.mcs > 2) || (value.bandwidth == 1 && value.mcs != 0))))
        return setError(result, Ieee80211HeSigCodecErrorCode::RESERVED_FIELD_VALUE, "HE SU SIG-A MCS or bandwidth value is reserved for the format");
    if (value.numberOfSpaceTimeStreams == 0 || value.numberOfSpaceTimeStreams > (extendedRange ? 2 : (value.doppler ? 4 : 8)))
        return setError(result, Ieee80211HeSigCodecErrorCode::FIELD_OUT_OF_RANGE, "HE SU SIG-A space-time stream count is invalid");
    if ((value.doppler && value.midamblePeriodicity != 10 && value.midamblePeriodicity != 20) ||
            (!value.doppler && value.midamblePeriodicity != 0))
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION, "HE SU SIG-A midamble periodicity is inconsistent with Doppler");
    if (!value.ldpcCoding && value.ldpcExtraSymbolSegment)
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION, "HE SU SIG-A LDPC extra symbol segment is not a semantic field with BCC coding");
    if (!extendedRange && !value.ldpcCoding &&
            (value.bandwidth > 0 || value.mcs >= 10 || value.numberOfSpaceTimeStreams > 4))
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                "HE SU SIG-A BCC requires 20 MHz, MCS below 10, and at most four space-time streams");
    if (value.dcm && value.stbc && value.giLtfSize != 3)
        return setError(result, Ieee80211HeSigCodecErrorCode::RESERVED_FIELD_VALUE, "HE SU SIG-A DCM/STBC/GI-LTF combination is reserved");
    bool dcmApplied = value.dcm && !(value.stbc && value.giLtfSize == 3);
    bool stbcApplied = value.stbc && !(value.dcm && value.giLtfSize == 3);
    if (dcmApplied && value.mcs != 0 && value.mcs != 1 && value.mcs != 3 && value.mcs != 4)
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION, "HE SU SIG-A DCM is not applicable to this MCS");
    if (dcmApplied && value.numberOfSpaceTimeStreams > 2)
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION, "HE SU SIG-A DCM supports at most two space-time streams");
    if (stbcApplied && value.numberOfSpaceTimeStreams != 2)
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION, "HE SU SIG-A applied STBC requires two space-time streams");
    if (extendedRange && value.numberOfSpaceTimeStreams != (stbcApplied ? 2 : 1))
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION, "HE ER SU SIG-A stream count is inconsistent with applied STBC");
    return true;
}

template<typename Fields>
Ieee80211HeSigABitsResult encodeSuSigA(const Fields& value, Ieee80211HeSigFormat format)
{
    Ieee80211HeSigABitsResult result;
    result.layout = makeSigALayout(format);
    if (!validateSuSigA(result, value, format == Ieee80211HeSigFormat::ER_SU))
        return result;
    result.bits.assign(52, false);
    result.bits[0] = true;
    result.bits[1] = value.beamChange;
    result.bits[2] = value.uplink;
    writeInteger(result.bits, 3, 4, value.mcs);
    result.bits[7] = value.dcm;
    writeInteger(result.bits, 8, 6, value.bssColor);
    result.bits[14] = true;
    writeInteger(result.bits, 15, 4, value.spatialReuse);
    writeInteger(result.bits, 19, 2, value.bandwidth);
    writeInteger(result.bits, 21, 2, value.giLtfSize);
    uint8_t nsts = value.numberOfSpaceTimeStreams - 1;
    if (value.doppler && value.midamblePeriodicity == 20)
        nsts |= 4;
    writeInteger(result.bits, 23, 3, nsts);
    writeInteger(result.bits, 26, 7, value.txop);
    result.bits[33] = value.ldpcCoding;
    result.bits[34] = value.ldpcCoding ? value.ldpcExtraSymbolSegment : true;
    result.bits[35] = value.stbc;
    result.bits[36] = value.beamformed;
    writeInteger(result.bits, 37, 2, value.preFecPaddingFactor);
    result.bits[39] = value.peDisambiguity;
    result.bits[40] = true;
    result.bits[41] = value.doppler;
    finishSigA(result.bits);
    setValid(result);
    return result;
}

template<typename Fields, typename Result>
Result decodeSuSigA(const std::vector<bool>& bits, Ieee80211HeSigFormat format)
{
    Result result;
    result.layout = makeSigALayout(format);
    if (!validateSigABits(result, bits))
        return result;
    if (!bits[0]) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FORMAT, "HE SU or ER SU SIG-A format bit must be 1");
        return result;
    }
    if (!bits[14] || !bits[40] || (!bits[33] && !bits[34])) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_RESERVED_FIELD, "HE SU SIG-A reserved bit is not set to 1");
        return result;
    }
    auto& value = result.value;
    value.beamChange = bits[1];
    value.uplink = bits[2];
    value.mcs = readInteger(bits, 3, 4);
    value.dcm = bits[7];
    value.bssColor = readInteger(bits, 8, 6);
    value.spatialReuse = readInteger(bits, 15, 4);
    value.bandwidth = readInteger(bits, 19, 2);
    value.giLtfSize = readInteger(bits, 21, 2);
    value.txop = readInteger(bits, 26, 7);
    value.ldpcCoding = bits[33];
    value.ldpcExtraSymbolSegment = value.ldpcCoding && bits[34];
    value.stbc = bits[35];
    value.beamformed = bits[36];
    value.preFecPaddingFactor = readInteger(bits, 37, 2);
    value.peDisambiguity = bits[39];
    value.doppler = bits[41];
    uint8_t nsts = readInteger(bits, 23, 3);
    if (value.doppler) {
        value.numberOfSpaceTimeStreams = (nsts & 3) + 1;
        value.midamblePeriodicity = (nsts & 4) ? 20 : 10;
    }
    else
        value.numberOfSpaceTimeStreams = nsts + 1;
    if (!validateSuSigA(result, value, format == Ieee80211HeSigFormat::ER_SU))
        return result;
    setValid(result);
    return result;
}

int encodeHeLtfCount(uint8_t count)
{
    switch (count) {
        case 1: return 0;
        case 2: return 1;
        case 4: return 2;
        case 6: return 3;
        case 8: return 4;
        default: return -1;
    }
}

template<typename Result>
bool validateMuSigA(Result& result, const Ieee80211HeMuSigA& value)
{
    if (value.heSigBMcs > 5 || value.bssColor > 63 || value.bandwidth > 7 || value.txop > 127 || value.preFecPaddingFactor > 3 || value.giLtfSize > 3)
        return setError(result, Ieee80211HeSigCodecErrorCode::FIELD_OUT_OF_RANGE, "HE MU SIG-A field exceeds its encoded width");
    if (!isValidSpatialReuse(value.spatialReuse))
        return setError(result, Ieee80211HeSigCodecErrorCode::RESERVED_FIELD_VALUE, "HE MU SIG-A spatial reuse value is reserved");
    if (value.heSigBDcm && value.heSigBMcs != 0 && value.heSigBMcs != 1 && value.heSigBMcs != 3 && value.heSigBMcs != 4)
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION, "HE-SIG-B DCM is not applicable to this MCS");
    if (value.heSigBCompression && value.bandwidth > 3)
        return setError(result, Ieee80211HeSigCodecErrorCode::RESERVED_FIELD_VALUE, "punctured HE MU bandwidth codes are reserved with HE-SIG-B compression");
    if ((!value.heSigBCompression && (value.numberOfHeSigBSymbols < 1 || value.numberOfHeSigBSymbols > 16 || value.numberOfMuMimoUsers != 0)) ||
            (value.heSigBCompression && (value.numberOfMuMimoUsers < 1 || value.numberOfMuMimoUsers > 8 || value.numberOfHeSigBSymbols != 0 || value.numberOfHeSigBSymbolsIsSaturated)))
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION, "HE MU SIG-A compression count semantics are inconsistent");
    if (!value.heSigBCompression && value.numberOfHeSigBSymbolsIsSaturated != (value.numberOfHeSigBSymbols == 16))
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION, "HE MU SIG-A saturated symbol-count flag must represent the 16-or-more code");
    if (value.heSigBCompression && value.numberOfMuMimoUsers > 1 && value.stbc)
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION, "compressed HE MU SIG-A does not support STBC for the full-band multi-user RU");
    int ltfCode = encodeHeLtfCount(value.numberOfHeLtfSymbols);
    if (ltfCode < 0 || (!value.doppler && ltfCode > 4) || (value.doppler && ltfCode > 2))
        return setError(result, Ieee80211HeSigCodecErrorCode::RESERVED_FIELD_VALUE, "HE MU SIG-A HE-LTF count is reserved");
    if ((value.doppler && value.midamblePeriodicity != 10 && value.midamblePeriodicity != 20) ||
            (!value.doppler && value.midamblePeriodicity != 0))
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION, "HE MU SIG-A midamble periodicity is inconsistent with Doppler");
    return true;
}

template<typename Result>
bool validateTbSpatialReuse(Result& result, const Ieee80211HeTbSigA& value)
{
    if (value.bandwidth == 0 && (value.spatialReuse[1] != value.spatialReuse[0] ||
            value.spatialReuse[2] != value.spatialReuse[0] || value.spatialReuse[3] != value.spatialReuse[0]))
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                "20 MHz HE TB SIG-A requires all spatial reuse values to be equal");
    if (value.bandwidth == 1 && (value.spatialReuse[2] != value.spatialReuse[0] ||
            value.spatialReuse[3] != value.spatialReuse[1]))
        return setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FIELD_COMBINATION,
                "40 MHz HE TB SIG-A requires spatial reuse values 3/4 to repeat values 1/2");
    return true;
}

} // namespace

Ieee80211HeLSigResult buildHeLSig(Ieee80211HeSigFormat format, uint32_t txTimeUs, uint32_t signalExtensionUs)
{
    Ieee80211HeLSigResult result;
    if (format == Ieee80211HeSigFormat::TB) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FORMAT, "HE TB L-SIG requires caller-supplied L_LENGTH");
        return result;
    }
    if (signalExtensionUs != 0 && signalExtensionUs != 6) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_SIGNAL_EXTENSION, "HE L-SIG signal extension must be 0 or 6 us");
        return result;
    }
    if (txTimeUs < signalExtensionUs + 20 || (txTimeUs - signalExtensionUs - 20) % 4 != 0) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_TXTIME, "HE L-SIG TXTIME does not yield an exact integer Equation 27-11 result");
        return result;
    }
    uint32_t m;
    if (format == Ieee80211HeSigFormat::SU)
        m = 2;
    else if (format == Ieee80211HeSigFormat::ER_SU || format == Ieee80211HeSigFormat::MU)
        m = 1;
    else {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FORMAT, "unknown HE PPDU format for L-SIG");
        return result;
    }
    uint64_t symbols = (txTimeUs - signalExtensionUs - 20) / 4;
    if (symbols * 3 < 3 + m) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_TXTIME, "HE L-SIG TXTIME is too short");
        return result;
    }
    uint64_t length = symbols * 3 - 3 - m;
    if (!validateLSigLength(result, length, format))
        return result;
    result.value.length = length;
    setValid(result);
    return result;
}

Ieee80211HeLSigResult buildHeTbLSig(uint16_t lLength)
{
    Ieee80211HeLSigResult result;
    if (!validateLSigLength(result, lLength, Ieee80211HeSigFormat::TB))
        return result;
    result.value.length = lLength;
    setValid(result);
    return result;
}

Ieee80211HeLSigBitsResult encodeHeLSig(const Ieee80211HeLSig& value, Ieee80211HeSigFormat format)
{
    Ieee80211HeLSigBitsResult result;
    if (!validateLSigLength(result, value.length, format))
        return result;
    result.bits.assign(24, false);
    writeInteger(result.bits, 0, 4, 0xB); // Table 17-6 transmit-order bits 1101
    writeInteger(result.bits, 5, 12, value.length);
    bool parity = false;
    for (size_t i = 0; i < 17; ++i)
        parity = parity != result.bits[i];
    result.bits[17] = parity;
    setValid(result);
    return result;
}

Ieee80211HeLSigResult decodeHeLSig(const std::vector<bool>& bits, Ieee80211HeSigFormat format)
{
    Ieee80211HeLSigResult result;
    if (bits.size() != 24) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_BIT_COUNT, "L-SIG must contain exactly 24 logical bits");
        return result;
    }
    if (readInteger(bits, 0, 4) != 0xB) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_RATE, "L-SIG RATE must be transmit-order bits 1101 (6 Mb/s)");
        return result;
    }
    if (bits[4]) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_RESERVED_FIELD, "L-SIG reserved bit must be 0");
        return result;
    }
    if (!hasEvenParity(bits, 17, bits[17])) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_PARITY, "L-SIG parity check failed");
        return result;
    }
    for (size_t i = 18; i < 24; ++i)
        if (bits[i]) {
            setError(result, Ieee80211HeSigCodecErrorCode::INVALID_TAIL, "L-SIG tail must be zero");
            return result;
        }
    result.value.length = readInteger(bits, 5, 12);
    if (!validateLSigLength(result, result.value.length, format))
        return result;
    setValid(result);
    return result;
}

Ieee80211HeLSigBitsResult encodeHeRlSig(const Ieee80211HeLSig& value, Ieee80211HeSigFormat format)
{
    return encodeHeLSig(value, format);
}

Ieee80211HeRlSigResult decodeHeRlSigRepeat(const std::vector<bool>& lSigBits, const std::vector<bool>& rlSigBits, Ieee80211HeSigFormat format)
{
    Ieee80211HeRlSigResult result;
    auto lSig = decodeHeLSig(lSigBits, format);
    if (!lSig) {
        copyError(result, lSig);
        return result;
    }
    auto rlSig = decodeHeLSig(rlSigBits, format);
    if (!rlSig) {
        copyError(result, rlSig);
        return result;
    }
    if (lSigBits != rlSigBits) {
        setError(result, Ieee80211HeSigCodecErrorCode::RL_SIG_MISMATCH, "RL-SIG is not an exact repeat of L-SIG");
        return result;
    }
    result.value = lSig.value;
    setValid(result);
    return result;
}

std::array<bool, 4> computeHeCrc4(const std::vector<bool>& protectedBits)
{
    uint8_t remainder = 0xFF;
    for (bool bit : protectedBits) {
        bool feedback = ((remainder >> 7) & 1) != bit;
        remainder <<= 1;
        if (feedback)
            remainder ^= 0x07;
    }
    remainder ^= 0xFF;
    return {{bool(remainder & 0x80), bool(remainder & 0x40), bool(remainder & 0x20), bool(remainder & 0x10)}};
}

Ieee80211HeSigABitsResult encodeHeSuSigA(const Ieee80211HeSuSigA& value)
{
    return encodeSuSigA(value, Ieee80211HeSigFormat::SU);
}

Ieee80211HeSuSigAResult decodeHeSuSigA(const std::vector<bool>& bits)
{
    return decodeSuSigA<Ieee80211HeSuSigA, Ieee80211HeSuSigAResult>(bits, Ieee80211HeSigFormat::SU);
}

Ieee80211HeSigABitsResult encodeHeErSuSigA(const Ieee80211HeErSuSigA& value)
{
    return encodeSuSigA(value, Ieee80211HeSigFormat::ER_SU);
}

Ieee80211HeErSuSigAResult decodeHeErSuSigA(const std::vector<bool>& bits)
{
    return decodeSuSigA<Ieee80211HeErSuSigA, Ieee80211HeErSuSigAResult>(bits, Ieee80211HeSigFormat::ER_SU);
}

Ieee80211HeSigABitsResult encodeHeMuSigA(const Ieee80211HeMuSigA& value)
{
    Ieee80211HeSigABitsResult result;
    result.layout = makeSigALayout(Ieee80211HeSigFormat::MU);
    if (!validateMuSigA(result, value))
        return result;
    result.bits.assign(52, false);
    result.bits[0] = value.uplink;
    writeInteger(result.bits, 1, 3, value.heSigBMcs);
    result.bits[4] = value.heSigBDcm;
    writeInteger(result.bits, 5, 6, value.bssColor);
    writeInteger(result.bits, 11, 4, value.spatialReuse);
    writeInteger(result.bits, 15, 3, value.bandwidth);
    uint8_t count = value.heSigBCompression ? value.numberOfMuMimoUsers : value.numberOfHeSigBSymbols;
    writeInteger(result.bits, 18, 4, count - 1);
    result.bits[22] = value.heSigBCompression;
    writeInteger(result.bits, 23, 2, value.giLtfSize);
    result.bits[25] = value.doppler;
    writeInteger(result.bits, 26, 7, value.txop);
    result.bits[33] = true;
    uint8_t ltfCode = encodeHeLtfCount(value.numberOfHeLtfSymbols);
    if (value.doppler && value.midamblePeriodicity == 20)
        ltfCode |= 4;
    writeInteger(result.bits, 34, 3, ltfCode);
    result.bits[37] = value.ldpcExtraSymbolSegment;
    result.bits[38] = value.stbc;
    writeInteger(result.bits, 39, 2, value.preFecPaddingFactor);
    result.bits[41] = value.peDisambiguity;
    finishSigA(result.bits);
    setValid(result);
    return result;
}

Ieee80211HeMuSigAResult decodeHeMuSigA(const std::vector<bool>& bits)
{
    Ieee80211HeMuSigAResult result;
    result.layout = makeSigALayout(Ieee80211HeSigFormat::MU);
    if (!validateSigABits(result, bits))
        return result;
    if (!bits[33]) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_RESERVED_FIELD, "HE MU SIG-A reserved bit must be 1");
        return result;
    }
    auto& value = result.value;
    value.uplink = bits[0];
    value.heSigBMcs = readInteger(bits, 1, 3);
    value.heSigBDcm = bits[4];
    value.bssColor = readInteger(bits, 5, 6);
    value.spatialReuse = readInteger(bits, 11, 4);
    value.bandwidth = readInteger(bits, 15, 3);
    uint8_t encodedCount = readInteger(bits, 18, 4);
    value.heSigBCompression = bits[22];
    uint8_t count = encodedCount + 1;
    if (value.heSigBCompression) {
        value.numberOfHeSigBSymbols = 0;
        value.numberOfMuMimoUsers = count;
    }
    else {
        value.numberOfHeSigBSymbols = count;
        value.numberOfHeSigBSymbolsIsSaturated = encodedCount == 15;
        value.numberOfMuMimoUsers = 0;
    }
    value.giLtfSize = readInteger(bits, 23, 2);
    value.doppler = bits[25];
    value.txop = readInteger(bits, 26, 7);
    uint8_t ltfCode = readInteger(bits, 34, 3);
    if (value.doppler) {
        value.midamblePeriodicity = (ltfCode & 4) ? 20 : 10;
        ltfCode &= 3;
    }
    static const uint8_t ltfCounts[] = {1, 2, 4, 6, 8};
    if (ltfCode >= sizeof(ltfCounts)) {
        setError(result, Ieee80211HeSigCodecErrorCode::RESERVED_FIELD_VALUE, "HE MU SIG-A HE-LTF count code is reserved");
        return result;
    }
    value.numberOfHeLtfSymbols = ltfCounts[ltfCode];
    value.ldpcExtraSymbolSegment = bits[37];
    value.stbc = bits[38];
    value.preFecPaddingFactor = readInteger(bits, 39, 2);
    value.peDisambiguity = bits[41];
    if (!validateMuSigA(result, value))
        return result;
    setValid(result);
    return result;
}

Ieee80211HeSigABitsResult encodeHeTbSigA(const Ieee80211HeTbSigA& value)
{
    Ieee80211HeSigABitsResult result;
    result.layout = makeSigALayout(Ieee80211HeSigFormat::TB);
    if (value.bssColor > 63 || value.bandwidth > 3 || value.txop > 127 ||
            std::any_of(value.spatialReuse.begin(), value.spatialReuse.end(), [](uint8_t reuse) { return reuse > 15; })) {
        setError(result, Ieee80211HeSigCodecErrorCode::FIELD_OUT_OF_RANGE, "HE TB SIG-A field exceeds its encoded width");
        return result;
    }
    if (value.triggerReserved != 511) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_RESERVED_FIELD, "HE TB SIG-A Trigger reserved value must be all ones");
        return result;
    }
    if (!validateTbSpatialReuse(result, value))
        return result;
    result.bits.assign(52, false);
    writeInteger(result.bits, 1, 6, value.bssColor);
    for (size_t i = 0; i < value.spatialReuse.size(); ++i)
        writeInteger(result.bits, 7 + i * 4, 4, value.spatialReuse[i]);
    result.bits[23] = true;
    writeInteger(result.bits, 24, 2, value.bandwidth);
    writeInteger(result.bits, 26, 7, value.txop);
    writeInteger(result.bits, 33, 9, value.triggerReserved);
    finishSigA(result.bits);
    setValid(result);
    return result;
}

Ieee80211HeTbSigAResult decodeHeTbSigA(const std::vector<bool>& bits)
{
    Ieee80211HeTbSigAResult result;
    result.layout = makeSigALayout(Ieee80211HeSigFormat::TB);
    if (!validateSigABits(result, bits))
        return result;
    if (bits[0]) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_FORMAT, "HE TB SIG-A format bit must be 0");
        return result;
    }
    if (!bits[23]) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_RESERVED_FIELD, "HE TB SIG-A reserved bit must be 1");
        return result;
    }
    result.value.bssColor = readInteger(bits, 1, 6);
    for (size_t i = 0; i < result.value.spatialReuse.size(); ++i)
        result.value.spatialReuse[i] = readInteger(bits, 7 + i * 4, 4);
    result.value.bandwidth = readInteger(bits, 24, 2);
    result.value.txop = readInteger(bits, 26, 7);
    result.value.triggerReserved = readInteger(bits, 33, 9);
    if (result.value.triggerReserved != 511) {
        setError(result, Ieee80211HeSigCodecErrorCode::INVALID_RESERVED_FIELD, "HE TB SIG-A Trigger reserved value must be all ones");
        return result;
    }
    if (!validateTbSpatialReuse(result, result.value))
        return result;
    setValid(result);
    return result;
}

bool decodeTable27_27(uint8_t code, std::vector<std::pair<int, int>>& RUs, std::vector<int>& userCounts)
{
    RUs.clear();
    userCounts.clear();
    if (code <= 15) {
        if (code & 8) {
            RUs.push_back({52, 0}); userCounts.push_back(1);
        } else {
            RUs.push_back({26, 0}); userCounts.push_back(1);
            RUs.push_back({26, 26}); userCounts.push_back(1);
        }
        if (code & 4) {
            RUs.push_back({52, 54}); userCounts.push_back(1);
        } else {
            RUs.push_back({26, 54}); userCounts.push_back(1);
            RUs.push_back({26, 80}); userCounts.push_back(1);
        }
        RUs.push_back({26, 108}); userCounts.push_back(1);
        if (code & 2) {
            RUs.push_back({52, 136}); userCounts.push_back(1);
        } else {
            RUs.push_back({26, 136}); userCounts.push_back(1);
            RUs.push_back({26, 162}); userCounts.push_back(1);
        }
        if (code & 1) {
            RUs.push_back({52, 190}); userCounts.push_back(1);
        } else {
            RUs.push_back({26, 190}); userCounts.push_back(1);
            RUs.push_back({26, 216}); userCounts.push_back(1);
        }
        return true;
    }
    if (code >= 16 && code <= 23) {
        int y = code - 16;
        RUs.push_back({52, 0}); userCounts.push_back(1);
        RUs.push_back({52, 54}); userCounts.push_back(1);
        RUs.push_back({106, 136}); userCounts.push_back(y + 1);
        return true;
    }
    if (code >= 24 && code <= 31) {
        int y = code - 24;
        RUs.push_back({106, 0}); userCounts.push_back(y + 1);
        RUs.push_back({52, 136}); userCounts.push_back(1);
        RUs.push_back({52, 190}); userCounts.push_back(1);
        return true;
    }
    if (code >= 32 && code <= 39) {
        int y = code - 32;
        RUs.push_back({26, 0}); userCounts.push_back(1);
        RUs.push_back({26, 26}); userCounts.push_back(1);
        RUs.push_back({26, 54}); userCounts.push_back(1);
        RUs.push_back({26, 80}); userCounts.push_back(1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({106, 136}); userCounts.push_back(y + 1);
        return true;
    }
    if (code >= 40 && code <= 47) {
        int y = code - 40;
        RUs.push_back({26, 0}); userCounts.push_back(1);
        RUs.push_back({26, 26}); userCounts.push_back(1);
        RUs.push_back({52, 54}); userCounts.push_back(1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({106, 136}); userCounts.push_back(y + 1);
        return true;
    }
    if (code >= 48 && code <= 55) {
        int y = code - 48;
        RUs.push_back({52, 0}); userCounts.push_back(1);
        RUs.push_back({26, 54}); userCounts.push_back(1);
        RUs.push_back({26, 80}); userCounts.push_back(1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({106, 136}); userCounts.push_back(y + 1);
        return true;
    }
    if (code >= 56 && code <= 63) {
        int y = code - 56;
        RUs.push_back({52, 0}); userCounts.push_back(1);
        RUs.push_back({52, 54}); userCounts.push_back(1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({106, 136}); userCounts.push_back(y + 1);
        return true;
    }
    if (code >= 64 && code <= 71) {
        int y = code - 64;
        RUs.push_back({106, 0}); userCounts.push_back(y + 1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({26, 136}); userCounts.push_back(1);
        RUs.push_back({26, 162}); userCounts.push_back(1);
        RUs.push_back({26, 190}); userCounts.push_back(1);
        RUs.push_back({26, 216}); userCounts.push_back(1);
        return true;
    }
    if (code >= 72 && code <= 79) {
        int y = code - 72;
        RUs.push_back({106, 0}); userCounts.push_back(y + 1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({26, 136}); userCounts.push_back(1);
        RUs.push_back({26, 162}); userCounts.push_back(1);
        RUs.push_back({52, 190}); userCounts.push_back(1);
        return true;
    }
    if (code >= 80 && code <= 87) {
        int y = code - 80;
        RUs.push_back({106, 0}); userCounts.push_back(y + 1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({52, 136}); userCounts.push_back(1);
        RUs.push_back({26, 190}); userCounts.push_back(1);
        RUs.push_back({26, 216}); userCounts.push_back(1);
        return true;
    }
    if (code >= 88 && code <= 95) {
        int y = code - 88;
        RUs.push_back({106, 0}); userCounts.push_back(y + 1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({52, 136}); userCounts.push_back(1);
        RUs.push_back({52, 190}); userCounts.push_back(1);
        return true;
    }
    if (code >= 96 && code <= 111) {
        int val = code - 96;
        int n1 = ((val >> 2) & 3) + 1;
        int n2 = (val & 3) + 1;
        RUs.push_back({106, 0}); userCounts.push_back(n1);
        RUs.push_back({106, 136}); userCounts.push_back(n2);
        return true;
    }
    if (code == 112) {
        RUs.push_back({52, 0}); userCounts.push_back(1);
        RUs.push_back({52, 54}); userCounts.push_back(1);
        RUs.push_back({52, 136}); userCounts.push_back(1);
        RUs.push_back({52, 190}); userCounts.push_back(1);
        return true;
    }
    if (code == 113) {
        RUs.push_back({242, 0}); userCounts.push_back(0);
        return true;
    }
    if (code == 114) {
        RUs.push_back({484, 0}); userCounts.push_back(0);
        return true;
    }
    if (code == 115) {
        RUs.push_back({996, 0}); userCounts.push_back(0);
        return true;
    }
    if (code >= 128 && code <= 191) {
        int val = code - 128;
        int n1 = ((val >> 3) & 7) + 1;
        int n2 = (val & 7) + 1;
        RUs.push_back({106, 0}); userCounts.push_back(n1);
        RUs.push_back({26, 108}); userCounts.push_back(1);
        RUs.push_back({106, 136}); userCounts.push_back(n2);
        return true;
    }
    if (code >= 192 && code <= 199) {
        int n = code - 192 + 1;
        RUs.push_back({242, 0}); userCounts.push_back(n);
        return true;
    }
    if (code >= 200 && code <= 207) {
        int n = code - 200 + 1;
        RUs.push_back({484, 0}); userCounts.push_back(n);
        return true;
    }
    if (code >= 208 && code <= 215) {
        int n = code - 208 + 1;
        RUs.push_back({996, 0}); userCounts.push_back(n);
        return true;
    }
    return false;
}

Ieee80211HeSigBCommonFieldResult encodeHeSigBCommonField(
        const std::vector<Ieee80211HeRu>& rus, Hz channelBandwidth,
        const std::vector<bool>& puncturedSubchannels)
{
    Ieee80211HeSigBCommonFieldResult result;
    (void)puncturedSubchannels;
    auto catalog = getHeRuAllocationCatalog(Hz(0), channelBandwidth);
    std::vector<Ieee80211HeRu> canonicalRUs;
    std::vector<Ieee80211HeRu> uniqueRUs;
    std::map<std::pair<int, int>, int> userCountsByGeometry;
    for (const auto& ru : rus) {
        auto it = std::find_if(catalog.begin(), catalog.end(), [&](const Ieee80211HeRu& candidate) {
            return candidate.toneSize == ru.toneSize && candidate.toneOffset == ru.toneOffset;
        });
        if (it == catalog.end()) {
            result.error = "unknown HE RU tone geometry for the channel bandwidth";
            return result;
        }
        if (it->toneSize == 1992) {
            result.error = "2x996-tone RU requires compressed HE-SIG-B and is unsupported by this uncompressed Common-field encoder";
            return result;
        }
        auto key = std::make_pair(it->toneSize, it->toneOffset);
        canonicalRUs.push_back(*it);
        if (userCountsByGeometry[key]++ == 0)
            uniqueRUs.push_back(*it);
    }
    if (!validateHeRuLayout(uniqueRUs, channelBandwidth)) {
        result.error = "overlapping, out-of-band, or reserved HE RU allocation";
        return result;
    }

    auto subchannelRUs = getHeRuAllocationCatalog(Hz(0), channelBandwidth);
    subchannelRUs.erase(std::remove_if(subchannelRUs.begin(), subchannelRUs.end(),
        [](const Ieee80211HeRu& ru) { return ru.toneSize != 242; }), subchannelRUs.end());
    std::sort(subchannelRUs.begin(), subchannelRUs.end(), [](const Ieee80211HeRu& a, const Ieee80211HeRu& b) {
        return a.toneOffset < b.toneOffset;
    });

    int K = subchannelRUs.size();
    int numContentChannels = (channelBandwidth > Hz(20e6)) ? 2 : 1;
    result.commonField.contentChannels.resize(numContentChannels);

    for (const auto& ru : uniqueRUs) {
        if (ru.toneSize == 26) {
            if (ru.toneOffset == 485) {
                result.commonField.contentChannels[0].hasCenterRu = true;
                if (channelBandwidth == Hz(80e6))
                    result.commonField.contentChannels[1].hasCenterRu = true;
            }
            else if (ru.toneOffset == 1481 && numContentChannels > 1)
                result.commonField.contentChannels[1].hasCenterRu = true;
        }
    }

    for (int s = 0; s < K; ++s) {
        int c = s % 2;
        auto& cc = result.commonField.contentChannels[c];

        std::vector<std::pair<int, int>> partitionKeys;
        std::vector<int> partitionUsers;
        bool isWide = false;
        Ieee80211HeRu wideRU;

        for (const auto& ru : uniqueRUs) {
            if (ru.toneSize > 242) {
                if (ru.toneOffset <= subchannelRUs[s].toneOffset &&
                    ru.toneOffset + ru.toneSize >= subchannelRUs[s].toneOffset + 242) {
                    isWide = true;
                    wideRU = ru;
                    break;
                }
            }
            else if (ru.toneOffset >= subchannelRUs[s].toneOffset &&
                    ru.toneOffset + ru.toneSize <= subchannelRUs[s].toneOffset + 242)
                partitionKeys.push_back({ru.toneSize, ru.toneOffset - subchannelRUs[s].toneOffset});
        }

        if (isWide) {
            int totalUsers = userCountsByGeometry[{wideRU.toneSize, wideRU.toneOffset}];
            int n_c = 0;
            if (totalUsers > 0) {
                int n_cc1 = (totalUsers + 1) / 2;
                int n_cc2 = totalUsers / 2;
                n_c = (c == 0) ? n_cc1 : n_cc2;
            }
            uint8_t code = 0;
            if (wideRU.toneSize == 484)
                code = n_c == 0 ? 114 : 200 + (n_c - 1);
            else {
                bool isFirst = true;
                for (int prev_s = c; prev_s < s; prev_s += 2) {
                    if (wideRU.toneOffset <= subchannelRUs[prev_s].toneOffset &&
                        wideRU.toneOffset + wideRU.toneSize >= subchannelRUs[prev_s].toneOffset + 242) {
                        isFirst = false;
                        break;
                    }
                }
                code = isFirst ? (n_c == 0 ? 115 : 208 + (n_c - 1)) : 115;
            }
            cc.ruAllocationSubfields.push_back(code);
        }
        else {
            std::sort(partitionKeys.begin(), partitionKeys.end(),
                [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                    return a.second < b.second;
                });
            for (const auto& key : partitionKeys) {
                int userCount = 0;
                auto geometry = std::make_pair(key.first,
                        key.second + subchannelRUs[s].toneOffset);
                userCount = userCountsByGeometry[geometry];
                partitionUsers.push_back(userCount);
            }

            uint8_t selectedCode = 113;
            bool found = false;
            for (int code = 0; code <= 215; ++code) {
                std::vector<std::pair<int, int>> candidateRUs;
                std::vector<int> candidateUsers;
                if (decodeTable27_27(code, candidateRUs, candidateUsers) &&
                        candidateRUs.size() == partitionKeys.size()) {
                    bool match = true;
                    for (size_t i = 0; i < candidateRUs.size(); ++i) {
                        if (candidateRUs[i].first != partitionKeys[i].first ||
                            candidateRUs[i].second != partitionKeys[i].second ||
                            candidateUsers[i] != partitionUsers[i]) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        selectedCode = code;
                        found = true;
                        break;
                    }
                }
            }
            if (!found && partitionKeys.empty()) {
                selectedCode = 113;
                found = true;
            }
            if (!found) {
                result.error = "No valid standard HE-SIG-B RU allocation code found for the partition";
                return result;
            }
            cc.ruAllocationSubfields.push_back(selectedCode);
        }
    }

    result.commonField.rus = canonicalRUs;
    result.valid = true;
    return result;
}

Ieee80211HeSigBCommonFieldResult decodeHeSigBCommonField(
        const Ieee80211HeSigBCommonField& commonField, Hz channelCenterFrequency,
        Hz channelBandwidth)
{
    Ieee80211HeSigBCommonFieldResult result;
    auto centeredCatalog = getHeRuAllocationCatalog(channelCenterFrequency, channelBandwidth);
    auto subchannelRUs = centeredCatalog;
    subchannelRUs.erase(std::remove_if(subchannelRUs.begin(), subchannelRUs.end(),
        [](const Ieee80211HeRu& ru) { return ru.toneSize != 242; }), subchannelRUs.end());
    std::sort(subchannelRUs.begin(), subchannelRUs.end(), [](const Ieee80211HeRu& a, const Ieee80211HeRu& b) {
        return a.toneOffset < b.toneOffset;
    });

    int K = subchannelRUs.size();
    std::vector<Ieee80211HeRu> resolvedRUs;
    struct WideRuState {
        Ieee80211HeRu ru;
        int n1 = 0;
        int n2 = 0;
    };
    std::vector<WideRuState> wideRus;

    for (int s = 0; s < K; ++s) {
        int c = s % 2;
        int f = s / 2;
        if (c >= (int)commonField.contentChannels.size() || f >= (int)commonField.contentChannels[c].ruAllocationSubfields.size()) {
            result.error = "HE-SIG-B common field content channels index overflow";
            return result;
        }
        uint8_t code = commonField.contentChannels[c].ruAllocationSubfields[f];
        if (code == 115)
            continue;

        std::vector<std::pair<int, int>> decodedRUs;
        std::vector<int> decodedUserCounts;
        if (!decodeTable27_27(code, decodedRUs, decodedUserCounts)) {
            result.error = "Reserved or invalid RU allocation code in HE-SIG-B Common field";
            return result;
        }

        for (size_t i = 0; i < decodedRUs.size(); ++i) {
            int toneSize = decodedRUs[i].first;
            int relOffset = decodedRUs[i].second;
            int uCount = decodedUserCounts[i];

            if (toneSize > 242) {
                auto it = std::find_if(centeredCatalog.begin(), centeredCatalog.end(), [&](const Ieee80211HeRu& candidate) {
                    return candidate.toneSize == toneSize && candidate.toneOffset <= subchannelRUs[s].toneOffset &&
                           candidate.toneOffset + candidate.toneSize >= subchannelRUs[s].toneOffset + 242;
                });
                if (it == centeredCatalog.end()) {
                    result.error = "Wide RU cannot be resolved in centered catalog";
                    return result;
                }
                auto wideIt = std::find_if(wideRus.begin(), wideRus.end(), [&](const WideRuState& state) {
                    return state.ru.toneOffset == it->toneOffset && state.ru.toneSize == it->toneSize;
                });
                if (wideIt == wideRus.end()) {
                    WideRuState state;
                    state.ru = *it;
                    if (c == 0) state.n1 = uCount;
                    else state.n2 = uCount;
                    wideRus.push_back(state);
                }
                else {
                    if (c == 0) wideIt->n1 = uCount;
                    else wideIt->n2 = uCount;
                }
            }
            else {
                int absOffset = subchannelRUs[s].toneOffset + relOffset;
                auto it = std::find_if(centeredCatalog.begin(), centeredCatalog.end(), [&](const Ieee80211HeRu& candidate) {
                    return candidate.toneSize == toneSize && candidate.toneOffset == absOffset;
                });
                if (it == centeredCatalog.end()) {
                    result.error = "RU cannot be resolved in centered catalog";
                    return result;
                }
                for (int u = 0; u < uCount; ++u)
                    resolvedRUs.push_back(*it);
            }
        }
    }

    for (const auto& state : wideRus) {
        int totalUsers = state.n1 + state.n2;
        for (int u = 0; u < totalUsers; ++u)
            resolvedRUs.push_back(state.ru);
    }

    for (size_t c = 0; c < commonField.contentChannels.size(); ++c) {
        if (commonField.contentChannels[c].hasCenterRu) {
            int targetOffset = (c == 0) ? 485 : 1481;
            auto it = std::find_if(centeredCatalog.begin(), centeredCatalog.end(), [&](const Ieee80211HeRu& candidate) {
                return candidate.toneSize == 26 && candidate.toneOffset == targetOffset;
            });
            if (it != centeredCatalog.end())
                resolvedRUs.push_back(*it);
        }
    }

    std::sort(resolvedRUs.begin(), resolvedRUs.end(), [](const Ieee80211HeRu& a, const Ieee80211HeRu& b) {
        return a.toneOffset < b.toneOffset;
    });

    result.commonField.rus = resolvedRUs;
    result.commonField.contentChannels = commonField.contentChannels;
    result.valid = true;
    return result;
}

Ieee80211HeSigCodecResult encodeHeSigBRuAllocation(
        const std::vector<Ieee80211HeRu>& rus, Hz channelBandwidth)
{
    Ieee80211HeSigCodecResult result;
    auto commonFieldResult = encodeHeSigBCommonField(rus, channelBandwidth);
    if (!commonFieldResult) {
        result.error = commonFieldResult.error;
        return result;
    }
    result.allocation.rus = commonFieldResult.commonField.rus;
    for (const auto& cc : commonFieldResult.commonField.contentChannels)
        for (uint8_t code : cc.ruAllocationSubfields)
            result.allocation.allocationCodes.push_back(code);
    result.valid = true;
    return result;
}

Ieee80211HeSigCodecResult decodeHeSigBRuAllocation(
        const std::vector<uint8_t>& allocationCodes, Hz channelCenterFrequency,
        Hz channelBandwidth)
{
    Ieee80211HeSigCodecResult result;
    int numContentChannels = (channelBandwidth > Hz(20e6)) ? 2 : 1;
    int N = (channelBandwidth >= Hz(160e6)) ? 4 : (channelBandwidth >= Hz(80e6)) ? 2 : 1;

    if ((int)allocationCodes.size() == numContentChannels * N) {
        Ieee80211HeSigBCommonField commonField;
        commonField.contentChannels.resize(numContentChannels);
        int idx = 0;
        for (int c = 0; c < numContentChannels; ++c)
            for (int f = 0; f < N; ++f)
                commonField.contentChannels[c].ruAllocationSubfields.push_back(allocationCodes[idx++]);
        auto decodedCF = decodeHeSigBCommonField(commonField, channelCenterFrequency, channelBandwidth);
        if (decodedCF) {
            result.allocation.rus = decodedCF.commonField.rus;
            result.allocation.allocationCodes = allocationCodes;
            result.valid = true;
            return result;
        }
    }

    auto zeroCenteredCatalog = getHeRuAllocationCatalog(Hz(0), channelBandwidth);
    auto centeredCatalog = getHeRuAllocationCatalog(channelCenterFrequency, channelBandwidth);
    for (uint8_t code : allocationCodes) {
        if (code >= zeroCenteredCatalog.size()) {
            result.error = "reserved HE-SIG-B RU allocation code";
            return result;
        }
        const auto& encoded = zeroCenteredCatalog[code];
        auto it = std::find_if(centeredCatalog.begin(), centeredCatalog.end(), [&] (const auto& candidate) {
            return candidate.toneSize == encoded.toneSize &&
                    candidate.toneOffset == encoded.toneOffset;
        });
        if (it == centeredCatalog.end()) {
            result.error = "HE-SIG-B RU allocation cannot be resolved";
            return result;
        }
        result.allocation.rus.push_back(*it);
        result.allocation.allocationCodes.push_back(code);
    }
    if (!validateHeRuLayout(result.allocation.rus, channelBandwidth)) {
        result.error = "decoded HE-SIG-B RU allocation overlaps";
        result.allocation = {};
        return result;
    }
    result.valid = true;
    return result;
}

} // namespace physicallayer
} // namespace inet
