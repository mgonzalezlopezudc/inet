//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.test.moduletest.lib.ieee80211;

simple Ieee80211VhtSigAEndToEndTest
{
    parameters:
        string stationRadioModule;
        string apRadioModule;
        string uplinkSinkModule;
        string downlinkSinkModule;
        string apMibModule;
        string stationAddress;
        string bssid;
        int expectedAssociationId = default(-1);
        int expectedStaToApPartialAid;
        int expectedApToStaPartialAid;
        bool expectedLdpc = default(false);
        bool expectedAdhoc = default(false);
        bool verifyMissingAssociationIdInvariant = default(false);
        @class(Ieee80211VhtSigAEndToEndTest);
}
