// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

import inet.common.SimpleModule;
import inet.linklayer.lowpan.contract.ILowpanLayer;

// IPv6 adaptation with RFC 4944 fragmentation; static native next-hop bindings.
simple LowpanLayer extends SimpleModule like ILowpanLayer
{
    parameters:
        @class(LowpanLayer);
        string headerCompression @enum("none", "iphc") = default("iphc");
        int maxReassemblyDatagrams = default(64);
        int maxReassemblyBytes @unit(B) = default(131008B);
        double reassemblyTimeout @unit(s) = default(60s); // RFC 4944 maximum is 60 seconds.
        @display("i=block/layer");
        @lifecycleSupport;
        @signal[datagramAccepted](type=inet::Packet);
        @signal[datagramCompleted](type=inet::Packet);
        @signal[packetDropped](type=inet::Packet);
        @signal[fragmentSent](type=inet::Packet);
        @signal[fragmentReceived](type=inet::Packet);
        @signal[reassemblyCompleted](type=inet::Packet);
        @signal[reassemblyExpired](type=long);
        @statistic[datagramAccepted](record=count);
        @statistic[datagramCompleted](record=count);
        @statistic[packetDropped](record=count);
        @statistic[fragmentSent](record=count);
        @statistic[fragmentReceived](record=count);
        @statistic[reassemblyCompleted](record=count);
        @statistic[reassemblyExpired](record=sum);
    gates:
        input upperLayerIn;
        output upperLayerOut;
        input lowerLayerIn;
        output lowerLayerOut;
}
