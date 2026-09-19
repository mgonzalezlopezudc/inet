// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANNATIVEPCAPHELPER_H
#define __INET_LOWPANNATIVEPCAPHELPER_H
#include "inet/common/packet/recorder/PcapRecorder.h"
namespace inet { namespace lowpan {
class INET_API LowpanNativePcapHelper : public cObject, public PcapRecorder::IHelper
{
  public:
    virtual PcapLinkType protocolToLinkType(const Protocol *protocol) const override;
    virtual bool matchesLinkType(PcapLinkType linkType, const Protocol *protocol) const override;
    virtual Packet *tryConvertToLinkType(const Packet *, b, b, PcapLinkType, const Protocol *) const override { return nullptr; }
};
} }
#endif
