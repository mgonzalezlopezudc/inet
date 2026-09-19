// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanProtocolPrinter.h"
#include "inet/linklayer/lowpan/LowpanHeader_m.h"
#include "inet/linklayer/lowpan/LowpanFrag1Header_m.h"
#include "inet/linklayer/lowpan/LowpanFragnHeader_m.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
#include "inet/linklayer/ieee802154/Ieee802154FrameHeader_m.h"
#include "inet/linklayer/lowpan/LowpanIphcHeader_m.h"
#include "inet/linklayer/lowpan/LowpanUdpNhcHeader_m.h"
#include "inet/common/packet/printer/ProtocolPrinterRegistry.h"

namespace inet { namespace lowpan {
Register_Protocol_Printer(&lowpanProtocol, LowpanProtocolPrinter);
Register_Protocol_Printer(&lowpanNativeProtocol, LowpanProtocolPrinter);

void LowpanProtocolPrinter::print(const Ptr<const Chunk>& chunk, const Protocol *protocol, const cMessagePrinter::Options *options, Context& context) const
{
    if (dynamicPtrCast<const LowpanHeader>(chunk))
        context.infoColumn << "LOWPAN_IPV6";
    else if (auto header = dynamicPtrCast<const LowpanFrag1Header>(chunk))
        context.infoColumn << "FRAG1 size=" << header->getDatagramSize() << " tag=" << header->getDatagramTag();
    else if (auto header = dynamicPtrCast<const LowpanFragnHeader>(chunk))
        context.infoColumn << "FRAGN size=" << header->getDatagramSize() << " tag=" << header->getDatagramTag()
                           << " offset=" << 8 * header->getDatagramOffset();
    else if (auto header = dynamicPtrCast<const LowpanIphcHeader>(chunk))
        context.infoColumn << "IPHC TF=" << int(header->getTf()) << " SAM=" << int(header->getSam())
                           << " DAM=" << int(header->getDam()) << " M=" << header->getM();
    else if (auto header = dynamicPtrCast<const LowpanUdpNhcHeader>(chunk))
        context.infoColumn << "UDP NHC " << header->getSourcePort() << " > " << header->getDestinationPort()
                           << " C=" << header->getChecksumElided();
    else if (auto header = dynamicPtrCast<const Ieee802154FrameHeader>(chunk))
        context.infoColumn << "IEEE802154 " << (header->getFrameControl() == 2 ? "ACK" : "DATA")
                           << " DSN=" << int(header->getSequenceNumber()) << " PAN=" << header->getPanId()
                           << " " << header->getSourceAddress() << " > " << header->getDestinationAddress();
    else
        context.infoColumn << "(6LoWPAN opaque) " << chunk;
}
} } // namespace inet::lowpan
