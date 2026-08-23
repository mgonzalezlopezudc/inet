//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211VHTSIGB_H
#define __INET_IEEE80211VHTSIGB_H

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"

namespace inet {
using namespace units::values;

namespace physicallayer {

struct INET_API Ieee80211VhtSigBLayout
{
    int lengthFieldWidth;
    int reservedFieldWidth;

    int getBitLength() const { return lengthFieldWidth + reservedFieldWidth + 6; }
    unsigned int getReservedValue() const { return (1U << reservedFieldWidth) - 1; }
};

/** Returns the VHT-SIG-B SU layout selected by the two-bit VHT-SIG-A bandwidth code. */
INET_API Ieee80211VhtSigBLayout getVhtSuSigBLayout(unsigned int bandwidthCode);

/** Implements IEEE Std 802.11-2024 Equation (21-46), ceil(APEP_LENGTH / 4). */
INET_API unsigned int encodeVhtSuSigBLength(B apepLength);

/** Converts the received four-octet VHT-SIG-B Length units to RXVECTOR APEP_LENGTH. */
INET_API B decodeVhtSuSigBLength(unsigned int encodedLength);

} // namespace physicallayer
} // namespace inet

#endif
