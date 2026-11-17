// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

import inet.common.SimpleModule;
import inet.linklayer.lowpan.contract.ILowpanLayer;

// IPv6 adaptation with RFC 4944 fragmentation and configured or registered native next-hop bindings.
simple LowpanLayer extends SimpleModule like ILowpanLayer
{
    parameters:
        @class(LowpanLayer);
        string headerCompression @enum("none", "iphc") = default("iphc");
        xml compressionContexts = default(xml("<contexts/>")); // At most 16 context elements: id, canonical prefix, prefixLength, compress (default true), absolute validUntil (default unlimited).
        int maxReassemblyDatagrams = default(64);
        int maxReassemblyBytes @unit(B) = default(131008B);
        double reassemblyTimeout @unit(s) = default(60s); // RFC 4944 maximum is 60 seconds.
        bool meshUnder = default(false); // Native-only configured L2 mesh; optimized ND remains one-hop.
        bool nativeFragmentPacing = default(false); // Native unicast data only; Mesh unsupported. Control/broadcast remain immediate.
        int maxPacedSourceDatagrams = default(64);
        int maxPacedSourceBytes @unit(B) = default(32768B); // Retained originals; bounded per-flow header/range metadata is additional.
        double sourceFragmentGap @unit(s) = default(500ms); // After previous matched ACK; at least one simulation tick.
        double sourceFragmentResidence @unit(s) = default(20s); // Fixed admission-to-last-delivery horizon, never refreshed.
        double sourceTagProtection @unit(s) = default(150s); // At least residence plus maximum 60s endpoint retention; effective clock durations saturate.
        xml meshRoutes = default(xml("<routes/>")); // Exact route elements: final and nextHop native extended addresses; duplicate finals rejected.
        int meshHops = default(8); // Initial transmission budget, 0..255; zero refuses source transmission.
        double meshResidence @unit(s) = default(1s); // Finite >= one simulation tick; quantized per-hop queue/MAC/propagation budget, uniform across participants.
        double broadcastRetention @unit(s) = default(60s); // Finite >= one tick; quantized first-arrival retention, uniform across participants. Large durations saturate at clock range.
        int maxBroadcastDatagrams = default(256); // Positive record count; each has a fixed 256-offset bitmap. No live eviction.
        @display("i=block/layer");
        @lifecycleSupport;
        @signal[datagramAccepted](type=inet::Packet);
        @signal[datagramCompleted](type=inet::Packet);
        @signal[packetDropped](type=inet::Packet);
        @signal[fragmentSent](type=inet::Packet);
        @signal[fragmentReceived](type=inet::Packet);
        @signal[reassemblyCompleted](type=inet::Packet);
        @signal[reassemblyExpired](type=long);
        @signal[reassemblyInvalidated](type=long);
        @signal[meshForwarded](type=inet::Packet);
        @signal[nativeFrameTransmissionSucceeded](type=inet::Packet); // Borrowed terminal original; matched ACK or local broadcast TX, not remote delivery.
        @signal[nativeFrameTransmissionFailed](type=inet::Packet); // Borrowed live-correlated result; lifecycle cancellation is not replayed.
        @statistic[datagramAccepted](record=count);
        @statistic[datagramCompleted](record=count);
        @statistic[packetDropped](record=count);
        @statistic[fragmentSent](record=count);
        @statistic[fragmentReceived](record=count);
        @statistic[reassemblyCompleted](record=count);
        @statistic[reassemblyExpired](record=sum);
        @statistic[reassemblyInvalidated](record=sum);
        @statistic[meshForwarded](record=count);
        @statistic[nativeFrameTransmissionSucceeded](record=count);
        @statistic[nativeFrameTransmissionFailed](record=count);
    gates:
        input upperLayerIn;
        output upperLayerOut;
        input lowerLayerIn;
        output lowerLayerOut;
}
