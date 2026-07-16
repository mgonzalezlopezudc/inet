// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.examples.ieee80211ax.common;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth100G;
import inet.node.inet.StandardHost;
import inet.node.inet.WirelessHost;
import inet.node.wireless.AccessPoint;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadioMedium;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.visualizer.contract.IIntegratedVisualizer;

//
// \brief Common wired-server, AP, and wireless station topology.
//
// This base network serves as the architectural foundation for the majority of the
// 802.11ax (HE) example configurations (such as Downlink OFDMA, Uplink OFDMA, OMI,
// Dynamic Fragmentation, and MU-MIMO). 
//
// Scenario-specific networks extend this compound module and override parameter/host values.
// This ensures that model paths (e.g. `**.host[0].wlan[0]`) remain identical and stable
// across different configurations, enabling modular INI file inheritances.
//
// Key submodules included are:
// - \p configurator (Ipv4NetworkConfigurator): Automatically assigns IP addresses and setups static routing.
// - \p radioMedium (Ieee80211ScalarRadioMedium by default): Models the shared wireless medium, handling pathloss and interference.
// - \p host[] (WirelessHost): A collection of wireless client stations using simplified 802.11 management (Ieee80211MgmtStaSimplified) by default.
// - \p ap (AccessPoint): The local BSS Access Point using simplified AP management (Ieee80211MgmtApSimplified).
// - \p server (StandardHost): A wired backend host connected directly to the AP via a 100 Gbps Ethernet link (Eth100G).
//
network HeSingleBssNetwork
{
    parameters:
        int numHosts = default(3);
        xml configuratorConfig = default(xml("<config><interface hosts='*' address='145.236.x.x' netmask='255.255.0.0'/></config>"));
        @display("bgb=600,400");
    submodules:
        visualizer: <default(firstAvailableOrEmpty("IntegratedCanvasVisualizer"))> like IIntegratedVisualizer if typename != "" {
            @display("p=100,300;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            config = default(parent.configuratorConfig);
            @display("p=100,100;is=s");
        }
        radioMedium: <default("Ieee80211ScalarRadioMedium")> like IRadioMedium {
            @display("p=100,200;is=s");
        }
        host[numHosts]: WirelessHost {
            wlan[*].mgmt.typename = "Ieee80211MgmtStaSimplified";
            wlan[*].agent.typename = "";
            @display("r=,,#707070");
        }
        ap: AccessPoint {
            wlan[*].mgmt.typename = "Ieee80211MgmtApSimplified";
            @display("p=250,250;r=,,#707070");
        }
        server: StandardHost {
            @display("p=250,300");
        }
    connections:
        server.ethg++ <--> Eth100G <--> ap.ethg++;
}
