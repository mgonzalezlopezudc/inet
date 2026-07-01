// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211ax.bss_coloring;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth100M;
import inet.node.inet.StandardHost;
import inet.node.inet.WirelessHost;
import inet.node.wireless.AccessPoint;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.visualizer.contract.IIntegratedVisualizer;

//
// Network topology for demonstrating 802.11ax BSS Coloring and Spatial Reuse.
//
// It consists of two overlapping BSSs:
// - BSS 1: AP1 (150, 250) and associated STA1_1, STA1_2 (100, 240/260)
// - BSS 2: AP2 (400, 250) and associated STA2_1, STA2_2 (450, 240/260)
//
// Having 2 STAs per BSS ensures that the AP downlink scheduler triggers
// HE MU-OFDMA transmissions (which carry BSS Color) instead of falling
// back to single-user (SU) transmissions.
//
network BssColoringNetwork
{
    parameters:
        @display("bgb=600,400");
    submodules:
        visualizer: <default(firstAvailableOrEmpty("IntegratedCanvasVisualizer"))> like IIntegratedVisualizer if typename != "" {
            parameters:
                @display("p=100,320;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            parameters:
                @display("p=100,50;is=s");
                config = xml("<config><interface hosts='*' address='192.168.1.x' netmask='255.255.255.0'/></config>");
        }
        radioMedium: Ieee80211ScalarRadioMedium {
            parameters:
                @display("p=100,120;is=s");
        }
        ap1: AccessPoint {
            parameters:
                @display("p=150,250;r=120,,#707070");
                wlan[*].mgmt.typename = "Ieee80211MgmtApSimplified";
        }
        sta1[2]: WirelessHost {
            parameters:
                @display("p=100,250;r=,,#707070");
                wlan[*].mgmt.typename = "Ieee80211MgmtStaSimplified";
                wlan[*].agent.typename = "";
        }
        ap2: AccessPoint {
            parameters:
                @display("p=400,250;r=120,,#707070");
                wlan[*].mgmt.typename = "Ieee80211MgmtApSimplified";
        }
        sta2[2]: WirelessHost {
            parameters:
                @display("p=450,250;r=,,#707070");
                wlan[*].mgmt.typename = "Ieee80211MgmtStaSimplified";
                wlan[*].agent.typename = "";
        }
        server1: StandardHost {
            parameters:
                @display("p=150,150");
        }
        server2: StandardHost {
            parameters:
                @display("p=350,150");
        }
    connections:
        server1.ethg++ <--> Eth100M <--> ap1.ethg++;
        server2.ethg++ <--> Eth100M <--> ap2.ethg++;
}
