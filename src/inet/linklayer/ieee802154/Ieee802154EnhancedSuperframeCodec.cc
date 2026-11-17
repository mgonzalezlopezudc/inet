// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154EnhancedSuperframeCodec.h"

namespace inet {

Ieee802154InformationElements::Header Ieee802154EnhancedSuperframeCodec::encodeSuperframe(const Superframe& value)
{
    if (value.allocated > 7 || value.firstSlot > 15 || value.lastSlot > 15)
        throw cRuntimeError("Invalid IEEE 802.15.4 simplified superframe fields");
    uint16_t cfp = value.allocated | (value.firstSlot << 4) | (value.lastSlot << 8) | (value.permit ? 0x1000 : 0);
    return {0x23,{uint8_t(value.timestamp),uint8_t(value.timestamp >> 8),uint8_t(value.specification),uint8_t(value.specification >> 8),uint8_t(cfp),uint8_t(cfp >> 8)}};
}

std::optional<Ieee802154EnhancedSuperframeCodec::Superframe> Ieee802154EnhancedSuperframeCodec::decodeSuperframe(const Ieee802154InformationElements::Header& element)
{
    if (element.id != 0x23 || element.content.size() != 6)
        return {};
    const auto& b = element.content;
    auto cfp = uint16_t(b[4] | (uint16_t(b[5]) << 8));
    if ((cfp & 15) > 7)
        return {};
    return Superframe{uint16_t(b[0] | (uint16_t(b[1]) << 8)),uint16_t(b[2] | (uint16_t(b[3]) << 8)),
            uint8_t(cfp & 15),uint8_t((cfp >> 4) & 15),uint8_t((cfp >> 8) & 15),bool(cfp & 0x1000)};
}

Ieee802154InformationElements::Header Ieee802154EnhancedSuperframeCodec::encodeGts(const std::vector<Ieee802154GtsDescriptor>& descriptors)
{
    if (descriptors.size() > 7)
        throw cRuntimeError("Too many IEEE 802.15.4 simplified GTS descriptors");
    std::vector<uint8_t> bytes(1,0);
    for (size_t i = 0; i < descriptors.size(); ++i) {
        const auto& descriptor = descriptors[i];
        if (descriptor.start > 15 || descriptor.length > 15 || uint8_t(descriptor.direction) > 1)
            throw cRuntimeError("Invalid IEEE 802.15.4 simplified GTS descriptor");
        bytes[0] |= uint8_t(descriptor.direction) << i;
        bytes.push_back(uint8_t(descriptor.shortAddress)); bytes.push_back(uint8_t(descriptor.shortAddress >> 8));
        bytes.push_back(descriptor.start | (descriptor.length << 4));
    }
    return {0x24,std::move(bytes)};
}

std::optional<std::vector<Ieee802154GtsDescriptor>> Ieee802154EnhancedSuperframeCodec::decodeGts(const Ieee802154InformationElements::Header& element)
{
    if (element.id != 0x24 || element.content.empty() || (element.content.size() - 1) % 3 || element.content.size() > 22)
        return {};
    const auto& b = element.content;
    std::vector<Ieee802154GtsDescriptor> descriptors;
    for (size_t offset = 1, i = 0; offset < b.size(); offset += 3, ++i)
        descriptors.push_back({uint16_t(b[offset] | (uint16_t(b[offset + 1]) << 8)),uint8_t(b[offset + 2] & 15),
                uint8_t(b[offset + 2] >> 4),Ieee802154GtsDirection((b[0] >> i) & 1)});
    return descriptors;
}

bool Ieee802154EnhancedSuperframeCodec::validSchedule(const Superframe& superframe, const std::vector<Ieee802154GtsDescriptor>& descriptors)
{
    auto finalCap = uint8_t((superframe.specification >> 8) & 15);
    auto so = uint8_t((superframe.specification >> 4) & 15);
    if (superframe.allocated > 7 || descriptors.size() > 7)
        return false;
    if (superframe.allocated) {
        if (so == 15 || finalCap == 15 || superframe.firstSlot != finalCap + 1 || superframe.lastSlot != 15 ||
                superframe.allocated > superframe.lastSlot - superframe.firstSlot + 1)
            return false;
    }
    else if (finalCap != 15 || superframe.firstSlot != 0 || superframe.lastSlot != 0)
        return false;
    bool occupied[16] = {};
    size_t positives = 0;
    for (size_t i = 0; i < descriptors.size(); ++i) {
        const auto& descriptor = descriptors[i];
        if (descriptor.start > 15 || descriptor.length > 15 || uint8_t(descriptor.direction) > 1)
            return false;
        for (size_t j = 0; j < i; ++j)
            if (descriptors[j].shortAddress == descriptor.shortAddress && descriptors[j].direction == descriptor.direction)
                return false;
        if (!descriptor.start)
            continue; // Retained denial/removal does not occupy a live slot.
        if (!superframe.allocated || so == 15 || !descriptor.length || descriptor.start < superframe.firstSlot ||
                descriptor.start + descriptor.length > superframe.lastSlot + 1 || ++positives > superframe.allocated)
            return false;
        for (unsigned int slot = descriptor.start; slot < descriptor.start + descriptor.length; ++slot) {
            if (occupied[slot]) return false;
            occupied[slot] = true;
        }
    }
    return true;
}

} // namespace inet
