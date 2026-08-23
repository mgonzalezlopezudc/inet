//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ReceivedDataEncodingPlan.h"

#include <limits>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigB.h"

namespace inet {
namespace physicallayer {

Ieee80211DataEncodingPlan reconstructIeee80211ReceivedDataEncodingPlan(
        const IIeee80211DataMode *dataMode,
        const Ptr<const Ieee80211PhyHeader>& phyHeader,
        simtime_t dataDuration)
{
    if (dataMode == nullptr || phyHeader == nullptr)
        throw cRuntimeError("Cannot reconstruct an IEEE 802.11 data encoding plan without a data mode and PHY header");

    auto dataTicks = dataDuration.raw();
    auto symbolTicks = dataMode->getSymbolInterval().raw();
    if (dataTicks <= 0 || symbolTicks <= 0 || dataTicks % symbolTicks != 0)
        throw cRuntimeError("IEEE 802.11 received data duration is not an integral number of OFDM symbols");
    auto receivedSymbols = dataTicks / symbolTicks;
    if (receivedSymbols <= 0 || receivedSymbols > std::numeric_limits<int>::max())
        throw cRuntimeError("IEEE 802.11 received OFDM symbol count is out of range");
    int receivedNumberOfSymbols = static_cast<int>(receivedSymbols);

    Ieee80211DataEncodingPlan plan = [&]() {
        if (auto htHeader = dynamicPtrCast<const Ieee80211HtPhyHeader>(phyHeader)) {
            if (dataMode->getPhyFormat() != Ieee80211PhyFormat::HT ||
                htHeader->getFecCoding() != (dataMode->getFecType() == Ieee80211FecType::LDPC))
                throw cRuntimeError("IEEE 802.11 HT received mode disagrees with HT-SIG FEC coding");
            return dataMode->computeEncodingPlan(B(htHeader->getLengthField()));
        }
        if (auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader)) {
            if (dataMode->getPhyFormat() != Ieee80211PhyFormat::VHT_SU ||
                vhtHeader->getCoding() != (dataMode->getFecType() == Ieee80211FecType::LDPC))
                throw cRuntimeError("IEEE 802.11 VHT-SU received mode disagrees with VHT-SIG-A FEC coding");
            if (dataMode->getFecType() != Ieee80211FecType::LDPC)
                return dataMode->computeEncodingPlan(B(vhtHeader->getLengthField()));

            if (receivedNumberOfSymbols <= static_cast<int>(vhtHeader->getLdpcExtraOfdmSymbol()))
                throw cRuntimeError("IEEE 802.11 VHT-SU LDPC received symbol count is too small");
            int initialNumberOfSymbols = receivedNumberOfSymbols -
                    (vhtHeader->getLdpcExtraOfdmSymbol() ? 1 : 0);
            auto reconstructed = Ieee80211LdpcPlanner::computeVhtSuFromReceivedSymbols(
                    initialNumberOfSymbols, dataMode->getNumberOfCodedBitsPerSymbol(),
                    dataMode->getNumberOfDataBitsPerSymbol(), dataMode->getCodeRate());

            // VHT-SIG-B carries ceil(APEP_LENGTH/4), while N_SYM determines
            // the complete PSDU capacity. Validate only information that is
            // present at the receiver; exact APEP length is recovered later
            // from the decoded A-MPDU delimiter.
            int completePsduOctets = (reconstructed.getUncodedDataBits() - 16) / 8;
            int roundedApepOctets = decodeVhtSuSigBLength(vhtHeader->getVhtSigBLength()).get<B>();
            if (vhtHeader->getVhtSigBLength() == 0 || roundedApepOctets > completePsduOctets ||
                vhtHeader->getShortGiNsymDisambiguation() !=
                        (vhtHeader->getShortGi() && receivedNumberOfSymbols % 10 == 9))
                throw cRuntimeError("IEEE 802.11 VHT-SU LDPC timing disagrees with received VHT-SIG fields");
            return reconstructed;
        }
        throw cRuntimeError("Receiver plan reconstruction supports only HT and VHT-SU PHY headers");
    }();

    if (plan.getPhyFormat() != dataMode->getPhyFormat() ||
        plan.getFecType() != dataMode->getFecType() ||
        plan.getNumberOfSymbols() != receivedNumberOfSymbols)
        throw cRuntimeError("IEEE 802.11 receiver-derived plan disagrees with received mode or duration");
    if (auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader);
        vhtHeader != nullptr && dataMode->getFecType() == Ieee80211FecType::LDPC &&
        vhtHeader->getLdpcExtraOfdmSymbol() != plan.getAdditionalCapacityApplied())
        throw cRuntimeError("IEEE 802.11 receiver-derived plan disagrees with VHT LDPC Extra OFDM Symbol");
    return plan;
}

} // namespace physicallayer
} // namespace inet
