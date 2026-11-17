// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef INET_IEEE802154_ENHANCEDSUPERFRAMECODEC_H
#define INET_IEEE802154_ENHANCEDSUPERFRAMECODEC_H

#include <optional>
#include <vector>
#include "inet/linklayer/ieee802154/Ieee802154GtsValues.h"
#include "inet/linklayer/ieee802154/Ieee802154InformationElements.h"

namespace inet {

/** Stateless wire values for the Simplified Superframe and GTS Header IEs. */
class INET_API Ieee802154EnhancedSuperframeCodec
{
  public:
    struct Superframe {
        uint16_t timestamp = 0;
        uint16_t specification = 0;
        uint8_t allocated = 0;
        uint8_t firstSlot = 0;
        uint8_t lastSlot = 0;
        bool permit = false;
    };

    [[nodiscard]] static Ieee802154InformationElements::Header encodeSuperframe(const Superframe& value);
    [[nodiscard]] static std::optional<Superframe> decodeSuperframe(const Ieee802154InformationElements::Header& element);
    [[nodiscard]] static Ieee802154InformationElements::Header encodeGts(const std::vector<Ieee802154GtsDescriptor>& descriptors);
    [[nodiscard]] static std::optional<std::vector<Ieee802154GtsDescriptor>> decodeGts(const Ieee802154InformationElements::Header& element);
    /** Validate the joint advertised geometry without requiring all live grants to be advertised. */
    [[nodiscard]] static bool validSchedule(const Superframe& superframe, const std::vector<Ieee802154GtsDescriptor>& descriptors);
};

} // namespace inet
#endif
