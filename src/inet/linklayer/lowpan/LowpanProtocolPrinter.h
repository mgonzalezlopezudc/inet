// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANPROTOCOLPRINTER_H
#define __INET_LOWPANPROTOCOLPRINTER_H
#include "inet/common/packet/printer/ProtocolPrinter.h"

namespace inet { namespace lowpan {
class INET_API LowpanProtocolPrinter : public ProtocolPrinter
{
  public:
    virtual void print(const Ptr<const Chunk>& chunk, const Protocol *protocol, const cMessagePrinter::Options *options, Context& context) const override;
};
} } // namespace inet::lowpan
#endif
