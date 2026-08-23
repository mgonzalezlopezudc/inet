//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigA.h"

namespace inet {
namespace physicallayer {

int computeVhtPartialAidForBssid(const MacAddress& bssid)
{
    // IEEE Std 802.11-2024, Clause 10.19/Table 10-13: PARTIAL_AID for
    // VHT-SU frames sent by a non-AP VHT STA to an AP is BSSID[39:47].
    // Bits 39:47 are byte 4 bit 7 followed by byte 5 bits 0:7. The
    // first listed bit is the least-significant bit of the integer.
    return (bssid.getAddressByte(4) >> 7) | (bssid.getAddressByte(5) << 1);
}

int computeVhtPartialAidForAssociatedSta(int associationId, const MacAddress& bssid)
{
    // IEEE Std 802.11-2024, Clause 10.19, Table 10-13 and Equation (10-13):
    // PARTIAL_AID for AP-to-associated-STA VHT-SU (STA_Partial_AID_VHT).
    if (associationId < 1 || associationId > 2007)
        throw cRuntimeError("Invalid IEEE 802.11 association ID %d for VHT partial AID", associationId);
    int bssidXor = (bssid.getAddressByte(5) & 0x0F) ^ (bssid.getAddressByte(5) >> 4);
    return ((associationId & 0x1FF) + (bssidXor << 5)) & 0x1FF;
}

void validateVhtSuGroupIdAndPartialAid(unsigned int groupId, unsigned int partialAid)
{
    // IEEE Std 802.11-2024, Clause 10.19/Table 10-13: VHT-SU uses
    // GROUP_ID 0 or 63; the matching VHT-SIG-A PARTIAL_AID field is 9 bits.
    if ((groupId != 0 && groupId != 63) || partialAid > 511)
        throw cRuntimeError("VHT-SU requires GROUP_ID 0 or 63 and a 9-bit PARTIAL_AID, got %u/%u",
                groupId, partialAid);
}

} // namespace physicallayer
} // namespace inet
