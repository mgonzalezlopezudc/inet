//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.packetlevel.errormodel;

import inet.common.Module;
import inet.physicallayer.wireless.ieee80211.packetlevel.errormodel.contract.IIeee80211FecSuccessModel;

module Ieee80211LdpcPerSuccessModel extends Module like IIeee80211FecSuccessModel
{
    parameters:
        string perTableFile = default("inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211LdpcPer1458B.csv"); // 1458-byte AWGN calibration; override for another calibrated payload length.
        @class(Ieee80211LdpcPerSuccessModel);
        @display("i=block/table");
}
