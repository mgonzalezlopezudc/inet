// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.examples.ieee80211be.mlo;

import inet.linklayer.contract.IWirelessInterface;
import inet.linklayer.ieee80211.Ieee80211Interface;
import inet.linklayer.ieee80211.mac.Ieee80211MldMac;
import inet.linklayer.ieee80211.llc.IIeee80211Llc;
import inet.linklayer.common.IIeee8021dQosClassifier;
import inet.linklayer.ieee80211.mac.Ieee80211MldMac;
import inet.linklayer.ieee80211.mib.Ieee80211Mib;
import inet.networklayer.common.NetworkInterface;

//
// A Multi-Link Device (MLD) interface comprising a single upper MLD MAC
// and multiple lower 802.11 interfaces.
//
module MldInterface extends NetworkInterface
{
    parameters:
        string interfaceTableModule;
        int numLinks = default(2);
        string address @mutable = default("auto"); // MLD MAC address
        
        **.interfaceTableModule = default(absPath(this.interfaceTableModule));
        *.energySourceModule = default(absPath("^.energySourceModule"));
        **.mobilityModule = default(absPath("^.mobility"));
        mldMac.mldMacAddress = this.address;

    gates:
        input upperLayerIn;
        output upperLayerOut;
    submodules:
        mib: Ieee80211Mib {
            @display("p=100,50");
        }
        classifier: <default("OmittedIeee8021dQosClassifier")> like IIeee8021dQosClassifier {
            @display("p=100,100");
        }
        llc: <default("Ieee80211LlcEpd")> like IIeee80211Llc {
            @display("p=100,200");
        }
        mldMac: Ieee80211MldMac {
            numLinks = parent.numLinks;
            @display("p=100,300");
        }
        link[numLinks]: Ieee80211Interface {
            mac.mldMacModule = default(absPath("^.mldMac"));
            **.interfaceTableModule = default(absPath("^.^.^.interfaceTable"));
        }
    connections allowunconnected:
        classifier.in <-- upperLayerIn;
        classifier.out --> llc.upperLayerIn;
        llc.upperLayerOut --> upperLayerOut;
        
        llc.lowerLayerOut --> mldMac.upperLayerIn;
        llc.lowerLayerIn <-- mldMac.upperLayerOut;
        
        for i=0..numLinks-1 {
            mldMac.lowerLinkOut[i] --> link[i].upperLayerIn;
            mldMac.lowerLinkIn[i] <-- link[i].upperLayerOut;
        }
}
