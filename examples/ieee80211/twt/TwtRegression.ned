// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211.twt;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth100M;
import inet.node.inet.StandardHost;
import inet.node.inet.WirelessHost;
import inet.node.wireless.AccessPoint;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;

network TwtRegression
{
    parameters:
        int numStations = default(2);
        @display("bgb=600,360");
    submodules:
        configurator: Ipv4NetworkConfigurator {
            parameters:
                config = xml("<config><interface hosts='*' address='10.0.x.x' netmask='255.255.0.0'/></config>");
                @display("p=70,50");
        }
        radioMedium: Ieee80211ScalarRadioMedium {
            @display("p=70,130");
        }
        ap: AccessPoint {
            parameters:
                wlan[*].mgmt.typename = "Ieee80211TwtExampleMgmtAp";
                @display("p=290,180");
        }
        sta[numStations]: WirelessHost {
            parameters:
                wlan[*].mgmt.typename = "Ieee80211MgmtSta";
                wlan[*].agent.typename = "Ieee80211TwtExampleAgent";
                @display("p=160,280");
        }
        server: StandardHost {
            @display("p=500,180");
        }
    connections:
        server.ethg++ <--> Eth100M <--> ap.ethg++;
}
