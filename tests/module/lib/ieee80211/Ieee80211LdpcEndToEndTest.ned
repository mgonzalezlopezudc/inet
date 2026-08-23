//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.test.moduletest.lib.ieee80211;

import inet.common.Module;
import inet.physicallayer.wireless.ieee80211.packetlevel.errormodel.contract.IIeee80211FecSuccessModel;

module DeterministicIeee80211FecSuccessModel extends Module like IIeee80211FecSuccessModel
{
    parameters:
        double targetDataSuccessRate;
        int minimumTargetUncodedBits;
        string expectedPhyFormat @enum("HT", "VHT_SU") = default("HT");
        @class(DeterministicIeee80211FecSuccessModel);
}

simple Ieee80211LdpcEndToEndTest
{
    parameters:
        string stationRadioModule;
        string stationDcfModule;
        string stationRecoveryModule;
        string serverAppModule;
        string fecSuccessModelModule;
        bool expectedSuccess;
        int expectedAttempts;
        int minimumTargetUncodedBits;
        @class(Ieee80211LdpcEndToEndTest);
}
