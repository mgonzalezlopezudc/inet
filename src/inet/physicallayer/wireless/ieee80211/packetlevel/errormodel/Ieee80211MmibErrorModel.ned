//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.packetlevel.errormodel;

import inet.physicallayer.wireless.common.base.packetlevel.ErrorModelBase;

// MMIB uses the normalized average exact log-MAP Gray bit-channel MI.
module Ieee80211MmibErrorModel extends ErrorModelBase
{
    parameters:
        string perTableFile = default("");
        double beta = default(nan);
        string artifactAcceptanceMode @enum("reviewed", "userAuthorizedLocal") = default("reviewed");
        object perTableManifest = default(nullptr);
        @class(Ieee80211MmibErrorModel);
        @display("i=block/broadcast");
}
