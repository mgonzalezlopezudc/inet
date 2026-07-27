// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.twt;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth100G;
import inet.node.inet.StandardHost;
import inet.node.inet.WirelessHost;
import inet.node.wireless.AccessPoint;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;

//
// \brief Target Wake Time (TWT) regression and showcase network.
//
// This network consists of an Access Point (ap), a wired backend server, and a variable
// number of TWT-capable wireless stations. It is designed to verify the TWT power-saving
// protocol mechanisms (individual TWT agreements and broadcast TWT modes) under deterministic
// scheduling constraints.
//
network TwtRegression
{
    parameters:
        int numStations = default(2);
        @display("bgb=600,360");
    submodules:
        configurator: Ipv4NetworkConfigurator {
            parameters:
                config = xml("<config><interface hosts='*' address='10.0.x.x' netmask='255.255.0.0'/></config>");
                @display("p=70,50;is=s");
        }
        radioMedium: Ieee80211ScalarRadioMedium {
            @display("p=70,130;is=s");
        }
        ap: AccessPoint {
            parameters:
                wlan[*].mgmt.typename = "Ieee80211TwtExampleMgmtAp";
                @display("p=290,180;r=,,#707070");
        }
        sta[numStations]: WirelessHost {
            parameters:
                wlan[*].mgmt.typename = "Ieee80211MgmtSta";
                wlan[*].agent.typename = "Ieee80211TwtExampleAgent";
                @display("r=,,#707070");
        }
        server: StandardHost {
            @display("p=500,180");
        }
    connections:
        server.ethg++ <--> Eth100G <--> ap.ethg++;
}
