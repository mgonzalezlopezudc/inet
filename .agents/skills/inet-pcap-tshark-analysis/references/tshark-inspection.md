## Contents

* Basic TShark inspection
* Use display filters
* IPv4 host
* IPv6 host
* Traffic between two IPv4 hosts
* Ethernet address
* TCP or UDP port
* ICMP and ICMPv6
* ARP and IPv6 Neighbor Discovery
* TCP connection establishment
* TCP termination or reset
* Packet data containing text
* Produce a concise packet timeline
* Correlate packet time with Cmdenv
* Filter by simulation time
* Analyze TCP exchanges
* Inspect endpoints, conversations, and protocol hierarchy

---

## Basic TShark inspection

Show the default packet summary:

```sh
tshark -n -r "$PCAP"
```

Show the first 20 packets:

```sh
tshark -n -r "$PCAP" -c 20
```

Show full decoded details:

```sh
tshark -n -r "$PCAP" -V
```

Show one packet in full, including a hexadecimal dump:

```sh
FRAME=1

tshark \
  -n \
  -r "$PCAP" \
  -Y "frame.number == $FRAME" \
  -V \
  -x
```

Use full decoding only for a small number of packets. `-V` output can become very large.

---

## Use display filters

Use `-Y` when reading an existing capture:

```sh
FILTER='tcp.port == 5000'

tshark \
  -n \
  -r "$PCAP" \
  -Y "$FILTER"
```

Always quote filters containing spaces, parentheses, comparison operators, or logical operators.

### IPv4 host

```sh
tshark -n -r "$PCAP" -Y 'ip.addr == 10.0.0.1'
```

### IPv6 host

```sh
tshark -n -r "$PCAP" -Y 'ipv6.addr == 2001:db8::1'
```

### Traffic between two IPv4 hosts

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y 'ip.addr == 10.0.0.1 && ip.addr == 10.0.0.2'
```

### Ethernet address

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y 'eth.addr == 02:00:00:00:00:01'
```

### TCP or UDP port

```sh
tshark -n -r "$PCAP" -Y 'tcp.port == 5000'
tshark -n -r "$PCAP" -Y 'udp.port == 5000'
```

### ICMP and ICMPv6

```sh
tshark -n -r "$PCAP" -Y 'icmp || icmpv6'
```

### ARP and IPv6 Neighbor Discovery

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y 'arp || icmpv6.type == 135 || icmpv6.type == 136'
```

### TCP connection establishment

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y 'tcp.flags.syn == 1'
```

### TCP termination or reset

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y 'tcp.flags.fin == 1 || tcp.flags.reset == 1'
```

### Packet data containing text

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y 'tcp contains "HELLO" || udp contains "HELLO"'
```

Failure to find text does not prove that it was not sent. The data may be:

* Encoded.
* Encrypted.
* Fragmented.
* Split across TCP segments.
* Captured at a layer that does not include it.
* Absent from the selected capture point.

---

## Produce a concise packet timeline

Export commonly useful fields:

```sh
tshark \
  -n \
  -r "$PCAP" \
  -T fields \
  -E header=y \
  -E separator=, \
  -E quote=d \
  -E occurrence=f \
  -e frame.number \
  -e frame.time_epoch \
  -e frame.time_delta_displayed \
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

Save the result:

```sh
TIMELINE="logs/${CONFIG}-${RUN}-packet-timeline.csv"

tshark \
  -n \
  -r "$PCAP" \
  -T fields \
  -E header=y \
  -E separator=, \
  -E quote=d \
  -E occurrence=f \
  -e frame.number \
  -e frame.time_epoch \
  -e ip.src \
  -e ip.dst \
  -e ipv6.src \
  -e ipv6.dst \
  -e tcp.srcport \
  -e tcp.dstport \
  -e udp.srcport \
  -e udp.dstport \
  -e _ws.col.Protocol \
  -e _ws.col.Info \
  > "$TIMELINE"
```

Some fields may be empty because the selected capture does not contain that protocol layer.

Before relying on an unfamiliar field, verify that the installed TShark knows it:

```sh
tshark -G fields | rg 'frame\.time_epoch|tcp\.seq|icmpv6\.type'
```

---

## Correlate packet time with Cmdenv

INET’s recorder writes the current simulation time as the capture timestamp.

Use:

```text
frame.time_epoch
```

to correlate TShark output with `%t` in the Cmdenv log.

Do not normally use:

```text
frame.time_relative
```

for direct correlation with simulation time. It is relative to the capture’s first packet or selected reference packet.

TShark frame numbers and OMNeT++ event numbers are independent:

```text
TShark frame.number != OMNeT++ event number
```

Extract packet time and identifying fields:

```sh
FILTER='tcp.stream == 0'

tshark \
  -n \
  -r "$PCAP" \
  -Y "$FILTER" \
  -T fields \
  -E header=y \
  -E separator=, \
  -E quote=d \
  -e frame.number \
  -e frame.time_epoch \
  -e ip.src \
  -e tcp.srcport \
  -e ip.dst \
  -e tcp.dstport \
  -e tcp.seq \
  -e tcp.ack \
  -e tcp.flags.str
```

Suppose the packet time is:

```text
10.250001000
```

Search the Cmdenv log:

```sh
rg -n -C 20 'time=10\.250001' "$LOG"
```

If formatting or timestamp precision differs, search a shorter prefix:

```sh
rg -n -C 30 'time=10\.25' "$LOG"
```

Also search using packet names, addresses, ports, or protocol-specific identifiers:

```sh
rg -n -i -C 20 \
  '10\.0\.0\.1|10\.0\.0\.2|5000|packetName|sequence' \
  "$LOG"
```

The module decision that caused transmission may occur slightly before the capture timestamp. Inspect a small surrounding interval rather than requiring exact equality.

---

## Filter by simulation time

Because `frame.time_epoch` represents the recorded simulation timestamp, filter by simulation time using:

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y 'frame.time_epoch >= 10.25 && frame.time_epoch <= 10.30'
```

Combine time and protocol filters:

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y '
    frame.time_epoch >= 10.25 &&
    frame.time_epoch <= 10.30 &&
    tcp.port == 5000
  '
```

Timestamp precision is limited by the recorder’s `timePrecision` setting. Events closer together than the stored precision may have identical capture timestamps.

---

## Analyze TCP exchanges

List TCP conversations:

```sh
tshark -n -q -r "$PCAP" -z conv,tcp
```

List TCP stream identifiers:

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y tcp \
  -T fields \
  -e tcp.stream |
  awk 'NF' |
  sort -n -u
```

Inspect one stream:

```sh
STREAM=0

tshark \
  -n \
  -r "$PCAP" \
  -Y "tcp.stream == $STREAM"
```

Export sequence and acknowledgement information:

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y "tcp.stream == $STREAM" \
  -T fields \
  -E header=y \
  -E separator=, \
  -E quote=d \
  -e frame.number \
  -e frame.time_epoch \
  -e ip.src \
  -e tcp.srcport \
  -e ip.dst \
  -e tcp.dstport \
  -e tcp.stream \
  -e tcp.seq \
  -e tcp.ack \
  -e tcp.len \
  -e tcp.flags.str \
  -e tcp.analysis.retransmission \
  -e tcp.analysis.fast_retransmission \
  -e tcp.analysis.spurious_retransmission \
  -e tcp.analysis.duplicate_ack
```

Show retransmissions:

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y '
    tcp.analysis.retransmission ||
    tcp.analysis.fast_retransmission ||
    tcp.analysis.spurious_retransmission
  '
```

Show general TCP analysis indications:

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y '
    tcp.analysis.flags ||
    tcp.analysis.lost_segment ||
    tcp.analysis.duplicate_ack ||
    tcp.analysis.out_of_order
  '
```

Follow a TCP stream as ASCII:

```sh
tshark \
  -n \
  -q \
  -r "$PCAP" \
  -z "follow,tcp,ascii,$STREAM"
```

Follow it as hexadecimal:

```sh
tshark \
  -n \
  -q \
  -r "$PCAP" \
  -z "follow,tcp,hex,$STREAM"
```

TShark’s TCP analysis fields are inferences based on packets present in the capture. Missing observation points, truncated captures, or a capture beginning midway through a connection may affect those conclusions.

Confirm important findings with:

* Captures from both endpoints.
* Cmdenv TCP logs.
* INET statistics.
* An OMNeT++ event log.

---

## Inspect endpoints, conversations, and protocol hierarchy

IPv4 endpoints:

```sh
tshark -n -q -r "$PCAP" -z endpoints,ip
```

IPv6 endpoints:

```sh
tshark -n -q -r "$PCAP" -z endpoints,ipv6
```

Ethernet endpoints:

```sh
tshark -n -q -r "$PCAP" -z endpoints,eth
```

UDP conversations:

```sh
tshark -n -q -r "$PCAP" -z conv,udp
```

Protocol hierarchy:

```sh
tshark -n -q -r "$PCAP" -z io,phs
```

Packet and byte statistics in one-second intervals:

```sh
tshark -n -q -r "$PCAP" -z io,stat,1
```

Select an interval suitable for the simulation timescale. A one-second interval may conceal short exchanges.

---

