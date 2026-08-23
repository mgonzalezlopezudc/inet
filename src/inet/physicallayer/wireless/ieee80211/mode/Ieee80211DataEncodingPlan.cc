//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DataEncodingPlan.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>

namespace inet {
namespace physicallayer {

namespace {

int checkedInt(int64_t value, const char *quantity)
{
    if (value < 0 || value > std::numeric_limits<int>::max())
        throw cRuntimeError("IEEE 802.11 LDPC %s is outside the supported integer range", quantity);
    return static_cast<int>(value);
}

int ceilDiv(int64_t numerator, int64_t denominator)
{
    if (numerator < 0 || denominator <= 0)
        throw cRuntimeError("Invalid operands for IEEE 802.11 LDPC ceiling division");
    return checkedInt((numerator + denominator - 1) / denominator, "ceiling-division result");
}

std::vector<int> distribute(int total, int count)
{
    if (total < 0 || count <= 0)
        throw cRuntimeError("Invalid IEEE 802.11 LDPC distribution total=%d count=%d", total, count);
    std::vector<int> result(count, total / count);
    for (int i = 0; i < total % count; i++)
        result[i]++;
    return result;
}

void validateLdpcRate(const Ieee80211CodeRate& rate)
{
    int p = rate.getNumerator();
    int q = rate.getDenominator();
    if (!((p == 1 && q == 2) || (p == 2 && q == 3) ||
          (p == 3 && q == 4) || (p == 5 && q == 6)))
        throw cRuntimeError("Unsupported IEEE 802.11 LDPC code rate %d/%d", p, q);
}

} // namespace

Ieee80211CodeRate::Ieee80211CodeRate(int numerator, int denominator)
{
    if (numerator <= 0 || denominator <= 0 || numerator >= denominator)
        throw cRuntimeError("Invalid IEEE 802.11 code rate %d/%d", numerator, denominator);
    int divisor = std::gcd(numerator, denominator);
    this->numerator = numerator / divisor;
    this->denominator = denominator / divisor;
}

int Ieee80211CodeRate::multiplyExact(int value) const
{
    int64_t product = int64_t(value) * numerator;
    if (product % denominator != 0)
        throw cRuntimeError("IEEE 802.11 value %d is not exactly divisible at code rate %d/%d", value, numerator, denominator);
    return checkedInt(product / denominator, "rate product");
}

bool Ieee80211CodeRate::operator==(const Ieee80211CodeRate& other) const
{
    return numerator == other.numerator && denominator == other.denominator;
}

Ieee80211LdpcCodewordPlan::Ieee80211LdpcCodewordPlan(int codewordLength, int informationLength,
        int shortenedBits, int puncturedBits, int repeatedBits) :
    codewordLength(codewordLength),
    informationLength(informationLength),
    shortenedBits(shortenedBits),
    puncturedBits(puncturedBits),
    repeatedBits(repeatedBits)
{
    if (codewordLength != 648 && codewordLength != 1296 && codewordLength != 1944)
        throw cRuntimeError("Unsupported IEEE 802.11 LDPC codeword length %d", codewordLength);
    if (informationLength <= 0 || informationLength >= codewordLength)
        throw cRuntimeError("Invalid IEEE 802.11 LDPC information length %d", informationLength);
    if (shortenedBits < 0 || shortenedBits > informationLength)
        throw cRuntimeError("Invalid IEEE 802.11 LDPC shortening count %d", shortenedBits);
    if (puncturedBits < 0 || puncturedBits > codewordLength - informationLength)
        throw cRuntimeError("Invalid IEEE 802.11 LDPC puncturing count %d", puncturedBits);
    if (repeatedBits < 0 || (puncturedBits > 0 && repeatedBits > 0))
        throw cRuntimeError("Invalid IEEE 802.11 LDPC puncturing/repetition counts %d/%d", puncturedBits, repeatedBits);
}

bool Ieee80211LdpcCodewordPlan::operator==(const Ieee80211LdpcCodewordPlan& other) const
{
    return codewordLength == other.codewordLength && informationLength == other.informationLength &&
           shortenedBits == other.shortenedBits && puncturedBits == other.puncturedBits &&
           repeatedBits == other.repeatedBits;
}

std::ostream& operator<<(std::ostream& stream, const Ieee80211LdpcCodewordPlan& plan)
{
    return stream << "{N=" << plan.codewordLength << ", K=" << plan.informationLength
                  << ", shortened=" << plan.shortenedBits << ", punctured=" << plan.puncturedBits
                  << ", repeated=" << plan.repeatedBits << '}';
}

Ieee80211DataEncodingPlan::Ieee80211DataEncodingPlan(Ieee80211FecType fecType, Ieee80211PhyFormat phyFormat,
        int uncodedDataBits, int availableEncodedBits, int initialNumberOfSymbols,
        int numberOfSymbols, int numberOfCodedBitsPerSymbol, bool additionalCapacityApplied,
        const std::vector<Ieee80211LdpcCodewordPlan>& codewords) :
    fecType(fecType),
    phyFormat(phyFormat),
    uncodedDataBits(uncodedDataBits),
    availableEncodedBits(availableEncodedBits),
    initialNumberOfSymbols(initialNumberOfSymbols),
    numberOfSymbols(numberOfSymbols),
    numberOfCodedBitsPerSymbol(numberOfCodedBitsPerSymbol),
    additionalCapacityApplied(additionalCapacityApplied),
    codewords(codewords)
{
    if (fecType != Ieee80211FecType::BCC && fecType != Ieee80211FecType::LDPC)
        throw cRuntimeError("Unsupported IEEE 802.11 FEC type");
    if (phyFormat != Ieee80211PhyFormat::HT && phyFormat != Ieee80211PhyFormat::VHT_SU)
        throw cRuntimeError("Unsupported IEEE 802.11 PHY format");
    if (uncodedDataBits < 0 || availableEncodedBits < 0 || initialNumberOfSymbols < 0 ||
        numberOfSymbols < 0 || numberOfCodedBitsPerSymbol <= 0)
        throw cRuntimeError("Invalid IEEE 802.11 data encoding plan dimensions");
    if (availableEncodedBits != numberOfSymbols * numberOfCodedBitsPerSymbol)
        throw cRuntimeError("IEEE 802.11 encoded-bit and symbol counts disagree");
    if (additionalCapacityApplied != (numberOfSymbols > initialNumberOfSymbols))
        throw cRuntimeError("IEEE 802.11 additional-capacity flag and symbol counts disagree");
    if (fecType == Ieee80211FecType::LDPC) {
        if (codewords.empty())
            throw cRuntimeError("IEEE 802.11 LDPC plan has no codewords");
        int dataBits = 0;
        int transmittedBits = 0;
        for (const auto& codeword : codewords) {
            dataBits += codeword.getDataBits();
            transmittedBits += codeword.getTransmittedBits();
        }
        if (dataBits != uncodedDataBits || transmittedBits != availableEncodedBits)
            throw cRuntimeError("IEEE 802.11 LDPC codeword totals disagree with the PPDU plan");
    }
    else if (!codewords.empty())
        throw cRuntimeError("IEEE 802.11 BCC plan must not contain LDPC codewords");
}

int Ieee80211DataEncodingPlan::getShortenedBits() const
{
    int result = 0;
    for (const auto& codeword : codewords)
        result += codeword.getShortenedBits();
    return result;
}

int Ieee80211DataEncodingPlan::getPuncturedBits() const
{
    int result = 0;
    for (const auto& codeword : codewords)
        result += codeword.getPuncturedBits();
    return result;
}

int Ieee80211DataEncodingPlan::getRepeatedBits() const
{
    int result = 0;
    for (const auto& codeword : codewords)
        result += codeword.getRepeatedBits();
    return result;
}

bool Ieee80211DataEncodingPlan::operator==(const Ieee80211DataEncodingPlan& other) const
{
    return fecType == other.fecType && phyFormat == other.phyFormat &&
           uncodedDataBits == other.uncodedDataBits && availableEncodedBits == other.availableEncodedBits &&
           initialNumberOfSymbols == other.initialNumberOfSymbols && numberOfSymbols == other.numberOfSymbols &&
           numberOfCodedBitsPerSymbol == other.numberOfCodedBitsPerSymbol &&
           additionalCapacityApplied == other.additionalCapacityApplied && codewords == other.codewords;
}

std::ostream& operator<<(std::ostream& stream, const Ieee80211DataEncodingPlan& plan)
{
    stream << "{fec=" << (plan.fecType == Ieee80211FecType::LDPC ? "LDPC" : "BCC")
           << ", format=";
    switch (plan.phyFormat) {
        case Ieee80211PhyFormat::LEGACY: stream << "legacy"; break;
        case Ieee80211PhyFormat::HT: stream << "HT"; break;
        case Ieee80211PhyFormat::VHT_SU: stream << "VHT-SU"; break;
    }
    stream << ", uncodedBits=" << plan.uncodedDataBits
           << ", availableBits=" << plan.availableEncodedBits
           << ", initialSymbols=" << plan.initialNumberOfSymbols
           << ", symbols=" << plan.numberOfSymbols
           << ", NCBPS=" << plan.numberOfCodedBitsPerSymbol
           << ", additionalCapacity=" << plan.additionalCapacityApplied
           << ", codewords=[";
    for (size_t i = 0; i < plan.codewords.size(); i++) {
        if (i != 0)
            stream << ", ";
        stream << plan.codewords[i];
    }
    return stream << "]}";
}

Ieee80211LdpcCodeSelection Ieee80211LdpcPlanner::selectCodewords(int uncodedDataBits,
        int availableEncodedBits, const Ieee80211CodeRate& codeRate)
{
    validateLdpcRate(codeRate);
    if (uncodedDataBits <= 0 || availableEncodedBits <= 0)
        throw cRuntimeError("IEEE 802.11 LDPC selection requires positive data and available-bit counts");
    int64_t npld = uncodedDataBits;
    int64_t navbits = availableEncodedBits;
    int64_t p = codeRate.getNumerator();
    int64_t q = codeRate.getDenominator();

    if (navbits <= 648)
        return navbits * q >= npld * q + 912 * (q - p) ?
               Ieee80211LdpcCodeSelection{1, 1296} : Ieee80211LdpcCodeSelection{1, 648};
    if (navbits <= 1296)
        return navbits * q >= npld * q + 1464 * (q - p) ?
               Ieee80211LdpcCodeSelection{1, 1944} : Ieee80211LdpcCodeSelection{1, 1296};
    if (navbits <= 1944)
        return {1, 1944};
    if (navbits <= 2592)
        return navbits * q >= npld * q + 2916 * (q - p) ?
               Ieee80211LdpcCodeSelection{2, 1944} : Ieee80211LdpcCodeSelection{2, 1296};
    return {ceilDiv(npld * q, 1944 * p), 1944};
}

Ieee80211DataEncodingPlan Ieee80211LdpcPlanner::compute(Ieee80211PhyFormat phyFormat,
        int uncodedDataBits, int initialAvailableEncodedBits, int numberOfCodedBitsPerSymbol,
        const Ieee80211CodeRate& codeRate, int stbcSymbolFactor)
{
    validateLdpcRate(codeRate);
    if (uncodedDataBits <= 0 || initialAvailableEncodedBits <= 0 || numberOfCodedBitsPerSymbol <= 0)
        throw cRuntimeError("IEEE 802.11 LDPC planning dimensions must be positive");
    if (stbcSymbolFactor != 1 && stbcSymbolFactor != 2)
        throw cRuntimeError("IEEE 802.11 LDPC STBC symbol factor must be 1 or 2");
    if (initialAvailableEncodedBits % numberOfCodedBitsPerSymbol != 0)
        throw cRuntimeError("IEEE 802.11 LDPC available bits do not form complete OFDM symbols");

    auto selection = selectCodewords(uncodedDataBits, initialAvailableEncodedBits, codeRate);
    int p = codeRate.getNumerator();
    int q = codeRate.getDenominator();
    int informationLength = codeRate.multiplyExact(selection.codewordLength);
    int informationCapacity = checkedInt(int64_t(selection.numberOfCodewords) * informationLength, "information capacity");
    int shortenedBits = std::max(0, informationCapacity - uncodedDataBits);
    int availableEncodedBits = initialAvailableEncodedBits;
    int puncturedBits = std::max(0, checkedInt(int64_t(selection.numberOfCodewords) * selection.codewordLength,
                                              "codeword capacity") - availableEncodedBits - shortenedBits);

    int64_t parityUnits = int64_t(selection.numberOfCodewords) * selection.codewordLength * (q - p);
    bool needsAdditionalCapacity =
        ((int64_t(10) * q * puncturedBits > parityUnits) &&
         (int64_t(5) * (q - p) * shortenedBits < int64_t(6) * p * puncturedBits)) ||
        (int64_t(10) * q * puncturedBits > int64_t(3) * parityUnits);
    if (needsAdditionalCapacity) {
        availableEncodedBits = checkedInt(int64_t(availableEncodedBits) +
                                          int64_t(numberOfCodedBitsPerSymbol) * stbcSymbolFactor,
                                          "available encoded bits");
        puncturedBits = std::max(0, checkedInt(int64_t(selection.numberOfCodewords) * selection.codewordLength,
                                              "codeword capacity") - availableEncodedBits - shortenedBits);
    }

    int parityCapacity = checkedInt(int64_t(selection.numberOfCodewords) *
                                    selection.codewordLength * (q - p) / q,
                                    "parity capacity");
    int repeatedBits = std::max(0, availableEncodedBits - parityCapacity - uncodedDataBits);
    if (puncturedBits > 0 && repeatedBits > 0)
        throw cRuntimeError("IEEE 802.11 LDPC plan simultaneously punctures and repeats bits");

    auto shortenedDistribution = distribute(shortenedBits, selection.numberOfCodewords);
    auto puncturedDistribution = distribute(puncturedBits, selection.numberOfCodewords);
    auto repeatedDistribution = distribute(repeatedBits, selection.numberOfCodewords);
    std::vector<Ieee80211LdpcCodewordPlan> codewords;
    codewords.reserve(selection.numberOfCodewords);
    for (int i = 0; i < selection.numberOfCodewords; i++)
        codewords.emplace_back(selection.codewordLength, informationLength,
                               shortenedDistribution[i], puncturedDistribution[i], repeatedDistribution[i]);

    int initialNumberOfSymbols = initialAvailableEncodedBits / numberOfCodedBitsPerSymbol;
    int numberOfSymbols = availableEncodedBits / numberOfCodedBitsPerSymbol;
    return {Ieee80211FecType::LDPC, phyFormat, uncodedDataBits, availableEncodedBits,
            initialNumberOfSymbols, numberOfSymbols, numberOfCodedBitsPerSymbol,
            needsAdditionalCapacity, codewords};
}

Ieee80211DataEncodingPlan Ieee80211LdpcPlanner::computeHt(int psduOctets,
        int numberOfCodedBitsPerSymbol, const Ieee80211CodeRate& codeRate, int stbcSymbolFactor)
{
    if (psduOctets < 0)
        throw cRuntimeError("IEEE 802.11 HT PSDU length must be nonnegative");
    validateLdpcRate(codeRate);
    int uncodedDataBits = checkedInt(int64_t(psduOctets) * 8 + 16, "HT Npld");
    int denominator = checkedInt(int64_t(numberOfCodedBitsPerSymbol) *
                                 codeRate.getNumerator() * stbcSymbolFactor,
                                 "HT symbol information capacity numerator");
    int symbolGroups = ceilDiv(int64_t(uncodedDataBits) * codeRate.getDenominator(), denominator);
    int initialAvailableEncodedBits = checkedInt(int64_t(numberOfCodedBitsPerSymbol) *
                                                 stbcSymbolFactor * symbolGroups,
                                                 "HT Navbits");
    return compute(Ieee80211PhyFormat::HT, uncodedDataBits, initialAvailableEncodedBits,
                   numberOfCodedBitsPerSymbol, codeRate, stbcSymbolFactor);
}

Ieee80211DataEncodingPlan Ieee80211LdpcPlanner::computeVhtSu(int apepOctets,
        int numberOfCodedBitsPerSymbol, int numberOfDataBitsPerSymbol,
        const Ieee80211CodeRate& codeRate, int stbcSymbolFactor)
{
    if (apepOctets < 0 || numberOfCodedBitsPerSymbol <= 0 || numberOfDataBitsPerSymbol <= 0)
        throw cRuntimeError("Invalid IEEE 802.11 VHT-SU LDPC planning dimensions");
    if (stbcSymbolFactor != 1 && stbcSymbolFactor != 2)
        throw cRuntimeError("IEEE 802.11 VHT-SU STBC symbol factor must be 1 or 2");
    int numberOfSymbols = checkedInt(int64_t(stbcSymbolFactor) *
                                     ceilDiv(int64_t(apepOctets) * 8 + 16,
                                             int64_t(stbcSymbolFactor) * numberOfDataBitsPerSymbol),
                                     "VHT initial symbol count");
    int uncodedDataBits = checkedInt(int64_t(numberOfSymbols) * numberOfDataBitsPerSymbol, "VHT Npld");
    int initialAvailableEncodedBits = checkedInt(int64_t(numberOfSymbols) * numberOfCodedBitsPerSymbol, "VHT Navbits");
    return compute(Ieee80211PhyFormat::VHT_SU, uncodedDataBits, initialAvailableEncodedBits,
                   numberOfCodedBitsPerSymbol, codeRate, stbcSymbolFactor);
}

Ieee80211DataEncodingPlan Ieee80211LdpcPlanner::computeVhtSuFromReceivedSymbols(
        int initialNumberOfSymbols, int numberOfCodedBitsPerSymbol,
        int numberOfDataBitsPerSymbol, const Ieee80211CodeRate& codeRate,
        int stbcSymbolFactor)
{
    if (initialNumberOfSymbols <= 0 || numberOfCodedBitsPerSymbol <= 0 ||
        numberOfDataBitsPerSymbol <= 0)
        throw cRuntimeError("Invalid received IEEE 802.11 VHT-SU LDPC symbol-plan dimensions");
    if (stbcSymbolFactor != 1 && stbcSymbolFactor != 2)
        throw cRuntimeError("IEEE 802.11 VHT-SU STBC symbol factor must be 1 or 2");

    // For reception, Npld and Navbits are reconstructed directly from the
    // received OFDM symbol count. This is the inverse of the transmitter's
    // initial-symbol calculation, without consulting Packet length or a
    // sender-side plan/tag.
    int uncodedDataBits = checkedInt(int64_t(initialNumberOfSymbols) * numberOfDataBitsPerSymbol,
                                     "received VHT Npld");
    int initialAvailableEncodedBits = checkedInt(int64_t(initialNumberOfSymbols) * numberOfCodedBitsPerSymbol,
                                                 "received VHT Navbits");
    return compute(Ieee80211PhyFormat::VHT_SU, uncodedDataBits,
                   initialAvailableEncodedBits, numberOfCodedBitsPerSymbol,
                   codeRate, stbcSymbolFactor);
}

} // namespace physicallayer
} // namespace inet
