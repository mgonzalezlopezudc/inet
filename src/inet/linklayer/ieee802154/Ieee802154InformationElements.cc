// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154InformationElements.h"
#include "inet/linklayer/ieee802154/Ieee802154Frame_m.h"
#include "inet/linklayer/ieee802154/Ieee802154FrameLayout.h"

namespace inet {

Ieee802154InformationElements::Result Ieee802154InformationElements::inspect(const Ieee802154Frame& frame)
{
    auto mode = [](const Ieee802154Address& address) { return address.isNone() ? 0 : address.isShort() ? 2 : 3; };
    auto layout = Ieee802154FrameLayout::fromControl(frame.getFrameVersion(),mode(frame.getDestinationAddress()),
            mode(frame.getSourceAddress()),frame.getPanCompression(),frame.getSequenceSuppressed());
    if (frame.getFrameVersion() != 2 || !layout)
        return Error::MALFORMED;
    View view;
    const auto base = layout->headerLength();
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < frame.getPayloadArraySize(); ++i) bytes.push_back(frame.getPayload(i));
    view.fixedMhr = {0,base};
    view.fcs = {base + bytes.size(),frame.getFcsPresent() ? size_t(2) : size_t(0)};
    size_t position = 0, clearEnd = bytes.size();
    view.auxiliary = {base,0};
    if (frame.getSecurityEnabled()) {
        if (bytes.empty()) return Error::TRUNCATED;
        auto control = bytes[0];
        view.securityLevel = control & 7;
        view.securityLevelReserved = view.securityLevel == 4;
        const size_t keyLengths[] = {0,1,5,9};
        auto auxiliaryLength = size_t(1) + ((control & 0x20) ? 0 : 4) + keyLengths[(control >> 3) & 3];
        if (bytes.size() < auxiliaryLength) return Error::TRUNCATED;
        view.auxiliary.length = auxiliaryLength;
        position = auxiliaryLength;
        view.protectedRemainder = true;
        if (view.securityLevelReserved) {
            view.headerIes = view.payloadIes = {base + position,0};
            view.data = {base + position,bytes.size() - position};
            return view;
        }
        const size_t micLengths[] = {0,4,8,16,0,4,8,16};
        view.micLength = micLengths[view.securityLevel];
        if (bytes.size() - position < view.micLength) return Error::TRUNCATED;
        clearEnd -= view.micLength;
    }
    auto headerStart = position;
    if (frame.getInformationElementsPresent()) {
        if (position == clearEnd) return Error::TRUNCATED;
        while (position < clearEnd) {
            if (clearEnd - position < 2) return Error::TRUNCATED;
            auto descriptor = uint16_t(bytes[position]) | uint16_t(bytes[position + 1]) << 8;
            if (descriptor & 0x8000) return Error::MALFORMED;
            auto length = size_t(descriptor & 0x7f);
            auto id = uint8_t((descriptor >> 7) & 0xff);
            position += 2;
            if (id == 0x7e || id == 0x7f) {
                if (length) return Error::MALFORMED;
                view.headerTermination1 = id == 0x7e;
                view.headerTermination2 = id == 0x7f;
                break;
            }
            if (length > clearEnd - position) return Error::TRUNCATED;
            view.headers.push_back({id,{bytes.begin() + position,bytes.begin() + position + length}});
            position += length;
        }
    }
    view.headerIes = {base + headerStart,position - headerStart};
    auto payloadStart = position;
    if (!view.protectedRemainder && view.headerTermination1) {
        while (position < clearEnd) {
            if (clearEnd - position < 2) return Error::TRUNCATED;
            auto descriptor = uint16_t(bytes[position]) | uint16_t(bytes[position + 1]) << 8;
            if (!(descriptor & 0x8000)) return Error::MALFORMED;
            auto length = size_t(descriptor & 0x7ff);
            auto group = uint8_t((descriptor >> 11) & 15);
            position += 2;
            if (group == 15) {
                if (length) return Error::MALFORMED;
                view.payloadTermination = true;
                break;
            }
            if (length > clearEnd - position) return Error::TRUNCATED;
            Payload payload{group,{bytes.begin() + position,bytes.begin() + position + length}};
            if (group == 1) {
                auto nested = inspectNested(payload);
                if (auto error = std::get_if<Error>(&nested)) return *error;
            }
            view.payloads.push_back(std::move(payload));
            position += length;
        }
    }
    view.payloadIes = {base + payloadStart,position - payloadStart};
    view.data = {base + position,bytes.size() - position};
    if (frame.getFrameType() == 3 && !frame.getSecurityEnabled() && !view.data.length)
        return Error::TRUNCATED; // The Command ID follows the complete IE region.
    return view;
}

std::vector<uint8_t> Ieee802154InformationElements::encode(const std::vector<Header>& headers,
        const std::vector<Payload>& payloads, const std::vector<uint8_t>& data, bool optionalPayloadTermination)
{
    std::vector<uint8_t> result;
    auto word = [&](uint16_t value) { result.push_back(value & 0xff); result.push_back(value >> 8); };
    for (const auto& header : headers) {
        if (header.id == 0x7e || header.id == 0x7f || header.content.size() > 127)
            throw cRuntimeError("Invalid IEEE 802.15.4 Header IE");
        word(uint16_t(header.id) << 7 | header.content.size());
        result.insert(result.end(),header.content.begin(),header.content.end());
    }
    if (!payloads.empty()) word(0x3f00); // Header Termination 1.
    else if (!headers.empty() && !data.empty()) word(0x3f80); // Header Termination 2.
    for (const auto& payload : payloads) {
        if (payload.group >= 15 || payload.content.size() > 2047)
            throw cRuntimeError("Invalid IEEE 802.15.4 Payload IE");
        word(0x8000 | uint16_t(payload.group) << 11 | payload.content.size());
        result.insert(result.end(),payload.content.begin(),payload.content.end());
    }
    if (!payloads.empty() && (!data.empty() || optionalPayloadTermination)) word(0xf800);
    else if (optionalPayloadTermination && payloads.empty())
        throw cRuntimeError("Payload Termination requires a Payload IE list");
    result.insert(result.end(),data.begin(),data.end());
    return result;
}

Ieee802154InformationElements::NestedResult Ieee802154InformationElements::inspectNested(const Payload& payload)
{
    if (payload.group != 1 || payload.content.size() > 2047) return Error::MALFORMED;
    std::vector<Nested> result;
    size_t position = 0;
    while (position < payload.content.size()) {
        if (payload.content.size() - position < 2) return Error::TRUNCATED;
        auto descriptor = uint16_t(payload.content[position]) | uint16_t(payload.content[position + 1]) << 8;
        bool longForm = descriptor & 0x8000;
        auto length = size_t(descriptor & (longForm ? 0x7ff : 0xff));
        auto id = uint8_t(longForm ? (descriptor >> 11) & 15 : (descriptor >> 8) & 127);
        position += 2;
        if (length > payload.content.size() - position) return Error::TRUNCATED;
        result.push_back({longForm,id,{payload.content.begin() + position,payload.content.begin() + position + length}});
        position += length;
    }
    return result;
}

std::vector<uint8_t> Ieee802154InformationElements::encodeNested(const std::vector<Nested>& elements)
{
    std::vector<uint8_t> result;
    for (const auto& element : elements) {
        if ((element.longForm && (element.id > 15 || element.content.size() > 2045)) ||
                (!element.longForm && (element.id > 127 || element.content.size() > 255)) ||
                result.size() + 2 + element.content.size() > 2047)
            throw cRuntimeError("Invalid IEEE 802.15.4 Nested IE");
        auto descriptor = element.longForm ? uint16_t(0x8000 | uint16_t(element.id) << 11 | element.content.size()) :
                uint16_t(uint16_t(element.id) << 8 | element.content.size());
        result.push_back(descriptor & 0xff); result.push_back(descriptor >> 8);
        result.insert(result.end(),element.content.begin(),element.content.end());
    }
    return result;
}

} // namespace inet
