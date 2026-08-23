//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.ieee80211.packetlevel.errormodel;

import inet.physicallayer.wireless.common.base.packetlevel.ErrorModelBase;
import inet.physicallayer.wireless.ieee80211.packetlevel.errormodel.contract.IIeee80211FecSuccessModel;

module Ieee80211ErrorModelBase extends ErrorModelBase
{
    parameters:
        @class(Ieee80211ErrorModelBase);
    submodules:
        fecSuccessModel: <default("")> like IIeee80211FecSuccessModel if typename != "" {
            @display("p=100,100");
        }
}
