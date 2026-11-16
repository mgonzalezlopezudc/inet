// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanRegistrationInfo.h"

namespace inet { namespace lowpan {

LowpanRegistrationInfo::Recency LowpanRegistrationInfo::compare(uint8_t incoming, uint8_t incumbent)
{
    if (incoming == incumbent) return EQUAL;
    bool aLinear = incoming >= 128, bLinear = incumbent >= 128;
    if (aLinear != bLinear) {
        int linear = aLinear ? incoming : incumbent;
        int circular = aLinear ? incumbent : incoming;
        bool circularNewer = 256 + circular - linear <= 16;
        return circularNewer != aLinear ? NEWER : OLDER;
    }
    if (std::abs(int(incoming) - int(incumbent)) > 16) return INCOMPARABLE;
    return incoming > incumbent ? NEWER : OLDER;
}

LowpanRegistrationInfo::ParseResult LowpanRegistrationInfo::parse(const Ipv6NdOption& option,
        bool updated, LowpanRegistrationInfo& result)
{
    int length = option.getOptionLength();
    if (int(option.getType()) != 33 || length < 2 || length > 5 ||
            int(option.getPaddingBytesArraySize()) != length * 8 - 2)
        return MALFORMED;
    bool known = updated && (uint8_t(option.getPaddingBytes(2)) & 1);
    if ((!known && length != 2) || (known && (uint8_t(option.getPaddingBytes(2)) & 12)))
        return UNSUPPORTED;
    result = LowpanRegistrationInfo();
    result.tidKnown = known;
    if (known) {
        result.tid = uint8_t(option.getPaddingBytes(3));
        result.reachability = uint8_t(option.getPaddingBytes(2)) & 2;
        result.opaque = uint8_t(option.getPaddingBytes(1));
    }
    for (int i = 6; i < length * 8 - 2; i++) result.rovr.push_back(uint8_t(option.getPaddingBytes(i)));
    return VALID;
}

Ipv6NdOption *LowpanRegistrationInfo::makeOption(int status, int lifetimeMinutes, bool updatedReply) const
{
    if ((rovr.size() != 8 && rovr.size() != 16 && rovr.size() != 24 && rovr.size() != 32) ||
            lifetimeMinutes < 0 || lifetimeMinutes > 65535 || status < 0 || status > 255)
        throw cRuntimeError("Invalid native registration metadata");
    auto option = new Ipv6NdOption();
    option->setType(static_cast<Ipv6NdOptionTypes>(33));
    option->setOptionLength(1 + rovr.size() / 8);
    option->setPaddingBytesArraySize(6 + rovr.size());
    option->setPaddingBytes(0, status);
    option->setPaddingBytes(1, updatedReply && tidKnown ? opaque : 0);
    option->setPaddingBytes(2, updatedReply ? (reachability ? 2 : 0) | 1 : 0);
    option->setPaddingBytes(3, updatedReply && tidKnown ? tid : 0);
    option->setPaddingBytes(4, lifetimeMinutes >> 8);
    option->setPaddingBytes(5, lifetimeMinutes);
    for (size_t i = 0; i < rovr.size(); i++) option->setPaddingBytes(i + 6, rovr[i]);
    return option;
}

bool LowpanRegistrationInfo::sameTransaction(const LowpanRegistrationInfo& other) const
{
    return sameOwner(other) && tidKnown == other.tidKnown && (!tidKnown || tid == other.tid);
}

bool LowpanRegistrationInfo::sameParameters(const LowpanRegistrationInfo& other) const
{
    return sameTransaction(other) && reachability == other.reachability && opaque == other.opaque;
}
} }
