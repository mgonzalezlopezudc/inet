//
// Copyright (C) 2018 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyProtocolPrinter.h"

#include <algorithm>
#include <sstream>

#include "inet/common/packet/printer/PacketPrinter.h"
#include "inet/common/packet/printer/ProtocolPrinterRegistry.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"

namespace inet {
namespace physicallayer {

Register_Protocol_Printer(&Protocol::ieee80211FhssPhy, Ieee80211PhyProtocolPrinter);
Register_Protocol_Printer(&Protocol::ieee80211IrPhy, Ieee80211PhyProtocolPrinter);
Register_Protocol_Printer(&Protocol::ieee80211DsssPhy, Ieee80211PhyProtocolPrinter);
Register_Protocol_Printer(&Protocol::ieee80211HrDsssPhy, Ieee80211PhyProtocolPrinter);
Register_Protocol_Printer(&Protocol::ieee80211OfdmPhy, Ieee80211PhyProtocolPrinter);
Register_Protocol_Printer(&Protocol::ieee80211ErpOfdmPhy, Ieee80211PhyProtocolPrinter);
Register_Protocol_Printer(&Protocol::ieee80211HtPhy, Ieee80211PhyProtocolPrinter);
Register_Protocol_Printer(&Protocol::ieee80211VhtPhy, Ieee80211PhyProtocolPrinter);
Register_Protocol_Printer(&Protocol::ieee80211HePhy, Ieee80211PhyProtocolPrinter);

namespace {

const char *codingName(uint8_t coding)
{
    switch (coding) {
        case 0: return "BCC";
        case 1: return "LDPC";
        default: return "unknown";
    }
}

const char *hePpduFormatName(uint8_t format)
{
    switch (format) {
        case 0: return "HE MU";
        case 1: return "HE TB";
        case 2: return "HE SU";
        case 3: return "HE ER SU";
        default: return "HE";
    }
}

const char *ehtPpduFormatName(uint8_t format)
{
    switch (format) {
        case 0: return "EHT MU";
        case 1: return "EHT TB";
        default: return "EHT";
    }
}

template<typename Header>
void printUserSummary(std::ostream& stream, const Header& header)
{
    constexpr size_t maxPrintedUsers = 6;
    auto usersCount = header.getUsersArraySize();
    stream << usersCount << " users";
    if (usersCount > 0)
        stream << ": ";
    size_t shownUsers = std::min(maxPrintedUsers, usersCount);
    for (size_t i = 0; i < shownUsers; i++) {
        if (i != 0)
            stream << "; ";
        const auto& user = header.getUsers(i);
        stream << "STA" << user.staId
               << " (MCS" << static_cast<int>(user.mcs)
               << ", NSS" << static_cast<int>(user.numberOfSpatialStreams)
               << ", " << user.psduLength << ")";
    }
    if (usersCount > maxPrintedUsers)
        stream << "; +" << (usersCount - maxPrintedUsers) << " more";
}

void printCommonPhyInfo(std::ostream& stream, const Ieee80211PhyHeader& header)
{
    stream << "length=" << header.getLengthField();
}

} // namespace

void Ieee80211PhyProtocolPrinter::print(const Ptr<const Chunk>& chunk, const Protocol *protocol, const cMessagePrinter::Options *options, Context& context) const
{
    if (dynamicPtrCast<const Ieee80211FhssPhyPreamble>(chunk) != nullptr)
        context.typeColumn << "FHSS Preamble";
    else if (dynamicPtrCast<const Ieee80211IrPhyPreamble>(chunk) != nullptr)
        context.typeColumn << "IR Preamble";
    else if (dynamicPtrCast<const Ieee80211HrDsssPhyPreamble>(chunk) != nullptr)
        context.typeColumn << "HR/DSSS Preamble";
    else if (dynamicPtrCast<const Ieee80211DsssPhyPreamble>(chunk) != nullptr)
        context.typeColumn << "DSSS Preamble";
    else if (dynamicPtrCast<const Ieee80211ErpOfdmPhyPreamble>(chunk) != nullptr)
        context.typeColumn << "ERP-OFDM Preamble";
    else if (dynamicPtrCast<const Ieee80211OfdmPhyPreamble>(chunk) != nullptr)
        context.typeColumn << "OFDM Preamble";
    else if (dynamicPtrCast<const Ieee80211HtPhyPreamble>(chunk) != nullptr)
        context.typeColumn << "HT Preamble";
    else if (dynamicPtrCast<const Ieee80211VhtPhyPreamble>(chunk) != nullptr)
        context.typeColumn << "VHT Preamble";
    else if (dynamicPtrCast<const Ieee80211EhtPhyPreamble>(chunk) != nullptr)
        context.typeColumn << "EHT Preamble";
    else if (auto fhssHeader = dynamicPtrCast<const Ieee80211FhssPhyHeader>(chunk)) {
        context.typeColumn << "FHSS PHY";
        std::ostringstream stream;
        printCommonPhyInfo(stream, *fhssHeader);
        stream << ", PLW=" << fhssHeader->getPlw() << ", PSF=" << static_cast<int>(fhssHeader->getPsf());
        context.infoColumn << stream.str();
    }
    else if (auto irHeader = dynamicPtrCast<const Ieee80211IrPhyHeader>(chunk)) {
        context.typeColumn << "IR PHY";
        printCommonPhyInfo(context.infoColumn, *irHeader);
    }
    else if (auto hrDsssHeader = dynamicPtrCast<const Ieee80211HrDsssPhyHeader>(chunk)) {
        context.typeColumn << "HR/DSSS PHY";
        std::ostringstream stream;
        printCommonPhyInfo(stream, *hrDsssHeader);
        stream << ", signal=" << static_cast<int>(hrDsssHeader->getSignal()) << ", service=" << static_cast<int>(hrDsssHeader->getService());
        context.infoColumn << stream.str();
    }
    else if (auto dsssHeader = dynamicPtrCast<const Ieee80211DsssPhyHeader>(chunk)) {
        context.typeColumn << "DSSS PHY";
        std::ostringstream stream;
        printCommonPhyInfo(stream, *dsssHeader);
        stream << ", signal=" << static_cast<int>(dsssHeader->getSignal()) << ", service=" << static_cast<int>(dsssHeader->getService());
        context.infoColumn << stream.str();
    }
    else if (auto erpOfdmHeader = dynamicPtrCast<const Ieee80211ErpOfdmPhyHeader>(chunk)) {
        context.typeColumn << "ERP-OFDM PHY";
        std::ostringstream stream;
        printCommonPhyInfo(stream, *erpOfdmHeader);
        stream << ", rate=" << static_cast<int>(erpOfdmHeader->getRate()) << ", service=" << erpOfdmHeader->getService();
        context.infoColumn << stream.str();
    }
    else if (auto ofdmHeader = dynamicPtrCast<const Ieee80211OfdmPhyHeader>(chunk)) {
        context.typeColumn << "OFDM PHY";
        std::ostringstream stream;
        printCommonPhyInfo(stream, *ofdmHeader);
        stream << ", rate=" << static_cast<int>(ofdmHeader->getRate()) << ", service=" << ofdmHeader->getService();
        context.infoColumn << stream.str();
    }
    else if (auto htHeader = dynamicPtrCast<const Ieee80211HtPhyHeader>(chunk)) {
        context.typeColumn << "HT PHY";
        std::ostringstream stream;
        printCommonPhyInfo(stream, *htHeader);
        stream << ", coding=" << codingName(htHeader->getCoding());
        context.infoColumn << stream.str();
    }
    else if (auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(chunk)) {
        context.typeColumn << "VHT PHY";
        std::ostringstream stream;
        printCommonPhyInfo(stream, *vhtHeader);
        stream << ", coding=" << codingName(vhtHeader->getCoding());
        context.infoColumn << stream.str();
    }
    else if (auto heMuHeader = dynamicPtrCast<const Ieee80211HeMuPhyHeader>(chunk)) {
        context.typeColumn << hePpduFormatName(heMuHeader->getPpduFormat()) << " PHY";
        std::ostringstream stream;
        printCommonPhyInfo(stream, *heMuHeader);
        stream << ", BSS color=" << static_cast<int>(heMuHeader->getBssColor())
               << ", GI=" << static_cast<int>(heMuHeader->getGuardInterval())
               << ", coding=" << codingName(heMuHeader->getCoding())
               << ", ";
        printUserSummary(stream, *heMuHeader);
        context.infoColumn << stream.str();
    }
    else if (auto hePayloadHeader = dynamicPtrCast<const Ieee80211HeMuRuPayloadHeader>(chunk)) {
        context.typeColumn << "HE RU payload";
        context.infoColumn << "RU" << hePayloadHeader->getRuIndex()
                           << "->STA" << hePayloadHeader->getStaId()
                           << " length=" << hePayloadHeader->getMpduLength()
                           << " MCS" << static_cast<int>(hePayloadHeader->getMcs())
                           << " NSS" << static_cast<int>(hePayloadHeader->getNumberOfSpatialStreams())
                           << " DCM" << (hePayloadHeader->getDcm() ? 1 : 0);
    }
    else if (auto ehtHeader = dynamicPtrCast<const Ieee80211EhtPhyHeader>(chunk)) {
        context.typeColumn << ehtPpduFormatName(ehtHeader->getPpduFormat()) << " PHY";
        std::ostringstream stream;
        printCommonPhyInfo(stream, *ehtHeader);
        stream << ", BSS color=" << static_cast<int>(ehtHeader->getBssColor())
               << ", GI=" << static_cast<int>(ehtHeader->getGuardInterval())
               << ", coding=" << codingName(ehtHeader->getCoding())
               << ", ";
        printUserSummary(stream, *ehtHeader);
        context.infoColumn << stream.str();
    }
    else if (auto ehtPayloadHeader = dynamicPtrCast<const Ieee80211EhtRuPayloadHeader>(chunk)) {
        context.typeColumn << "EHT RU payload";
        context.infoColumn << "MRU" << ehtPayloadHeader->getMruIndex()
                           << "->STA" << ehtPayloadHeader->getStaId()
                           << " length=" << ehtPayloadHeader->getMpduLength()
                           << " MCS" << static_cast<int>(ehtPayloadHeader->getMcs())
                           << " NSS" << static_cast<int>(ehtPayloadHeader->getNumberOfSpatialStreams());
    }
    else if (auto phyHeader = dynamicPtrCast<const Ieee80211PhyHeader>(chunk)) {
        context.typeColumn << "802.11 PHY";
        printCommonPhyInfo(context.infoColumn, *phyHeader);
    }
    else {
        context.infoColumn << "(IEEE 802.11 Phy) " << chunk;
    }
}

} // namespace physicallayer
} // namespace inet
