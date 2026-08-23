//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCCODE_H
#define __INET_IEEE80211LDPCCODE_H

#include "inet/physicallayer/wireless/common/radio/bitlevel/LdpcCode.h"

namespace inet {
namespace physicallayer {

enum class Ieee80211LdpcRate {
    RATE_1_2,
    RATE_2_3,
    RATE_3_4,
    RATE_5_6
};

/**
 * One of the twelve IEEE 802.11 HT/VHT LDPC codes from Annex F.
 *
 * The literals and the P_i expansion are tied to IEEE Std 802.11-2024
 * 19.3.11.7.3, 19.3.11.7.4, Table 19-15, and Annex F chunks 11834-11839.
 */
class INET_API Ieee80211LdpcCode : public LdpcCode
{
  protected:
    Ieee80211LdpcRate rate;

    Ieee80211LdpcCode(int codewordLength, Ieee80211LdpcRate rate, const std::vector<int16_t>& shifts);

  public:
    Ieee80211LdpcRate getRate() const { return rate; }

    static const Ieee80211LdpcCode& getCode(int codewordLength, Ieee80211LdpcRate rate);
    static bool isSupportedCodewordLength(int codewordLength);
    static bool isSupportedRate(Ieee80211LdpcRate rate);
    static int getExpansionFactorForLength(int codewordLength);
    static int getInformationLengthForRate(int codewordLength, Ieee80211LdpcRate rate);
};

} // namespace physicallayer
} // namespace inet

#endif
