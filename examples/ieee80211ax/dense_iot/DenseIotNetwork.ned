// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.dense_iot;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth100G;
import inet.node.inet.StandardHost;
import inet.node.inet.WirelessHost;
import inet.node.wireless.AccessPoint;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;

//
// A single dense BSS with a wired application server and many stationary IoT
// stations. The same topology is used by the 802.11ax OFDMA/TWT treatment and
// the 802.11ac EDCA/no-TWT baseline.
//
network DenseIotNetwork
{
    parameters:
        int numStations = default(128);
        @display("bgb=600,440");
    submodules:
        configurator: Ipv4NetworkConfigurator {
            parameters:
                config = xml("<config><interface hosts='*' address='10.0.x.x' netmask='255.255.0.0'/></config>");
                @display("p=70,60;is=s");
        }
        radioMedium: Ieee80211ScalarRadioMedium {
            @display("p=70,150;is=s");
        }
        ap: AccessPoint {
            @display("p=300,220;r=,,#707070");
        }
        sta[numStations]: WirelessHost {
            @display("r=,,#707070");
        }
        server: StandardHost {
            @display("p=500,220");
        }
    connections:
        server.ethg++ <--> Eth100G <--> ap.ethg++;
}
