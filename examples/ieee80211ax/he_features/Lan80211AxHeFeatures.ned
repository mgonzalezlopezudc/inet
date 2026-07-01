// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211ax.he_features;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth100M;
import inet.node.inet.StandardHost;
import inet.node.inet.WirelessHost;
import inet.node.wireless.AccessPoint;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.visualizer.contract.IIntegratedVisualizer;

//
// 802.11ax HE advanced-feature showcase network.
//
// The topology consists of a wired server, one access point, and four wireless
// stations on an 80 MHz channel.  The 80 MHz channel is required because HE
// preamble puncturing is only defined for 80 MHz and 160 MHz operation.  The
// four hosts let the example demonstrate puncturing of one 20 MHz subchannel
// while still serving the remaining three stations concurrently.
//
network Lan80211AxHeFeatures
{
    parameters:
        int numHosts = default(4);
        @display("bgb=700,500");
    submodules:
        visualizer: <default(firstAvailableOrEmpty("IntegratedCanvasVisualizer"))> like IIntegratedVisualizer if typename != "" {
            parameters:
                @display("p=100,400;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            parameters:
                @display("p=100,100;is=s");
                config = xml("<config><interface hosts='*' address='145.236.x.x' netmask='255.255.0.0'/></config>");
        }
        radioMedium: Ieee80211ScalarRadioMedium {
            parameters:
                @display("p=100,200;is=s");
        }
        host[numHosts]: WirelessHost {
            @display("r=,,#707070");
            wlan[*].mgmt.typename = "Ieee80211MgmtStaSimplified";
            wlan[*].agent.typename = "";
        }
        ap: AccessPoint {
            @display("p=350,250;r=,,#707070");
            wlan[*].mgmt.typename = "Ieee80211MgmtApSimplified";
        }
        server: StandardHost {
            @display("p=550,250");
        }
    connections:
        server.ethg++ <--> Eth100M <--> ap.ethg++;
}
