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
        bool nativeFragmentForwarding = default(false); // Requires native pacing, no Mesh, and declared <=8-hop compatible data deployment.
        xml fragmentForwardingPeers = default(xml("<peers/>")); // Administrative actual-native peer allowlist, scoped by this interface/PAN. Empty keeps ingress ordinary.
        string routingTableModule = default("");
        string icmpv6Module = default("");
        string ipv6Module = default(""); // Same-node LowpanIpv6; registered policy hooks force ordinary fallback.
        int maxForwardingRecords = default(64); // 1..64, including ordinary dispositions and closed identities.
        int maxForwardingRanges = default(4); // 1..4 pending input ranges; carry is additional.
        int maxForwardingPendingBytes @unit(B) = default(512B);
        int maxForwardingCarryBytes @unit(B) = default(128B);
        int maxForwardingBytes @unit(B) = default(32768B); // Aggregate retained pending/carry originals, excluding bounded header/range metadata.
        int forwardingPayloadLimit @unit(B) = default(0B); // 0: native envelope; otherwise 13..127, further limited by actual envelope. No MAC MTU change.
        int forwardingMaxHops = default(8); // Declared deployment maximum, 1..8; never inferred from IPv6 Hop Limit.
        double forwardingResidence @unit(s) = default(1s); // Finite >=one tick and <=1s, anchored at ingress, includes queue/MAC/propagation.
        double forwardingGap @unit(s) = default(150ms); // Finite >=one tick, after prior matched lower success.
        double forwardingActiveLifetime @unit(s) = default(90s); // <=90s; must exceed effective source residence + hop residence +60s.
        double forwardingHistoryLifetime @unit(s) = default(120s); // <=120s; outlives active; source protection must cover full path/history sum.
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
