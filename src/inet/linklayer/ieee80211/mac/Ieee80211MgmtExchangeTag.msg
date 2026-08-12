//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

import inet.common.INETDefs;
import inet.common.TagBase;

namespace inet::ieee80211;

// Local-only correlation metadata for management frame exchange results.
// This tag is never part of the serialized IEEE 802.11 frame body.
class Ieee80211MgmtExchangeTag extends TagBase
{
    uint64_t transactionId;
}
