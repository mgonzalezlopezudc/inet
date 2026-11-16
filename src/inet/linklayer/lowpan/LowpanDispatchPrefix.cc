// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanDispatchPrefix.h"
#include "inet/common/packet/chunk/BytesChunk.h"
namespace inet { namespace lowpan {
LowpanDispatchPrefix::Result LowpanDispatchPrefix::parse(const Packet& packet, int offset, State state)
{
    Result result{MALFORMED, 0, state};
    if (offset < 0 || packet.getDataLength() <= b(0) || packet.getDataLength().get() % 8 != 0) return result;
    auto bytes = packet.peekDataAsBytes()->getBytes();
    if (size_t(offset) >= bytes.size()) return result;
    size_t cursor = offset;
    // RFC 8025 sections 2/3: paging is recognized on every page, even unsupported pages.
    while (cursor < bytes.size() && (bytes[cursor] & 0xf0) == 0xf0) {
        result.state.page = bytes[cursor++] & 15;
        result.state.seenPageOne |= result.state.page == 1;
        result.pagingLength++;
    }
    if (cursor == bytes.size()) return result;
    uint8_t dispatch = bytes[cursor];
    if (result.state.page > 1) { result.kind = UNSUPPORTED_PAGE; return result; }
    if ((dispatch & 0xe0) == 0x60) { result.kind = IPHC; return result; }
    if (result.state.page != 0) { result.kind = UNKNOWN; return result; }
    if ((dispatch & 0xc0) == 0x80) result.kind = MESH;
    else if ((dispatch & 0xf8) == 0xc0) result.kind = FRAG1;
    else if ((dispatch & 0xf8) == 0xe0) result.kind = FRAGN;
    else if (dispatch == 0x50) result.kind = BROADCAST;
    else if (dispatch == 0x41) result.kind = IPV6;
    else if (dispatch == 0x40) result.kind = cursor + 1 < bytes.size() ? ESC : MALFORMED;
    else result.kind = UNKNOWN;
    // RFC 8025 section 3: neither switching back nor BC0 erases the first-page-one boundary.
    if (result.state.seenPageOne && (result.kind == MESH || result.kind == FRAG1 || result.kind == FRAGN))
        result.kind = MALFORMED;
    return result;
}
} }
