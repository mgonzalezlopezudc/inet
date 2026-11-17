// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef INET_IEE_E802154_FRAMELAYOUT_H
#define INET_IEE_E802154_FRAMELAYOUT_H

#include <optional>
#include "inet/common/INETDefs.h"

namespace inet {

/** Wire-field presence from IEEE Std 802.15.4-2024, Table 7-2. */
struct INET_API Ieee802154FrameLayout
{
    bool sequencePresent;
    bool destinationPanPresent;
    bool sourcePanPresent;
    unsigned int destinationAddressLength;
    unsigned int sourceAddressLength;

    static std::optional<Ieee802154FrameLayout> fromControl(int version, int destinationMode, int sourceMode,
            bool compressed, bool sequenceSuppressed)
    {
        auto length = [](int mode) -> unsigned int { return mode == 2 ? 2 : mode == 3 ? 8 : 0; };
        if (version < 0 || version > 2 || destinationMode < 0 || destinationMode > 3 || sourceMode < 0 || sourceMode > 3 ||
                destinationMode == 1 || sourceMode == 1 || (version < 2 && sequenceSuppressed))
            return std::nullopt;
        bool destination = destinationMode != 0, source = sourceMode != 0;
        if (version < 2) {
            if (compressed && (!destination || !source))
                return std::nullopt;
            return Ieee802154FrameLayout{true,destination,source && !compressed,length(destinationMode),length(sourceMode)};
        }
        bool destinationPan, sourcePan;
        if (!destination && !source) { destinationPan = compressed; sourcePan = false; }
        else if ((destination && !source) || (destinationMode == 3 && sourceMode == 3)) { destinationPan = !compressed; sourcePan = false; }
        else if (!destination && source) { destinationPan = false; sourcePan = !compressed; }
        else { destinationPan = true; sourcePan = !compressed; }
        return Ieee802154FrameLayout{!sequenceSuppressed,destinationPan,sourcePan,length(destinationMode),length(sourceMode)};
    }

    [[nodiscard]] size_t headerLength() const {
        return 2 + (sequencePresent ? 1 : 0) + (destinationPanPresent ? 2 : 0) + destinationAddressLength +
                (sourcePanPresent ? 2 : 0) + sourceAddressLength;
    }
};

} // namespace inet
#endif
