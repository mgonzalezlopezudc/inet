// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

import inet.linklayer.contract.IMacProtocol;
import inet.linklayer.contract.IWirelessInterface;
import inet.linklayer.lowpan.contract.ILowpanLayer;
import inet.linklayer.lowpan.contract.ILowpanLink;
import inet.networklayer.common.NetworkInterface;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadio;
import inet.queueing.contract.IPacketQueue;

// IPv6 service over the explicit compatibility profile; not a native wire MAC.
module Ieee802154LowpanInterface extends NetworkInterface like IWirelessInterface
{
    parameters:
        @class(::inet::NetworkInterface);
        string interfaceTableModule = default("");
        string energySourceModule = default("");
        string linkDomainModule = default("");
        *.interfaceTableModule = default(absPath(this.interfaceTableModule));
        *.energySourceModule = default(absPath(this.energySourceModule));
        *.linkDomainModule = default(absPath(this.linkDomainModule));
    gates:
        input upperLayerIn;
        output upperLayerOut;
        input radioIn @labels(IWirelessSignal);
    submodules:
        lowpan: <default("LowpanLayer")> like ILowpanLayer {
            @display("p=150,100");
        }
        boundary: <default("LowpanCompatibilityBoundary")> like ILowpanLink {
            @display("p=150,200");
        }
        queue: <default("PacketQueue")> like IPacketQueue {
            packetCapacity = default(100);
            @display("p=250,200");
        }
        mac: <default("LowpanCompatibilityMac")> like IMacProtocol {
            @display("p=350,200");
        }
        radio: <default("Ieee802154NarrowbandRadio")> like IRadio {
            @display("p=350,300");
        }
    connections:
        upperLayerIn --> lowpan.upperLayerIn;
        lowpan.lowerLayerOut --> boundary.in;
        boundary.out --> queue.in;
        queue.out --> mac.upperLayerIn;
        mac.lowerLayerOut --> radio.upperLayerIn;
        radioIn --> radio.radioIn;
        radio.upperLayerOut --> mac.lowerLayerIn;
        mac.upperLayerOut --> boundary.lowerLayerIn;
        boundary.upperLayerOut --> lowpan.lowerLayerIn;
        lowpan.upperLayerOut --> upperLayerOut;
}
