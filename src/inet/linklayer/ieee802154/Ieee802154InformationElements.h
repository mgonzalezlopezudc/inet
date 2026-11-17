// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef INET_IEE_E802154_INFORMATIONELEMENTS_H
#define INET_IEE_E802154_INFORMATIONELEMENTS_H

#include <variant>
#include <vector>
#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee802154/Ieee802154Frame_m.h"

namespace inet {

/** Stateless views over the one stored frame payload; offsets include the fixed MHR prefix. */
class INET_API Ieee802154InformationElements
{
  public:
    struct Region { size_t offset = 0; size_t length = 0; };
    struct Header { uint8_t id; std::vector<uint8_t> content; };
    struct Payload { uint8_t group; std::vector<uint8_t> content; };
    struct Nested { bool longForm; uint8_t id; std::vector<uint8_t> content; };
    struct View {
        Region fixedMhr, auxiliary, headerIes, payloadIes, data, fcs;
        std::vector<Header> headers;
        std::vector<Payload> payloads;
        bool headerTermination1 = false;
        bool headerTermination2 = false;
        bool payloadTermination = false;
        bool protectedRemainder = false;
        bool securityLevelReserved = false;
        uint8_t securityLevel = 0;
        size_t micLength = 0;
    };
    enum class Error { TRUNCATED = 1, MALFORMED = 2 };
    using Result = std::variant<View,Error>;
    using NestedResult = std::variant<std::vector<Nested>,Error>;

    static Result inspect(const Ieee802154Frame& frame);
    static std::vector<uint8_t> encode(const std::vector<Header>& headers,
            const std::vector<Payload>& payloads, const std::vector<uint8_t>& data,
            bool optionalPayloadTermination = false);
    static NestedResult inspectNested(const Payload& payload);
    static std::vector<uint8_t> encodeNested(const std::vector<Nested>& elements);
};

} // namespace inet
#endif
