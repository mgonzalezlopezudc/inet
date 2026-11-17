// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANUDPNHCCODEC_H
#define __INET_LOWPANUDPNHCCODEC_H
#include "inet/linklayer/lowpan/LowpanUdpNhcHeader_m.h"
#include "inet/common/packet/chunk/BytesChunk.h"
namespace inet { namespace lowpan {
class INET_API LowpanUdpNhcCodec
{
  public:
    static int getWireLength(int portMode, bool checksumElided = false);
    static Ptr<const LowpanUdpNhcHeader> encode(const std::vector<uint8_t>& bytes, int ipv6PayloadLength);
    static Ptr<const BytesChunk> decode(const LowpanUdpNhcHeader& header, int ipv6PayloadLength);
};
} } // namespace inet::lowpan
#endif
