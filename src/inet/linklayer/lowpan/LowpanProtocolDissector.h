// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANPROTOCOLDISSECTOR_H
#define __INET_LOWPANPROTOCOLDISSECTOR_H
#include "inet/common/packet/dissector/ProtocolDissector.h"

namespace inet { namespace lowpan {
class INET_API LowpanProtocolDissector : public ProtocolDissector
{
  public:
    virtual void dissect(Packet *packet, const Protocol *protocol, ICallback& callback) const override;
};
} } // namespace inet::lowpan
#endif
