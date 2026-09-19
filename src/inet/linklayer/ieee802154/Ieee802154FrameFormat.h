// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_IEEE802154FRAMEFORMAT_H
#define __INET_IEEE802154FRAMEFORMAT_H
#include "inet/common/INETDefs.h"
namespace inet {
// Fixed, unsecured 2006 data and immediate ACK subset used by the native link.
class INET_API Ieee802154FrameFormat
{
  public:
    static constexpr int MAX_FRAME_BYTES = 127;
    static constexpr int FCS_BYTES = 2;
    static constexpr int EXTENDED_HEADER_BYTES = 21;
    static constexpr int BROADCAST_HEADER_BYTES = 15;
    static constexpr int ACK_HEADER_BYTES = 3;
    static int getHeaderLength(uint16_t control) {
        switch (control) {
            case 0x0002: return ACK_HEADER_BYTES;
            case 0xd841: return BROADCAST_HEADER_BYTES;
            case 0xdc41: case 0xdc61: return EXTENDED_HEADER_BYTES;
            default: return -1;
        }
    }
    static int getPayloadLimit(bool broadcast) {
        return MAX_FRAME_BYTES - FCS_BYTES - (broadcast ? BROADCAST_HEADER_BYTES : EXTENDED_HEADER_BYTES);
    }
};
}
#endif
