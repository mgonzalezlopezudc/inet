//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HTCONTROL_H
#define __INET_IEEE80211HTCONTROL_H

#include <cstdint>

namespace inet {
namespace ieee80211 {

/** Packet-level subset of the HT variant HT Control field. */
struct Ieee80211HtMcsControl
{
    bool trainingRequest = false;
    bool mcsRequest = false;
    uint8_t mcsRequestSequenceIdentifier = 0;
    uint8_t mcsFeedbackSequenceIdentifier = 7;
    uint8_t mcsFeedback = 127;
    uint8_t csiSteering = 0; // 0=None, 1=CSI, 2=noncompressed, 3=compressed
    bool ndpAnnouncement = false;
};

inline bool isHtVariantHtControl(uint32_t value)
{
    // IEEE Std 802.11-2024, 9.2.4.6.1: B0=0 selects the HT variant.
    return (value & 0x1) == 0;
}

inline bool packHtMcsControl(const Ieee80211HtMcsControl& control, uint32_t& value)
{
    if ((control.mcsRequest && control.mcsRequestSequenceIdentifier > 6) ||
            control.mcsRequestSequenceIdentifier > 7 ||
            control.mcsFeedbackSequenceIdentifier > 7 ||
            control.mcsFeedback > 127 || control.csiSteering > 3)
        return false;
    // IEEE Std 802.11-2024, 9.2.4.6.2, Figures 9-13..9-15 and Table 9-22.
    value = (static_cast<uint32_t>(control.trainingRequest) << 1) |
            (static_cast<uint32_t>(control.mcsRequest) << 2) |
            (static_cast<uint32_t>(control.mcsRequestSequenceIdentifier) << 3) |
            (static_cast<uint32_t>(control.mcsFeedbackSequenceIdentifier) << 6) |
            (static_cast<uint32_t>(control.mcsFeedback) << 9) |
            (static_cast<uint32_t>(control.csiSteering) << 22) |
            (static_cast<uint32_t>(control.ndpAnnouncement) << 24);
    return true;
}

inline bool unpackHtMcsControl(uint32_t value, Ieee80211HtMcsControl& control)
{
    if (!isHtVariantHtControl(value))
        return false;
    control.trainingRequest = ((value >> 1) & 0x1) != 0;
    control.mcsRequest = ((value >> 2) & 0x1) != 0;
    control.mcsRequestSequenceIdentifier = (value >> 3) & 0x7;
    control.mcsFeedbackSequenceIdentifier = (value >> 6) & 0x7;
    control.mcsFeedback = (value >> 9) & 0x7F;
    control.csiSteering = (value >> 22) & 0x3;
    control.ndpAnnouncement = ((value >> 24) & 0x1) != 0;
    return !control.mcsRequest || control.mcsRequestSequenceIdentifier <= 6;
}

} // namespace ieee80211
} // namespace inet

#endif
