//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCSOFTTRANSMISSIONMODEL_H
#define __INET_IEEE80211LDPCSOFTTRANSMISSIONMODEL_H

#include <complex>
#include <vector>

#include "inet/common/BitVector.h"
#include "inet/physicallayer/wireless/common/contract/bitlevel/ISignalBitModel.h"
#include "inet/physicallayer/wireless/ieee80211/bitlevel/Ieee80211LdpcBitPipeline.h"

namespace inet {
namespace physicallayer {

/**
 * Immutable packet-level representation of an exact scalar IEEE 802.11
 * LDPC data field.  The mapped symbols are arranged as
 * [OFDM symbol][spatial stream][frequency block][constellation point].
 * The scalar mean SNIR is applied independently to every stream; streams are
 * already separated ideal streams.  This representation intentionally does
 * not model a MIMO channel matrix, precoding, inter-stream interference, or
 * equalization.
 *
 * The object contains only received/transmitted data-field observations. In
 * particular, it deliberately carries no exact APEP/PSDU length: VHT-SIG-B
 * carries only ceil(APEP_LENGTH/4) (IEEE Std 802.11-2024, Table 21-14 and
 * Eq. 21-46), so a transmitter-side exact-length oracle would violate the
 * PHY receive boundary. The receiver reconstructs the complete PSDU length
 * from its received data duration and PHY fields.
 * This object is intentionally IEEE-specific; generic reception bit-model
 * semantics remain hard-decision/BER based.
 */
class INET_API Ieee80211LdpcSoftTransmissionModel : public ITransmissionBitModel
{
  public:
    using Symbol = std::complex<double>;
    using SymbolBlock = std::vector<Symbol>;
    using SymbolBlocks = std::vector<std::vector<std::vector<SymbolBlock>>>;

  protected:
    const BitVector allBits;
    const Ieee80211LdpcMappedData mappedData;
    const SymbolBlocks symbols;

  protected:
    static BitVector flattenMappedBits(const Ieee80211LdpcMappedData& mappedData);

  public:
    Ieee80211LdpcSoftTransmissionModel(const Ieee80211LdpcMappedData& mappedData,
            const SymbolBlocks& symbols);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual b getHeaderLength() const override { return b(0); }
    virtual b getDataLength() const override { return b(allBits.getSize()); }
    virtual bps getHeaderGrossBitrate() const override { return bps(NaN); }
    virtual bps getDataGrossBitrate() const override { return bps(NaN); }
    virtual const BitVector *getAllBits() const override { return &allBits; }

    virtual const IForwardErrorCorrection *getForwardErrorCorrection() const override { return nullptr; }
    virtual const IScrambling *getScrambling() const override { return nullptr; }
    virtual const IInterleaving *getInterleaving() const override { return nullptr; }

    const Ieee80211LdpcMappedData& getMappedData() const { return mappedData; }
    const SymbolBlocks& getSymbols() const { return symbols; }
};

} // namespace physicallayer
} // namespace inet

#endif
