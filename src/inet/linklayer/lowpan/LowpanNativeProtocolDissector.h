// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANNATIVEPROTOCOLDISSECTOR_H
#define __INET_LOWPANNATIVEPROTOCOLDISSECTOR_H
#include "inet/common/packet/dissector/ProtocolDissector.h"

namespace inet { namespace lowpan {
class INET_API LowpanNativeProtocolDissector : public ProtocolDissector
{
  public:
    virtual void dissect(Packet *packet, const Protocol *protocol, ICallback& callback) const override;
};
} } // namespace inet::lowpan
#endif
