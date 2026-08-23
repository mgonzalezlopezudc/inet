//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.test.moduletest.lib.ieee80211;

simple Ieee80211LdpcSoftEndToEndTest
{
    parameters:
        string stationRadioModule;
        string stationDcfModule;
        string stationRecoveryModule;
        string serverAppModule;
        string stationTransmitterModule;
        string apReceiverModule;
        string expectedPhyFormat @enum("HT", "VHT_SU");
        bool expectedExact;
        bool expectedSuccess;
        int expectedAttempts;
        int minimumTargetPsduOctets;
        @class(Ieee80211LdpcSoftEndToEndTest);
}
