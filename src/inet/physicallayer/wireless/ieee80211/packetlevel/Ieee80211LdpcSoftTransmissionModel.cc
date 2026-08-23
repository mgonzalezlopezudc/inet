//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211LdpcSoftTransmissionModel.h"

namespace inet {
namespace physicallayer {

BitVector Ieee80211LdpcSoftTransmissionModel::flattenMappedBits(
        const Ieee80211LdpcMappedData& mappedData)
{
    BitVector result;
    for (const auto& symbol : mappedData.blocks)
        for (const auto& stream : symbol)
            for (const auto& block : stream)
                for (unsigned int bit = 0; bit < block.getSize(); bit++)
                    result.appendBit(block.getBit(bit));
    return result;
}

Ieee80211LdpcSoftTransmissionModel::Ieee80211LdpcSoftTransmissionModel(
        const Ieee80211LdpcMappedData& mappedData, const SymbolBlocks& symbols) :
    allBits(flattenMappedBits(mappedData)),
    mappedData(mappedData),
    symbols(symbols)
{
    if (mappedData.blocks.size() != symbols.size())
        throw cRuntimeError("IEEE 802.11 LDPC soft signal symbol count disagrees with mapped data");
    for (size_t symbol = 0; symbol < mappedData.blocks.size(); symbol++) {
        if (mappedData.blocks[symbol].size() != symbols[symbol].size())
            throw cRuntimeError("IEEE 802.11 LDPC soft signal stream count disagrees with mapped data");
        for (size_t stream = 0; stream < mappedData.blocks[symbol].size(); stream++) {
            if (mappedData.blocks[symbol][stream].size() != symbols[symbol][stream].size())
                throw cRuntimeError("IEEE 802.11 LDPC soft signal block count disagrees with mapped data");
            for (size_t block = 0; block < mappedData.blocks[symbol][stream].size(); block++) {
                const auto& bits = mappedData.blocks[symbol][stream][block];
                if (bits.getSize() == 0 || symbols[symbol][stream][block].empty() ||
                    bits.getSize() % symbols[symbol][stream][block].size() != 0)
                    throw cRuntimeError("IEEE 802.11 LDPC soft signal cannot contain empty constellation groups");
            }
        }
    }
}

std::ostream& Ieee80211LdpcSoftTransmissionModel::printToStream(std::ostream& stream,
        int level, int evFlags) const
{
    stream << "Ieee80211LdpcSoftTransmissionModel";
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(allBits.getSize()) << EV_FIELD(mappedData.blocks.size())
               << EV_FIELD(symbols.size());
    return stream;
}

} // namespace physicallayer
} // namespace inet
