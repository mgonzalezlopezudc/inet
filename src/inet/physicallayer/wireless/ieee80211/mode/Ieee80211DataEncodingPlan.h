//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211DATAENCODINGPLAN_H
#define __INET_IEEE80211DATAENCODINGPLAN_H

#include <ostream>
#include <vector>

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

enum class Ieee80211FecType {
    BCC,
    LDPC
};

enum class Ieee80211PhyFormat {
    LEGACY,
    HT,
    VHT_SU
};

class INET_API Ieee80211CodeRate
{
  protected:
    int numerator;
    int denominator;

  public:
    Ieee80211CodeRate(int numerator, int denominator);

    int getNumerator() const { return numerator; }
    int getDenominator() const { return denominator; }
    int multiplyExact(int value) const;

    bool operator==(const Ieee80211CodeRate& other) const;
    bool operator!=(const Ieee80211CodeRate& other) const { return !(*this == other); }
};

class INET_API Ieee80211LdpcCodewordPlan
{
  protected:
    int codewordLength;
    int informationLength;
    int shortenedBits;
    int puncturedBits;
    int repeatedBits;

  public:
    Ieee80211LdpcCodewordPlan(int codewordLength, int informationLength, int shortenedBits, int puncturedBits, int repeatedBits);

    int getCodewordLength() const { return codewordLength; }
    int getInformationLength() const { return informationLength; }
    int getShortenedBits() const { return shortenedBits; }
    int getPuncturedBits() const { return puncturedBits; }
    int getRepeatedBits() const { return repeatedBits; }
    int getDataBits() const { return informationLength - shortenedBits; }
    int getTransmittedBits() const { return codewordLength - shortenedBits - puncturedBits + repeatedBits; }

    bool operator==(const Ieee80211LdpcCodewordPlan& other) const;

    friend INET_API std::ostream& operator<<(std::ostream& stream, const Ieee80211LdpcCodewordPlan& plan);
};

class INET_API Ieee80211DataEncodingPlan
{
  protected:
    Ieee80211FecType fecType;
    Ieee80211PhyFormat phyFormat;
    int uncodedDataBits;
    int availableEncodedBits;
    int initialNumberOfSymbols;
    int numberOfSymbols;
    int numberOfCodedBitsPerSymbol;
    bool additionalCapacityApplied;
    std::vector<Ieee80211LdpcCodewordPlan> codewords;

  public:
    Ieee80211DataEncodingPlan(Ieee80211FecType fecType, Ieee80211PhyFormat phyFormat,
            int uncodedDataBits, int availableEncodedBits, int initialNumberOfSymbols,
            int numberOfSymbols, int numberOfCodedBitsPerSymbol, bool additionalCapacityApplied,
            const std::vector<Ieee80211LdpcCodewordPlan>& codewords = {});

    Ieee80211FecType getFecType() const { return fecType; }
    Ieee80211PhyFormat getPhyFormat() const { return phyFormat; }
    int getUncodedDataBits() const { return uncodedDataBits; }
    int getAvailableEncodedBits() const { return availableEncodedBits; }
    int getInitialNumberOfSymbols() const { return initialNumberOfSymbols; }
    int getNumberOfSymbols() const { return numberOfSymbols; }
    int getNumberOfCodedBitsPerSymbol() const { return numberOfCodedBitsPerSymbol; }
    bool getAdditionalCapacityApplied() const { return additionalCapacityApplied; }
    const std::vector<Ieee80211LdpcCodewordPlan>& getCodewords() const { return codewords; }

    int getNumberOfCodewords() const { return codewords.size(); }
    int getShortenedBits() const;
    int getPuncturedBits() const;
    int getRepeatedBits() const;

    bool operator==(const Ieee80211DataEncodingPlan& other) const;

    friend INET_API std::ostream& operator<<(std::ostream& stream, const Ieee80211DataEncodingPlan& plan);
};

struct INET_API Ieee80211LdpcCodeSelection {
    int numberOfCodewords;
    int codewordLength;
};

/**
 * Pure IEEE 802.11 HT/VHT-SU LDPC PPDU planner.
 *
 * It implements IEEE Std 802.11-2024 19.3.11.7.5 and Table 19-16 using
 * integer arithmetic. VHT-SU supplies its format-specific initial values as
 * specified by 21.3.10.5.4 and then uses the same Clause 19 procedure.
 */
class INET_API Ieee80211LdpcPlanner
{
  protected:
    static Ieee80211DataEncodingPlan compute(Ieee80211PhyFormat phyFormat,
            int uncodedDataBits, int initialAvailableEncodedBits,
            int numberOfCodedBitsPerSymbol, const Ieee80211CodeRate& codeRate,
            int stbcSymbolFactor);

  public:
    static Ieee80211DataEncodingPlan computeHt(int psduOctets,
            int numberOfCodedBitsPerSymbol, const Ieee80211CodeRate& codeRate,
            int stbcSymbolFactor = 1);
    static Ieee80211DataEncodingPlan computeVhtSu(int apepOctets,
            int numberOfCodedBitsPerSymbol, int numberOfDataBitsPerSymbol,
            const Ieee80211CodeRate& codeRate, int stbcSymbolFactor = 1);

    /**
     * Reconstruct a VHT-SU LDPC plan from received N_SYM,init and N_CBPS.
     *
     * The receiver obtains N_SYM,init from the received data duration and
     * LDPC-Extra-OFDM-Symbol indication (IEEE Std 802.11-2024,
     * 21.3.20, Eqs. 21-104..108).  It must not use the transmitter's exact
     * APEP length because VHT-SIG-B carries only ceil(APEP_LENGTH/4)
     * (Table 21-14 and Eq. 21-46).
     */
    static Ieee80211DataEncodingPlan computeVhtSuFromReceivedSymbols(
            int initialNumberOfSymbols, int numberOfCodedBitsPerSymbol,
            int numberOfDataBitsPerSymbol, const Ieee80211CodeRate& codeRate,
            int stbcSymbolFactor = 1);

    static Ieee80211LdpcCodeSelection selectCodewords(int uncodedDataBits,
            int availableEncodedBits, const Ieee80211CodeRate& codeRate);
};

} // namespace physicallayer
} // namespace inet

#endif
