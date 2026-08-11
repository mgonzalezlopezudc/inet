//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

import inet.common.INETDefs;
import inet.common.TagBase;
import inet.common.Units;

namespace inet::ieee80211;

//
// Identifies the original MAC-SAP service data unit whose bytes occupy the
// tagged region. This local metadata is assigned once at MAC-service ingress
// and is not part of the transmitted IEEE 802.11 frame representation.
//
class Ieee80211MacSapServiceTag extends TagBase
{
    uint64_t serviceDataUnitId;
    B serviceDataUnitBytes;
}
