// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanNativePcapHelper.h"
#include "inet/linklayer/lowpan/LowpanProtocol.h"
namespace inet { namespace lowpan {
Register_Class(LowpanNativePcapHelper);
PcapLinkType LowpanNativePcapHelper::protocolToLinkType(const Protocol *protocol) const
{
    return protocol == &lowpanNativeProtocol ? LINKTYPE_IEEE802_15_4 : LINKTYPE_INVALID;
}
bool LowpanNativePcapHelper::matchesLinkType(PcapLinkType linkType, const Protocol *protocol) const
{
    return protocol == &lowpanNativeProtocol && linkType == LINKTYPE_IEEE802_15_4;
}
} }
