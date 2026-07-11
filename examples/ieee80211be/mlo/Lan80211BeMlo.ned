// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211be.mlo;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth1G;
import inet.node.inet.StandardHost;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.visualizer.contract.IIntegratedVisualizer;
import inet.examples.ieee80211be.mlo.MldAp;
import inet.examples.ieee80211be.mlo.MldHost;

//
// 802.11be Multi-Link Operation (MLO) showcase network.
//
// Demonstrates MLD AP and STA aggregating throughput across two links
// (e.g., 5 GHz and 6 GHz) using the Ieee80211MldMac.
//
network Lan80211BeMlo
{
    parameters:
        @display("bgb=700,500");
    submodules:
        visualizer: <default(firstAvailableOrEmpty("IntegratedCanvasVisualizer"))> like IIntegratedVisualizer if typename != "" {
            parameters:
                @display("p=100,400;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            parameters:
                @display("p=100,100;is=s");
                config = xml("<config>"
                    + "<interface hosts='server ap' names='eth*' address='145.236.0.x' netmask='255.255.255.0'/>"
                    + "<interface hosts='ap host' names='mldWlan' address='145.236.1.x' netmask='255.255.255.0'/>"
                    + "<autoroute destinationInterfaces='*.server.eth0 *.ap.eth0 *.ap.mldWlan *.host.mldWlan'/>"
                    + "</config>");
        }
        radioMedium5GHz: Ieee80211ScalarRadioMedium {
            parameters:
                @display("p=100,200;is=s");
        }
        radioMedium6GHz: Ieee80211ScalarRadioMedium {
            parameters:
                @display("p=100,300;is=s");
        }
        host: MldHost {
            @display("p=250,250;r=,,#707070");
            numLinks = 2;
            mldWlan.link[*].mgmt.typename = "Ieee80211MgmtStaSimplified";
            mldWlan.link[*].agent.typename = "";
        }
        ap: MldAp {
            @display("p=350,250;r=,,#707070");
            numLinks = 2;
            mldWlan.link[*].mgmt.typename = "Ieee80211MgmtApSimplified";
            mldWlan.link[*].agent.typename = "";
        }
        server: StandardHost {
            @display("p=550,250");
        }
    connections:
        server.ethg++ <--> Eth1G <--> ap.ethg++;
}
