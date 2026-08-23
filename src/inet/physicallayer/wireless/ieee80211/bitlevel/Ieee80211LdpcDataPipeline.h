//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCDATAPIPELINE_H
#define __INET_IEEE80211LDPCDATAPIPELINE_H

#include <cstdint>
#include <vector>

#include "inet/common/BitVector.h"
#include "inet/physicallayer/wireless/common/contract/bitlevel/IFecCoder.h"
#include "inet/physicallayer/wireless/ieee80211/bitlevel/Ieee80211LdpcBitPipeline.h"

namespace inet {
namespace physicallayer {

/**
 * The complete pre-FEC and post-FEC HT/VHT-SU LDPC data-field result.
 *
 * preScrambledBits contains SERVICE, PSDU, and (for VHT-SU) PHY padding.
 * scrambledBits is the input to the LDPC encoder and mapped contains the
 * constellation-bit groups after stream/segment/tone mapping.
 */
struct INET_API Ieee80211LdpcEncodedDataField
{
    BitVector preScrambledBits;
    BitVector scrambledBits;
    Ieee80211LdpcMappedData mapped;
};

/** Result of the exact receive-side HT/VHT-SU LDPC data-field pipeline. */
struct INET_API Ieee80211LdpcDecodedDataField
{
    BitVector descrambledBits;
    BitVector psduBits;
    bool converged = false;
    int iterations = 0;
};

/**
 * Exact HT/VHT-SU LDPC Data-field preparation and receive inverse.
 *
 * This component deliberately sits beside, rather than inside, the legacy
 * Ieee80211LayeredOfdmTransmitter/Receiver implementation.  It consumes the
 * immutable Ieee80211DataEncodingPlan and delegates codeword and post-FEC
 * mapping to Ieee80211LdpcBitPipeline.  STBC and VHT-MU are not represented by
 * this SU component and are rejected at its public boundary.
 *
 * The generic packet-level receiver contract has no demapped constellation
 * LLR vector.  Consequently this component exposes an explicit IEEE-specific
 * reliability API; Ieee80211LdpcSoftRadio connects it at the receiver boundary
 * without widening the generic hard-bit contract or inventing hard +/-LLRs.
 *
 * Normative references (IEEE Std 802.11-2024): 19.3.11.2-3 (HT SERVICE and
 * scrambler), 19.3.11.7 (LDPC), 21.3.4.9.2 and 21.3.10.1-2/5.4 (VHT-SU
 * SERVICE, PHY padding, and LDPC data processing), and 21.3.10.3 (VHT-SIG-B
 * CRC).
 */
class INET_API Ieee80211LdpcDataPipeline
{
  public:
    // The deterministic reference internal LFSR register state used by the
    // existing IEEE 802.11 OFDM scrambler definition (binary 1011101,
    // represented least-significant bit first by ShortBitVector/BitVector).
    // This is not the TXVECTOR SCRAMBLER_INITIAL_VALUE field, which denotes
    // transmitted scrambling-sequence bits and needs an explicit conversion.
    // Production callers may and, for independent simultaneous users, should
    // pass a distinct nonzero 7-bit register state.
    static constexpr uint8_t DEFAULT_SCRAMBLER_REGISTER_STATE = 0x5d;

  protected:
    static void validateSuPlan(const Ieee80211DataEncodingPlan& plan,
            int psduBitLength, int stbcSymbolFactor);
    static BitVector makeServiceField(Ieee80211PhyFormat phyFormat,
            uint8_t vhtSigBCrc);
    static BitVector scrambleOrDescramble(const BitVector& bits,
            uint8_t scramblerRegisterState);
    static void appendBits(BitVector& destination, const BitVector& source);

  public:
    /** Compute the complemented VHT-SIG-B CRC from its protected bits. */
    static uint8_t computeVhtSigBCrc(const std::vector<bool>& protectedBits);

    /** Build the un-scrambled SERVICE field for the selected format. */
    static BitVector buildServiceField(Ieee80211PhyFormat phyFormat,
            uint8_t vhtSigBCrc = 0);

    /**
     * Build SERVICE + PSDU + PHY padding before scrambling.  The PSDU is
     * supplied in the on-air bit order used by BitVector (LSB first per byte).
     */
    static BitVector buildPreScrambledData(const BitVector& psduBits,
            const Ieee80211DataEncodingPlan& plan, uint8_t vhtSigBCrc = 0,
            int stbcSymbolFactor = 1);

    /** Scramble one HT/VHT data-field bit vector with a deterministic register state. */
    static BitVector scramble(const BitVector& bits,
            uint8_t scramblerRegisterState = DEFAULT_SCRAMBLER_REGISTER_STATE);

    /** Additive scrambling is self-inverse, so this is the receive inverse. */
    static BitVector descramble(const BitVector& bits,
            uint8_t scramblerRegisterState = DEFAULT_SCRAMBLER_REGISTER_STATE);

    /**
     * Recover the internal seven-bit scrambler register from the first seven
     * scrambled SERVICE bits.  The on-air SERVICE sequence is not the
     * register value itself; this deliberately evaluates the scrambler
     * sequence for every nonzero seven-bit state and compares the resulting
     * sequence.  A zero return value is never a valid register state.
     */
    static uint8_t recoverScramblerRegisterState(const BitVector& scrambledService);

    /**
     * Construct the complete transmitter-side HT/VHT-SU LDPC Data field.
     * stbcSymbolFactor must be one; VHT-MU has no VHT_SU plan and is rejected
     * by validateSuPlan.
     */
    static Ieee80211LdpcEncodedDataField encodeAndMap(const BitVector& psduBits,
            const Ieee80211DataEncodingPlan& plan,
            const std::vector<int>& bitsPerSubcarrier, int bandwidthMhz,
            uint8_t vhtSigBCrc = 0, int stbcSymbolFactor = 1,
            const Ieee80211LdpcDataCoder& coder = Ieee80211LdpcDataCoder(),
            uint8_t scramblerRegisterState = DEFAULT_SCRAMBLER_REGISTER_STATE);

    /**
     * Demap, decode, descramble, and remove SERVICE/PHY padding.  For VHT-SU
     * the CRC bytes can be checked when the receiver has decoded VHT-SIG-B by
     * setting validateVhtSigBCrc to true and passing the expected value.
     * Passing scramblerRegisterState=0 recovers the transmitter's register
     * from the first seven decoded scrambled SERVICE bits.
     * Decoder, scrambler recovery, SERVICE/CRC, or PHY-padding validation
     * failure returns converged=false and no partial payload. Representation
     * and configuration contract violations still raise an error.
     */
    static Ieee80211LdpcDecodedDataField inverseMapAndDecode(
            const Ieee80211LdpcMappedReliabilities& mapped,
            const Ieee80211DataEncodingPlan& plan,
            const std::vector<int>& bitsPerSubcarrier, int bandwidthMhz,
            int psduBitLength, bool validateVhtSigBCrc = false,
            uint8_t expectedVhtSigBCrc = 0, int stbcSymbolFactor = 1,
            const Ieee80211LdpcDataCoder& coder = Ieee80211LdpcDataCoder(),
            uint8_t scramblerRegisterState = DEFAULT_SCRAMBLER_REGISTER_STATE);
};

} // namespace physicallayer
} // namespace inet

#endif
