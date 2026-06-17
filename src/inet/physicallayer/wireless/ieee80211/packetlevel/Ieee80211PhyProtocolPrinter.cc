//
// Copyright (C) 2018 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyProtocolPrinter.h"

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

void Ieee80211PhyProtocolPrinter::print(const Ptr<const Chunk>& chunk, const Protocol *protocol, const cMessagePrinter::Options *options, Context& context) const
{
    if (auto heMuHeader = dynamicPtrCast<const Ieee80211HeMuPhyHeader>(chunk)) {
        constexpr size_t maxPrintedUsers = 6;
        auto usersCount = heMuHeader->getUsersArraySize();
        std::ostringstream stream;
        stream << "HE MU PHY, " << usersCount << " users";
        if (usersCount > 0)
            stream << ": ";
        size_t shownUsers = std::min(maxPrintedUsers, usersCount);
        for (size_t i = 0; i < shownUsers; i++) {
            if (i != 0)
                stream << "; ";
            const auto& user = heMuHeader->getUsers(i);
            stream << "RU" << user.ruIndex << "->STA" << user.staId
                   << " (MCS" << static_cast<int>(user.mcs)
                   << ", NSS" << static_cast<int>(user.numberOfSpatialStreams)
                   << ", DCM" << (user.dcm ? 1 : 0) << ")";
        }
        if (usersCount > maxPrintedUsers)
            stream << "; +" << (usersCount - maxPrintedUsers) << " more";
        context.infoColumn << stream.str();
    }
    else {
        context.infoColumn << "(IEEE 802.11 Phy) " << chunk;
    }
}

} // namespace physicallayer
} // namespace inet
