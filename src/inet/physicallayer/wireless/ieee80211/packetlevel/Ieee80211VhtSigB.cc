//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigB.h"

namespace inet {
namespace physicallayer {

Ieee80211VhtSigBLayout getVhtSuSigBLayout(unsigned int bandwidthCode)
{
    // IEEE Std 802.11-2024, Table 21-14: VHT-SIG-B fields for a VHT SU PPDU.
    switch (bandwidthCode) {
        case 0: return {17, 3}; // 20 MHz: 17 Length + 3 Reserved + 6 Tail
        case 1: return {19, 2}; // 40 MHz: 19 Length + 2 Reserved + 6 Tail
        case 2:                 // 80 MHz
        case 3: return {21, 2}; // 160 MHz (the supported contiguous case)
        default:
            throw cRuntimeError("Invalid VHT-SIG-A bandwidth code %u", bandwidthCode);
    }
}

unsigned int encodeVhtSuSigBLength(B apepLength)
{
    auto bytes = apepLength.get<B>();
    if (bytes < 0)
        throw cRuntimeError("VHT APEP length must be nonnegative");
    // Equation (21-46) uses CEILING(APEP_LENGTH / 4). A received VHT-SIG-B
    // therefore reports the enclosing four-octet boundary for an unaligned
    // packet-level APEP proxy.
    return (bytes + 3) / 4;
}

B decodeVhtSuSigBLength(unsigned int encodedLength)
{
    return B(4ULL * encodedLength);
}

} // namespace physicallayer
} // namespace inet
