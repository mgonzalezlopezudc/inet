//
// Copyright (C) 2005 Michael Tuexen
// Copyright (C) 2008 Irene Ruengeler
// Copyright (C) 2009 Thomas Dreibholz
// Copyright (C) 2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.common.packet.recorder;

import inet.common.SimpleModule;

//
// Records PCAP traces of frames sent/received by other modules within
// the same host. By default, it records frames sent/received by L2 modules
// of ~StandardHost and ~Router. The output filename is expected in the
// `pcapFile` parameter. The `PcapRecorder` module can also print tcpdump-like
// textual information on the log (EV); this functionality can be
// controlled by the verbose parameter.
//
// <b>Which modules to record:</b> The list of modules can be specified in
// the `moduleNamePatterns` parameter. It is a space-separated list of module
// names, which will be interpreted as children of the `PcapRecorder`'s parent
// module. To record elements of a module vector, add "[*]" to the name
// (example: "eth[*]").
//
// <b>Operation:</b> `PcapRecorder` adds signal listeners to the recorded
// modules. The signals subscribed to are `packetSentToLower` and
// `packetReceivedFromLower`, but these names can be overridden with the
// `sendingSignalNames` and `receivingSignalNames` parameters. The packets
// themselves are expected as `cPacket*` signal values.
//
// IEEE 802.11 MAC frames are written using the Radiotap link type. The recorder
// prepends a Radiotap header containing the FCS state and direction. Channel
// frequency and received or transmitted power are also included when the packet
// signal provides the corresponding physical-layer transmission or reception.
// A-MPDUs are written as one packet record per MPDU; A-MPDU delimiters and
// inter-subframe padding are not included in the packet records.
//
simple PcapRecorder extends SimpleModule
{
    parameters:
        @class(PcapRecorder);
        bool verbose = default(true);  // Whether to log packets on the module output
        bool recordEmptyPackets = default(true); // Specifies if zero length packets are recorded or not
        bool enableConvertingPackets = default(true); // Specifies if converting packets to link type is allowed or not
        string pcapFile = default(""); // The PCAP file to be written, suggested value: pcapFile = "${resultdir}/${configname}-#${runnumber}" + fullpath() + ".pcap"
        string fileFormat @enum("pcap", "pcapng") = default("pcapng");
        int snaplen = default(65535);  // Maximum number of bytes to record per packet
        int timePrecision = default(6); // Time precision in recorded file. pcap supports only 6 (usec) or 9 (nanosec), pcapng supports more values (see 'if_tsresol' option in pcapng file format).
        bool dumpBadFrames = default(true); // Enable dump of frames with hasBitError
        string moduleNamePatterns = default("wlan[*] eth[*] ppp[*]"); // Space-separated list of sibling module names to listen on
        string sendingSignalNames = default("transmissionEnded"); // Space-separated list of outbound packet signals to subscribe to
        string receivingSignalNames = default("receptionEnded"); // Space-separated list of inbound packet signals to subscribe to
        string dumpProtocols = default("ethernetmac ppp ieee80211mac"); // Space-separated list of protocol names as defined in the Protocol class
        object packetFilter = default("*"); // Which packets are considered, matches all packets by default
        string helpers = default("");    // Usable PcapRecorder::IHelper helpers for accept packettype and store/convert packet as specified linktype
                                         // currently available: "inet::AckingMacToEthernetPcapRecorderHelper"
        string captureDirection @enum("both", "inbound", "outbound") = default("both"); // Direction gate after resolving an undefined signal direction from DirectionTag; "both" also accepts unresolved direction
        double captureStartTime @unit(s) = default(0s); // Inclusive, nonnegative start of the capture time window
        double captureEndTime @unit(s) = default(-1s); // Inclusive end of the capture time window; exactly -1s means no end
        int maxNumRecords = default(-1); // Exact output-record cap; exactly -1 is unlimited. Limit rejections count an offered packet already over the cap, or each known record candidate skipped after fanout reaches it.
        int outputBufferSize @unit(B) = default(0B); // Stdio output buffer size; 0 uses the libc default
        int flushInterval = default(0); // Flush after this many successfully written records; 0 disables periodic flushing and alwaysFlush takes precedence
        bool alwaysFlush = default(false); // Flush the pcapFile after each write to ensure that all packets are captured in case of a crash
        displayStringTextFormat = default("rec: %n pks");
        @display("i=block/blackboard");
        @signal[packetRecorded](type=Packet);
        // The following count signals are emitted exactly once at finish. Offered counts Packet observations. Direction/time gates run before filtering; filter and bad-frame rejections are mutually exclusive. Empty rejections occur after conversion.
        // Direct protocol and fallback dissection count the selected protocol path. Conversion counts each materialized helper conversion or generated segmented prefix. A-MPDU MPDU records count successfully written MPDU records.
        // Interface resolution counts are mutually exclusive per accepted observation: direct hierarchy lookup, source-module cache hit, tag/table lookup, or unresolved. Payload requested is pre-snaplen record payload; payload written is captured payload accepted by the writer. Writer flushes count successful explicit fflush calls, excluding close.
        @signal[offeredPacketCount](type=long);
        @signal[directionRejectedCount](type=long);
        @signal[timeRejectedCount](type=long);
        @signal[limitRejectedCount](type=long);
        @signal[filterRejectedCount](type=long);
        @signal[badFrameRejectedCount](type=long);
        @signal[emptyRejectedCount](type=long);
        @signal[directProtocolCount](type=long);
        @signal[fallbackDissectionCount](type=long);
        @signal[conversionCount](type=long);
        @signal[ampduMpduRecordCount](type=long);
        @signal[directInterfaceResolutionCount](type=long);
        @signal[cachedInterfaceResolutionCount](type=long);
        @signal[taggedInterfaceResolutionCount](type=long);
        @signal[unresolvedInterfaceResolutionCount](type=long);
        @signal[payloadBytesRequested](type=long);
        @signal[payloadBytesWritten](type=long);
        @signal[writerFlushCount](type=long);
        @statistic[offeredPacketCount](source=offeredPacketCount; record=last);
        @statistic[directionRejectedCount](source=directionRejectedCount; record=last);
        @statistic[timeRejectedCount](source=timeRejectedCount; record=last);
        @statistic[limitRejectedCount](source=limitRejectedCount; record=last);
        @statistic[filterRejectedCount](source=filterRejectedCount; record=last);
        @statistic[badFrameRejectedCount](source=badFrameRejectedCount; record=last);
        @statistic[emptyRejectedCount](source=emptyRejectedCount; record=last);
        @statistic[directProtocolCount](source=directProtocolCount; record=last);
        @statistic[fallbackDissectionCount](source=fallbackDissectionCount; record=last);
        @statistic[conversionCount](source=conversionCount; record=last);
        @statistic[ampduMpduRecordCount](source=ampduMpduRecordCount; record=last);
        @statistic[directInterfaceResolutionCount](source=directInterfaceResolutionCount; record=last);
        @statistic[cachedInterfaceResolutionCount](source=cachedInterfaceResolutionCount; record=last);
        @statistic[taggedInterfaceResolutionCount](source=taggedInterfaceResolutionCount; record=last);
        @statistic[unresolvedInterfaceResolutionCount](source=unresolvedInterfaceResolutionCount; record=last);
        @statistic[payloadBytesRequested](source=payloadBytesRequested; record=last; unit=B);
        @statistic[payloadBytesWritten](source=payloadBytesWritten; record=last; unit=B);
        @statistic[writerFlushCount](source=writerFlushCount; record=last);
}
