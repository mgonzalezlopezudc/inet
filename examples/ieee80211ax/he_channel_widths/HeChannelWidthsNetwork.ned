// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.he_channel_widths;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth100M;
import inet.node.inet.StandardHost;
import inet.node.inet.WirelessHost;
import inet.node.wireless.AccessPoint;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;

// Same offered load and topology for 20/40/80/160 MHz HE operation.
network HeChannelWidthsNetwork
{
    parameters:
        int numStations = default(4);
        @display("bgb=650,400");
    submodules:
        configurator: Ipv4NetworkConfigurator {
            parameters:
                config = xml("<config><interface hosts='*' address='10.10.x.x' netmask='255.255.0.0'/></config>");
        }
        radioMedium: Ieee80211ScalarRadioMedium;
        ap: AccessPoint {
            parameters:
                @display("p=300,200;r=,,#707070");
                wlan[*].mgmt.typename = "Ieee80211MgmtApSimplified";
        }
        sta[numStations]: WirelessHost {
            parameters:
                @display("r=,,#707070");
                wlan[*].mgmt.typename = "Ieee80211MgmtStaSimplified";
                wlan[*].agent.typename = "";
        }
        server: StandardHost { @display("p=520,200"); }
    connections:
        server.ethg++ <--> Eth100M <--> ap.ethg++;
}
