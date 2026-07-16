//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.packetlevel.errormodel;

module Ieee80211RbirErrorModel extends Ieee80211NistErrorModel
{
    parameters:
        // Line-oriented calibration file. Accepted records are:
        // RBIR <BPSK|QPSK|QAM16|QAM64|QAM256> <snrDb> <meanRbir>
        // PER <BCC|LDPC> <32|1458> <mcs> <snrDb> <meanPer>
        // The file must provide the Appendix 3 Mean curves; it is not bundled.
        string calibrationFile;
        @class(Ieee80211RbirErrorModel);
}
