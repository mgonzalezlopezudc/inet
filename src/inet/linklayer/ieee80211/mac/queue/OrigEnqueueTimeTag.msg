//
// Copyright (C) 2024 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

import inet.common.INETDefs;
import inet.common.TagBase;

namespace inet::ieee80211;

//
// Tag that stores the original enqueue time of a MAC frame.
// Used to calculate head-of-line (HoL) delay while preserving the original
// timestamp even when the packet is retried and requeued.
//
class OrigEnqueueTimeTag extends TagBase
{
    simtime_t enqueueTime;
}
