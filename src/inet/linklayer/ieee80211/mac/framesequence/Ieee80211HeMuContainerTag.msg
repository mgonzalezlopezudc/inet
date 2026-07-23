//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

import inet.common.INETDefs;
import inet.common.TagBase;

namespace inet::ieee80211;

//
// MAC-owned metadata for the model-only packet that contains the concatenated
// per-recipient PSDUs of one HE DL MU PPDU. This is not PHY TXVECTOR content
// and is never serialized on the wire.
//
class Ieee80211HeMuContainerReq extends TagBase
{
    simtime_t durationField;
}
