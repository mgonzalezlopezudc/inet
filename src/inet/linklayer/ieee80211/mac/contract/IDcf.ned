//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.linklayer.ieee80211.mac.contract;

//
// Interface for modules that implement the DCF (Distributed Coordination Function)
// for IEEE 802.11.
//
moduleinterface IDcf
{
    parameters:
        string channelWidthSelectionPolicy @enum("dynamic", "static");
        @display("i=block/layer");
}
