//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.packetlevel;

module Ieee80211LdpcSoftReceiver extends Ieee80211Receiver
{
    parameters:
        string decoderAlgorithm @enum("sumProduct", "normalizedMinSum") = default("sumProduct");
        int maxIterations = default(20);
        double normalizedMinSumFactor = default(0.75);
        double maximumLlr = default(20);
        @class(Ieee80211LdpcSoftReceiver);
        @signal[ldpcDataDecodeAttempted](type=long);
        @signal[ldpcDataDecodeSucceeded](type=long);
        @signal[ldpcDataDecodeIterations](type=long);
        @statistic[ldpcDataDecodeInvocations](title="LDPC DATA decode invocations"; source=ldpcDataDecodeAttempted; record=count);
        @statistic[ldpcDataDecodeConvergences](title="LDPC DATA decode convergences"; source=ldpcDataDecodeSucceeded; record=count);
        @statistic[ldpcDataDecodeIterations](title="LDPC DATA decode iterations"; source=ldpcDataDecodeIterations; record=sum);
}
