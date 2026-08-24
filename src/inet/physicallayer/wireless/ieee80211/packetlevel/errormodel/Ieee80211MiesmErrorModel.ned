//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.packetlevel.errormodel;

import inet.physicallayer.wireless.common.base.packetlevel.ErrorModelBase;

// MIESM/RBIR symbol-constrained MI effective-SNR policy for HT BCC SISO.
// The scalar beta is a dimensionless linear calibration parameter with no
// usable default: beta=1 is an explicitly uncalibrated mapping, not an EESM beta.
module Ieee80211MiesmErrorModel extends ErrorModelBase
{
    parameters:
        string perTableFile = default("");
        double beta = default(nan);
        string artifactAcceptanceMode @enum("reviewed", "userAuthorizedLocal") = default("reviewed");
        object perTableManifest = default(nullptr);
        @class(Ieee80211MiesmErrorModel);
        @display("i=block/broadcast");
}

module Ieee80211RbirErrorModel extends ErrorModelBase
{
    parameters:
        string perTableFile = default("");
        double beta = default(nan);
        string artifactAcceptanceMode @enum("reviewed", "userAuthorizedLocal") = default("reviewed");
        object perTableManifest = default(nullptr);
        @class(Ieee80211RbirErrorModel);
        @display("i=block/broadcast");
}
