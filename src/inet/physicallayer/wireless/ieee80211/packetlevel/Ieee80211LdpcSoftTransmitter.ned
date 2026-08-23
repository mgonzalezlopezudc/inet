//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.packetlevel;

module Ieee80211LdpcSoftTransmitter extends Ieee80211Transmitter
{
    parameters:
        @class(Ieee80211LdpcSoftTransmitter);
        @signal[ldpcDataEncoded](type=long);
        @statistic[ldpcDataEncodeInvocations](title="LDPC DATA encode invocations"; source=ldpcDataEncoded; record=count);
}
