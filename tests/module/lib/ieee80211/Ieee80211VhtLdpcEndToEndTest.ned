//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.test.moduletest.lib.ieee80211;

import inet.physicallayer.wireless.ieee80211.packetlevel.errormodel.Ieee80211LdpcPerSuccessModel;

module RecordingIeee80211LdpcPerSuccessModel extends Ieee80211LdpcPerSuccessModel
{
    parameters:
        @class(RecordingIeee80211LdpcPerSuccessModel);
}

simple Ieee80211VhtLdpcEndToEndTest
{
    parameters:
        string stationRadioModule;
        string stationDcfModule;
        string serverAppModule;
        string fecSuccessModelModule;
        string apMibModule;
        string stationAddress;
        string bssid;
        int expectedAssociationId;
        int expectedPartialAid;
        @class(Ieee80211VhtLdpcEndToEndTest);
}
