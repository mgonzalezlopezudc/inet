//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.packetlevel.errormodel;

import inet.physicallayer.wireless.common.base.packetlevel.ErrorModelBase;

// Packet-level BCC SISO HT EESM error model. The opt-in supported scope is
// HT MCS 0-7 at 20 or 40 MHz, using dimensional SNIR from the occupied-
// subcarrier Data spectrum and one WHOLE-packet error probability (52 or 108
// data carriers, respectively).
//
// Runtime inputs are AWGN PER CSV and manifest artifacts, together with the
// versioned beta/calibration artifact selected by betaTableFile and
// calibrationSet. This model has no analytical fallback. Reviewed artifacts
// remain the default production gate. A local-only user-supplied table is
// usable only through the explicit userAuthorizedLocal mode, whose manifest
// records its local-evaluation-only scope and unavailable provenance evidence.
// This policy is generic: every local manifest must declare
// deploymentScope=localEvaluationOnly. Repository-owned synthetic fixtures may
// use notApplicable/granted statuses, while exact user/paper artifacts retain
// unavailable/notVerified/notGranted; reviewed remains the production gate.
// Empty defaults intentionally fail explicitly.
module Ieee80211EesmErrorModel extends ErrorModelBase
{
    parameters:
        string perTableFile = default("");
        string betaTableFile = default("");
        string calibrationSet = default("");
        string artifactAcceptanceMode @enum("reviewed", "userAuthorizedLocal") = default("reviewed");
        object perTableManifest = default(nullptr);
        @class(Ieee80211EesmErrorModel);
        @display("i=block/broadcast");
}
