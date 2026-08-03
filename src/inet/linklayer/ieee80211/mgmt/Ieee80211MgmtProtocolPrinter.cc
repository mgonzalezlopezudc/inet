//
// Copyright (C) 2018 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtProtocolPrinter.h"

#include "inet/common/packet/printer/PacketPrinter.h"
#include "inet/common/packet/printer/ProtocolPrinterRegistry.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"

namespace inet {
namespace ieee80211 {

Register_Protocol_Printer(&Protocol::ieee80211Mgmt, Ieee80211MgmtProtocolPrinter);

void Ieee80211MgmtProtocolPrinter::print(const Ptr<const Chunk>& chunk, const Protocol *protocol, const cMessagePrinter::Options *options, Context& context) const
{
    if (auto feedback = dynamicPtrCast<const Ieee80211VhtCompressedBeamformingFeedback>(chunk)) {
        context.typeColumn << "VHT Compressed BF";
        context.infoColumn << "token=" << static_cast<int>(feedback->getSoundingDialogTokenNumber())
                           << " Nc=1 Nr=2 20MHz Ng=4 SU unsegmented"
                           << " averageSnr=" << static_cast<int>(feedback->getAverageSnr());
    }
    else if (dynamicPtrCast<const Ieee80211VhtGroupIdManagement>(chunk)) {
        context.typeColumn << "VHT Group ID Management";
        context.infoColumn << "membership=8B userPosition=16B (state deferred)";
    }
    else
        context.infoColumn << "(IEEE 802.11 Mgmt) " << chunk;
}

} // namespace ieee80211
} // namespace inet
