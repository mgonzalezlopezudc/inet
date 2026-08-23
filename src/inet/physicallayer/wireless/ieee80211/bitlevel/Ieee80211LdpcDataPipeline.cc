//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/bitlevel/Ieee80211LdpcDataPipeline.h"

#include <cmath>
#include <utility>

#include "inet/common/ShortBitVector.h"
#include "inet/physicallayer/wireless/common/radio/bitlevel/AdditiveScrambler.h"
#include "inet/physicallayer/wireless/common/radio/bitlevel/AdditiveScrambling.h"

namespace inet {
namespace physicallayer {

namespace {

constexpr int SERVICE_FIELD_LENGTH = 16;

void validateCrcBitLength(size_t bitLength)
{
    if (bitLength != 20 && bitLength != 21 && bitLength != 23)
        throw cRuntimeError("IEEE 802.11 VHT-SIG-B CRC expects 20, 21, or 23 protected bits, got %zu", bitLength);
}

} // namespace

void Ieee80211LdpcDataPipeline::validateSuPlan(const Ieee80211DataEncodingPlan& plan,
        int psduBitLength, int stbcSymbolFactor)
{
    if (plan.getFecType() != Ieee80211FecType::LDPC)
        throw cRuntimeError("IEEE 802.11 LDPC Data pipeline requires an LDPC plan");
    if (plan.getPhyFormat() != Ieee80211PhyFormat::HT &&
        plan.getPhyFormat() != Ieee80211PhyFormat::VHT_SU)
        throw cRuntimeError("IEEE 802.11 LDPC Data pipeline supports only HT and VHT-SU plans");
    if (stbcSymbolFactor != 1)
        throw cRuntimeError("IEEE 802.11 LDPC Data pipeline does not support STBC (symbol factor %d)", stbcSymbolFactor);
    if (psduBitLength < 0)
        throw cRuntimeError("IEEE 802.11 LDPC Data pipeline requires a nonnegative PSDU length");
    if (plan.getPhyFormat() == Ieee80211PhyFormat::HT) {
        if (plan.getUncodedDataBits() != SERVICE_FIELD_LENGTH + psduBitLength)
            throw cRuntimeError("HT LDPC plan Npld=%d does not equal SERVICE plus PSDU length %d",
                                plan.getUncodedDataBits(), SERVICE_FIELD_LENGTH + psduBitLength);
    }
    else if (plan.getUncodedDataBits() < SERVICE_FIELD_LENGTH + psduBitLength) {
        throw cRuntimeError("VHT-SU LDPC plan Npld=%d is shorter than SERVICE plus PSDU length %d",
                            plan.getUncodedDataBits(), SERVICE_FIELD_LENGTH + psduBitLength);
    }
}

void Ieee80211LdpcDataPipeline::appendBits(BitVector& destination, const BitVector& source)
{
    for (unsigned int i = 0; i < source.getSize(); i++)
        destination.appendBit(source.getBit(i));
}

BitVector Ieee80211LdpcDataPipeline::makeServiceField(Ieee80211PhyFormat phyFormat,
        uint8_t vhtSigBCrc)
{
    BitVector service;
    if (phyFormat == Ieee80211PhyFormat::HT) {
        service.appendBit(false, SERVICE_FIELD_LENGTH);
        if (vhtSigBCrc != 0)
            throw cRuntimeError("HT SERVICE field cannot carry a VHT-SIG-B CRC");
    }
    else if (phyFormat == Ieee80211PhyFormat::VHT_SU) {
        // Table 21-16: B0-B6 scrambler initialization and B7 reserved are
        // zero before scrambling; B8-B15 carry c7 through c0 of the VHT-SIG-B
        // CRC in that transmission order (IEEE 802.11-2024, 21.3.10.3).
        service.appendBit(false, 8);
        for (int i = 7; i >= 0; i--)
            service.appendBit((vhtSigBCrc >> i) & 1);
    }
    else
        throw cRuntimeError("Unsupported IEEE 802.11 LDPC SERVICE format");
    return service;
}

uint8_t Ieee80211LdpcDataPipeline::computeVhtSigBCrc(const std::vector<bool>& protectedBits)
{
    // IEEE Std 802.11-2024 21.3.10.3, Equation (21-59):
    // CRC(D) = (M(D) XOR I(D)) D^8 mod (D^8 + D^2 + D + 1), complemented.
    // protectedBits are m0...m(N-1) in transmission order. I(D) complements
    // the first eight coefficients, as in the standard's definition.
    validateCrcBitLength(protectedBits.size());
    uint64_t message = 0;
    for (bool bit : protectedBits)
        message = (message << 1) | uint64_t(bit);
    const int n = protectedBits.size();
    uint64_t dividend = (message ^ (uint64_t(0xff) << (n - 8))) << 8;
    constexpr uint64_t generator = (uint64_t(1) << 8) | (uint64_t(1) << 2) |
                                   (uint64_t(1) << 1) | uint64_t(1);
    for (int degree = n + 7; degree >= 8; degree--)
        if (dividend & (uint64_t(1) << degree))
            dividend ^= generator << (degree - 8);
    return static_cast<uint8_t>(~dividend);
}

BitVector Ieee80211LdpcDataPipeline::buildServiceField(Ieee80211PhyFormat phyFormat,
        uint8_t vhtSigBCrc)
{
    return makeServiceField(phyFormat, vhtSigBCrc);
}

BitVector Ieee80211LdpcDataPipeline::scrambleOrDescramble(const BitVector& bits,
        uint8_t scramblerRegisterState)
{
    if (scramblerRegisterState == 0 || scramblerRegisterState > 0x7f)
        throw cRuntimeError("IEEE 802.11 LDPC scrambler register state must be a nonzero 7-bit value");

    // The generator is x^7 + x^4 + 1.  Constructing this small value object
    // per operation makes the register state explicit and prevents mutable scrambler
    // state from leaking between transmissions.
    AdditiveScrambling scrambling(ShortBitVector(scramblerRegisterState, 7),
                                  ShortBitVector("0001001"));
    AdditiveScrambler scrambler(&scrambling);
    return scrambler.scramble(bits);
}

BitVector Ieee80211LdpcDataPipeline::buildPreScrambledData(const BitVector& psduBits,
        const Ieee80211DataEncodingPlan& plan, uint8_t vhtSigBCrc, int stbcSymbolFactor)
{
    validateSuPlan(plan, psduBits.getSize(), stbcSymbolFactor);
    BitVector result = makeServiceField(plan.getPhyFormat(), vhtSigBCrc);
    appendBits(result, psduBits);
    if (result.getSize() > static_cast<unsigned int>(plan.getUncodedDataBits()))
        throw cRuntimeError("IEEE 802.11 LDPC SERVICE and PSDU exceed the planned Npld");
    result.appendBit(false, plan.getUncodedDataBits() - result.getSize());
    return result;
}

BitVector Ieee80211LdpcDataPipeline::scramble(const BitVector& bits,
        uint8_t scramblerRegisterState)
{
    return scrambleOrDescramble(bits, scramblerRegisterState);
}

BitVector Ieee80211LdpcDataPipeline::descramble(const BitVector& bits,
        uint8_t scramblerRegisterState)
{
    return scrambleOrDescramble(bits, scramblerRegisterState);
}

uint8_t Ieee80211LdpcDataPipeline::recoverScramblerRegisterState(const BitVector& scrambledService)
{
    if (scrambledService.getSize() < 7)
        throw cRuntimeError("IEEE 802.11 LDPC scrambler recovery requires seven scrambled SERVICE bits");

    BitVector serviceBits;
    serviceBits.appendBit(false, 7);
    for (uint8_t state = 1; state <= 0x7f; state++) {
        auto sequence = scrambleOrDescramble(serviceBits, state);
        bool matches = true;
        for (int i = 0; i < 7; i++) {
            if (sequence.getBit(i) != scrambledService.getBit(i)) {
                matches = false;
                break;
            }
        }
        if (matches)
            return state;
    }
    throw cRuntimeError("IEEE 802.11 LDPC scrambled SERVICE bits do not identify a valid scrambler state");
}

Ieee80211LdpcEncodedDataField Ieee80211LdpcDataPipeline::encodeAndMap(
        const BitVector& psduBits, const Ieee80211DataEncodingPlan& plan,
        const std::vector<int>& bitsPerSubcarrier, int bandwidthMhz,
        uint8_t vhtSigBCrc, int stbcSymbolFactor,
        const Ieee80211LdpcDataCoder& coder, uint8_t scramblerRegisterState)
{
    auto preScrambled = buildPreScrambledData(psduBits, plan, vhtSigBCrc, stbcSymbolFactor);
    auto scrambled = scramble(preScrambled, scramblerRegisterState);
    auto mapped = Ieee80211LdpcBitPipeline::encodeAndMap(scrambled, plan,
                                                         bitsPerSubcarrier, bandwidthMhz, coder);
    return {std::move(preScrambled), std::move(scrambled), std::move(mapped)};
}

Ieee80211LdpcDecodedDataField Ieee80211LdpcDataPipeline::inverseMapAndDecode(
        const Ieee80211LdpcMappedReliabilities& mapped,
        const Ieee80211DataEncodingPlan& plan,
        const std::vector<int>& bitsPerSubcarrier, int bandwidthMhz,
        int psduBitLength, bool validateVhtSigBCrc,
        uint8_t expectedVhtSigBCrc, int stbcSymbolFactor,
        const Ieee80211LdpcDataCoder& coder, uint8_t scramblerRegisterState)
{
    validateSuPlan(plan, psduBitLength, stbcSymbolFactor);
    if (plan.getPhyFormat() == Ieee80211PhyFormat::HT && validateVhtSigBCrc)
        throw cRuntimeError("HT LDPC receive path cannot validate a VHT-SIG-B CRC");

    auto decoded = Ieee80211LdpcBitPipeline::inverseMapAndDecode(mapped, plan,
                                                                  bitsPerSubcarrier, bandwidthMhz, coder);
    if (!decoded.converged)
        return {BitVector(), BitVector(), false, decoded.iterations};
    if (decoded.informationBits.getSize() != static_cast<unsigned int>(plan.getUncodedDataBits()))
        throw cRuntimeError("IEEE 802.11 LDPC decoder returned %u bits, expected Npld=%d",
                            decoded.informationBits.getSize(), plan.getUncodedDataBits());

    // A zero state is an explicit receive-side request to recover the
    // register from the transmitted SERVICE sequence.  It is not accepted
    // by scrambleOrDescramble itself, and therefore cannot be confused with
    // an on-air state.
    if (scramblerRegisterState == 0) {
        try {
            scramblerRegisterState = recoverScramblerRegisterState(decoded.informationBits);
        }
        catch (const cRuntimeError&) {
            return {BitVector(), BitVector(), false, decoded.iterations};
        }
    }
    auto descrambled = descramble(decoded.informationBits, scramblerRegisterState);
    auto service = makeServiceField(plan.getPhyFormat(), expectedVhtSigBCrc);
    // VHT SERVICE B7 is reserved and ignored on receive (Table 21-16).
    // B0-B6 still validate the recovered scrambler sequence; B8-B15 carry
    // the separately checked VHT-SIG-B CRC.
    int serviceBitsToCheck = plan.getPhyFormat() == Ieee80211PhyFormat::HT ? SERVICE_FIELD_LENGTH : 7;
    for (int i = 0; i < serviceBitsToCheck; i++) {
        if (descrambled.getBit(i) != service.getBit(i))
            return {BitVector(), BitVector(), false, decoded.iterations};
    }
    if (plan.getPhyFormat() == Ieee80211PhyFormat::VHT_SU && validateVhtSigBCrc) {
        for (int i = 8; i < SERVICE_FIELD_LENGTH; i++) {
            if (descrambled.getBit(i) != service.getBit(i))
                return {BitVector(), BitVector(), false, decoded.iterations};
        }
    }
    for (int i = SERVICE_FIELD_LENGTH + psduBitLength; i < plan.getUncodedDataBits(); i++) {
        if (descrambled.getBit(i))
            return {BitVector(), BitVector(), false, decoded.iterations};
    }

    BitVector psduBits;
    for (int i = 0; i < psduBitLength; i++)
        psduBits.appendBit(descrambled.getBit(SERVICE_FIELD_LENGTH + i));
    return {std::move(descrambled), std::move(psduBits), true, decoded.iterations};
}

} // namespace physicallayer
} // namespace inet
