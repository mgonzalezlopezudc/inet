// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211be.eht_features;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth1G;
import inet.node.inet.StandardHost;
import inet.node.inet.WirelessHost;
import inet.node.wireless.AccessPoint;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.visualizer.contract.IIntegratedVisualizer;

//
// 802.11be EHT baseline features showcase network.
//
// The topology consists of a wired server, one access point, and wireless
// stations. This example demonstrates raw EHT physical layer capabilities
// like 320 MHz channel bandwidth and 4096-QAM in the 6 GHz band.
//
network Lan80211BeEhtFeatures
{
    parameters:
        int numHosts = default(1);
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
        server.ethg++ <--> Eth1G <--> ap.ethg++;
}
