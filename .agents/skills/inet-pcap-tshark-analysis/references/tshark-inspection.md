## Contents

* Validate and scope the capture
* Export a packet timeline
* Correlate with simulation evidence
* Analyze TCP exchanges
* Diagnose missing fields

## Validate and scope the capture

Inspect the file before drawing conclusions:

```sh
capinfos "$PCAP"
tshark -n -r "$PCAP" -c 20
tshark -n -q -r "$PCAP" -z io,phs
```

Use `-Y` for offline display filters. Quote filters and preserve the original capture. Use `-V -x` only for a few selected frames because full decoding is large:

```sh
tshark -n -r "$PCAP" -Y 'frame.number == 12' -V -x
```

A zero-match filter proves only that the selected observation point contains no matching decoded frame.

## Export a packet timeline

Start with fields that identify the packet and observation point:

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y "$FILTER" \
  -T fields \
  -E header=y \
  -E separator=, \
  -E quote=d \
  -E occurrence=f \
  -e frame.number \
  -e frame.time_epoch \
  -e frame.interface_id \
  -e frame.interface_name \
  -e eth.src \
  -e eth.dst \
  -e ip.src \
  -e ip.dst \
  -e ipv6.src \
  -e ipv6.dst \
  -e tcp.srcport \
  -e tcp.dstport \
  -e udp.srcport \
  -e udp.dstport \
  -e _ws.col.Protocol \
  -e _ws.col.Info
```

Remove irrelevant fields and add protocol-specific sequence, flag, or identifier fields. Empty fields may mean the captured representation lacks that protocol layer; do not interpret them as zero.

## Correlate with simulation evidence

INET records simulation time as the packet timestamp. Correlate `frame.time_epoch` with `%t` in Cmdenv logs. `frame.time_relative` is relative to the capture and is unsuitable for direct simulation-time correlation.

TShark frame numbers and OMNeT++ event numbers are independent. Match packets across files using addresses, protocol fields, sequence identifiers, payload length or identity, and timestamp—not `frame.number`. For TCP, prefer `tcp.seq_raw` or explicitly disable relative sequence numbering because `tcp.seq` may be capture-local.

When the exact timestamp formatting differs, search a small surrounding interval in the Cmdenv log; the decision that caused a transmission may precede the captured packet.

## Analyze TCP exchanges

Use stream identifiers to scope analysis:

```sh
tshark -n -q -r "$PCAP" -z conv,tcp
tshark -n -r "$PCAP" -Y 'tcp' -T fields -e tcp.stream
tshark -n -r "$PCAP" -Y "tcp.stream == $STREAM"
```

For retransmission diagnosis, export `frame.time_epoch`, endpoints, `tcp.seq`, `tcp.ack`, `tcp.len`, flags, and the relevant `tcp.analysis.*` fields. These analysis fields are TShark inferences based only on packets present in the capture. Confirm important conclusions with both endpoints, Cmdenv logs, results, or an event log.

Use stream following only when payload reconstruction matters:

```sh
tshark -n -q -r "$PCAP" -z "follow,tcp,ascii,$STREAM"
```

## Diagnose missing fields

Confirm unfamiliar fields against the dissector field table:

```sh
tshark -G fields | rg 'frame\.time_epoch|tcp\.seq|wlan\.fc\.retry'
```

If frames decode only as `Data` or expected fields are absent:

1. Inspect the capture encapsulation and protocol hierarchy.
2. Decode one frame with `-V -x`.
3. Check the recorder's `dumpProtocols`, capture module, and link type.
4. Determine whether the capture starts at Ethernet, raw IP, IEEE 802.11, PPP, or another representation.
5. Use recorder logs or simulator evidence when no Wireshark dissector represents the needed INET data.
