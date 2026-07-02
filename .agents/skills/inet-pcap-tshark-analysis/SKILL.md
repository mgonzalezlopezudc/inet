---
name: inet-pcap-tshark-analysis
description: Record and analyze packet exchanges in INET simulations using PcapRecorder, Cmdenv, TShark, and capinfos. Use when asked to find packets, inspect protocol headers, analyze TCP streams or retransmissions, compare captures from different nodes or interfaces, verify that an exchange occurred, or correlate network packets with Cmdenv simulation logs.
---

# Analyzing INET packet exchanges with PcapRecorder and TShark

Use this skill when the question concerns protocol-visible packets exchanged by an INET simulation.

Examples include:

* Did a particular packet cross an interface?
* Which TCP, UDP, ICMP, ICMPv6, ARP, IPv4, IPv6, Ethernet, or IEEE 802.11 packets were exchanged?
* Which addresses, ports, flags, sequence numbers, or acknowledgement numbers were used?
* Was a TCP segment retransmitted?
* Did a packet appear at the sender but not at the receiver?
* What traffic occurred during a particular simulation-time interval?
* Which Cmdenv event or module log corresponds to a captured packet?

Use the complementary skills when the question is primarily about:

* Internal module decisions or log messages: `inet-cmdenv-log-analysis`.
* Message creation, scheduling, delivery, and simulator causality: `omnetpp-eventlog-analysis`.
* General simulation execution or startup failures: `inet-simulation-run`.
* Source-level C++ failures: `inet-lldb-debugging`.

A packet capture shows packets at selected observation points. It does not, by itself, explain why an INET module made a routing, queueing, MAC, or drop decision.

---

## General rules

* Use PCAPng rather than legacy PCAP unless compatibility requires PCAP.
* Use command-line overrides for temporary packet recording; do not edit `omnetpp.ini` solely to enable a diagnostic capture.
* Capture only the nodes, interfaces, protocols, and time interval needed for the investigation.
* Use unique filenames for every configuration, run, node, interface, and recorder.
* Use `tshark -n` for automated analysis to disable name resolution and improve reproducibility.
* Use TShark display filters with `-Y` when reading an existing capture.
* Do not use `-f` to filter an existing capture; `-f` is a live-capture filter.
* Do not confuse a TShark frame number with an OMNeT++ event number.
* Do not claim that a packet was dropped merely because it is absent from one capture.
* Do not claim that a packet was delivered merely because it appears in a sender-side capture.
* Preserve the original capture whenever producing filtered or converted captures.

---

## Prerequisites

Run commands from the same working directory expected by the simulation.

Verify the environment:

```sh
: "${INET_ROOT:?INET_ROOT must point to the INET project root}"

command -v opp_run >/dev/null ||
    { echo "opp_run is not available on PATH" >&2; exit 1; }

command -v tshark >/dev/null ||
    { echo "tshark is not available on PATH" >&2; exit 1; }

test -f "$INET_ROOT/src/libINET.so" ||
    {
        echo "INET release library not found:" >&2
        echo "  $INET_ROOT/src/libINET.so" >&2
        exit 1
    }
```

Assume OMNeT++, INET, and related tools such as `opp_run`, `opp_run_dbg`, and
`opp_repl` are already sourced when they are available on `PATH`. Do not prefix
every command with `source "$OMNETPP_ROOT/setenv"` or `source setenv`. Source an
environment script only as an explicit recovery step after validation fails, and
state why it was necessary.

Record the TShark version:

```sh
tshark --version
```

Check whether `capinfos` is available:

```sh
if ! command -v capinfos >/dev/null; then
    echo "Warning: capinfos is not available" >&2
fi
```

`capinfos` is useful but not required. TShark can still validate and inspect the capture.

---

## Define the common simulation command

Use the same base invocation as the `inet-simulation-run` skill:

```sh
INI_FILE="${INI_FILE:-omnetpp.ini}"
PROJECT_NED_ROOT="${PROJECT_NED_ROOT:-.}"
CONFIG="${CONFIG:?Set CONFIG to the OMNeT++ configuration name}"
RUN="${RUN:-0}"

INET_RUN=(
  opp_run
  -u Cmdenv
  -f "$INI_FILE"
  "--image-path=$INET_ROOT/images"
  "--ned-path=$PROJECT_NED_ROOT;$INET_ROOT/src;$INET_ROOT/examples;$INET_ROOT/tutorials;$INET_ROOT/showcases"
  -l "$INET_ROOT/src/libINET.so"
)
```

If the project contains compiled C++ modules, append its release library:

```sh
PROJECT_LIBRARY="${PROJECT_LIBRARY:-}"

if [[ -n "$PROJECT_LIBRARY" ]]; then
    test -f "$PROJECT_LIBRARY" ||
        {
            echo "Project library not found: $PROJECT_LIBRARY" >&2
            exit 1
        }

    INET_RUN+=(-l "$PROJECT_LIBRARY")
fi
```

Do not use release libraries with `opp_run_dbg` or debug libraries with `opp_run`.

---

## Determine the capture point

Before configuring a recorder, inspect the NED model and existing INI configuration to determine:

* The actual node path.
* Whether the node supports `numPcapRecorders`.
* The interface names under the node.
* Whether the desired observation point is Ethernet, Wi-Fi, PPP, IPv4, IPv6, or another protocol layer.

Common node patterns include:

```text
*.host[0]
*.client
*.server
*.router[0]
*.switch[0]
```

Common interface module names include:

```text
eth[0]
eth[*]
wlan[0]
wlan[*]
ppp[0]
ppp[*]
```

Do not assume these paths exist. Check the project’s NED definitions.

`moduleNamePatterns` contains module names relative to the network node containing the recorder. It is not an arbitrary full network path.

For example:

```text
eth[0]
```

not:

```text
Network.host[0].eth[0]
```

Nodes derived from the usual INET link-layer node bases, including common hosts and routers, normally expose `numPcapRecorders`. A custom node may not. If the parameter is absent, inspect its NED inheritance rather than forcing the override.

---

## Fast packet capture without detailed Cmdenv logging

Use this when the primary goal is to create a capture for TShark:

```sh
NODE='*.host[0]'
CAPTURE_MODULES='eth[0]'

mkdir -p logs

PCAP="logs/${CONFIG}-${RUN}-host0-eth0.pcapng"

CAPTURE_ARGS=(
  "--${NODE}.numPcapRecorders=1"
  "--${NODE}.pcapRecorder[0].pcapFile=\"$PCAP\""
  "--${NODE}.pcapRecorder[0].fileFormat=\"pcapng\""
  "--${NODE}.pcapRecorder[0].timePrecision=9"
  "--${NODE}.pcapRecorder[0].moduleNamePatterns=\"$CAPTURE_MODULES\""
  "--${NODE}.pcapRecorder[0].verbose=false"
  "--${NODE}.pcapRecorder[0].alwaysFlush=true"
)

"${INET_RUN[@]}" \
  -c "$CONFIG" \
  -r "$RUN" \
  --cmdenv-express-mode=true \
  "${CAPTURE_ARGS[@]}"
```

Use `alwaysFlush=true` for diagnostic and crash investigations. It reduces the chance of losing the final packets if the simulation aborts, but may increase I/O overhead.

For a long performance-sensitive run that is expected to complete normally, consider:

```sh
"--${NODE}.pcapRecorder[0].alwaysFlush=false"
```

---

## Capture packets and Cmdenv context together

Use normal Cmdenv mode when captured packets must be correlated with module logs:

```sh
NODE='*.host[0]'
CAPTURE_MODULES='eth[0]'

mkdir -p logs
set -o pipefail

PCAP="logs/${CONFIG}-${RUN}-host0-eth0.pcapng"
LOG="logs/${CONFIG}-${RUN}.cmdenv.log"

"${INET_RUN[@]}" \
  -c "$CONFIG" \
  -r "$RUN" \
  --cmdenv-express-mode=false \
  --cmdenv-event-banners=false \
  '--cmdenv-log-prefix=[%l] event=%e time=%t module=%M: ' \
  "--${NODE}.numPcapRecorders=1" \
  "--${NODE}.pcapRecorder[0].pcapFile=\"$PCAP\"" \
  "--${NODE}.pcapRecorder[0].fileFormat=\"pcapng\"" \
  "--${NODE}.pcapRecorder[0].timePrecision=9" \
  "--${NODE}.pcapRecorder[0].moduleNamePatterns=\"$CAPTURE_MODULES\"" \
  "--${NODE}.pcapRecorder[0].alwaysFlush=true" \
  "--${NODE}.pcapRecorder[0].verbose=false" \
  '--**.cmdenv-log-level=info' \
  2>&1 | tee "$LOG"

status=$?
echo "opp_run exit status: $status"
```

When only one protocol subtree matters, use targeted logging rather than globally enabling `debug` or `trace`:

```sh
'--*.host[0].tcp.**.cmdenv-log-level=debug'
'--**.cmdenv-log-level=off'
```

Adapt the module path to the actual model.

---

## Print recorder packet descriptions in Cmdenv

`PcapRecorder` can print packet descriptions through OMNeT++ logging.

Use this when a searchable text representation is useful in addition to the binary capture:

```sh
NODE='*.host[0]'
CAPTURE_MODULES='eth[0]'

mkdir -p logs
set -o pipefail

PCAP="logs/${CONFIG}-${RUN}-host0.pcapng"
LOG="logs/${CONFIG}-${RUN}-packets.log"

"${INET_RUN[@]}" \
  -c "$CONFIG" \
  -r "$RUN" \
  --cmdenv-express-mode=false \
  --cmdenv-event-banners=false \
  '--cmdenv-log-prefix=[%l] event=%e time=%t module=%M: ' \
  "--${NODE}.numPcapRecorders=1" \
  "--${NODE}.pcapRecorder[0].pcapFile=\"$PCAP\"" \
  "--${NODE}.pcapRecorder[0].fileFormat=\"pcapng\"" \
  "--${NODE}.pcapRecorder[0].timePrecision=9" \
  "--${NODE}.pcapRecorder[0].moduleNamePatterns=\"$CAPTURE_MODULES\"" \
  "--${NODE}.pcapRecorder[0].alwaysFlush=true" \
  "--${NODE}.pcapRecorder[0].verbose=true" \
  "--${NODE}.pcapRecorder[0].cmdenv-log-level=debug" \
  '--**.cmdenv-log-level=off' \
  2>&1 | tee "$LOG"
```

Use TShark, not the textual recorder log, as the primary source for exact decoded packet-header fields.

---

## Choose the recorded protocol representation

By default, `PcapRecorder` is normally configured to record common link-layer representations such as Ethernet, PPP, and IEEE 802.11.

To record an IPv4 representation instead:

```sh
"--${NODE}.pcapRecorder[0].dumpProtocols=\"ipv4\""
```

For IPv6:

```sh
"--${NODE}.pcapRecorder[0].dumpProtocols=\"ipv6\""
```

To allow either:

```sh
"--${NODE}.pcapRecorder[0].dumpProtocols=\"ipv4 ipv6\""
```

`moduleNamePatterns` selects where packet signals are observed. `dumpProtocols` selects which protocol representation is written.

Do not assume that every protocol can be represented by the selected PCAP link type. If INET reports that it cannot determine or convert the link type, inspect:

* `moduleNamePatterns`.
* `dumpProtocols`.
* The packet’s protocol tags.
* Available recorder helpers.
* The selected capture layer.

---

## Checksums and frame check sequences

When the investigation requires valid computed checksum or FCS fields in the capture, the corresponding INET modules may need computed modes:

```sh
'--**.checksumMode="computed"'
'--**.fcsMode="computed"'
```

Only add these overrides when the model supports them and the capture requires calculated values.

Do not silently alter checksum or FCS modes when doing so would change the experiment’s intended behavior. Record the overrides in the final report.

---

## Restrict packet recording inside INET

Prefer reducing the capture at its source when a long run would otherwise create a very large file.

### Limit interfaces

```sh
"--${NODE}.pcapRecorder[0].moduleNamePatterns=\"eth[0]\""
```

### Limit captured size

```sh
"--${NODE}.pcapRecorder[0].snaplen=256"
```

A reduced `snaplen` may truncate headers or payloads required for later analysis. Use the default full capture unless file size is a problem.

### Filter packet classes or fields

Example for ARP packets:

```sh
"--${NODE}.pcapRecorder[0].packetFilter=expr(has(ArpPacket))"
```

Example for packets containing TCP and IPv4 headers:

```sh
"--${NODE}.pcapRecorder[0].packetFilter=expr(has(TcpHeader) && has(Ipv4Header))"
```

INET packet-filter expressions are not Wireshark display filters. Do not use TShark syntax in `packetFilter`.

For complex expressions, consult the INET packet-filter documentation and inspect examples in the installed INET tree.

When uncertain, record a broader capture and filter it afterward with TShark.

---

## Record multiple interfaces separately

Use multiple recorders when a router or node has several relevant interfaces:

```sh
NODE='*.router[0]'

"${INET_RUN[@]}" \
  -c "$CONFIG" \
  -r "$RUN" \
  "--${NODE}.numPcapRecorders=2" \
  "--${NODE}.pcapRecorder[0].pcapFile=\"logs/${CONFIG}-${RUN}-router0-eth0.pcapng\"" \
  "--${NODE}.pcapRecorder[0].moduleNamePatterns=\"eth[0]\"" \
  "--${NODE}.pcapRecorder[0].alwaysFlush=true" \
  "--${NODE}.pcapRecorder[1].pcapFile=\"logs/${CONFIG}-${RUN}-router0-ppp0.pcapng\"" \
  "--${NODE}.pcapRecorder[1].moduleNamePatterns=\"ppp[0]\"" \
  "--${NODE}.pcapRecorder[1].alwaysFlush=true"
```

Use separate files so the capture point remains unambiguous.

---

## Validate the generated capture

Verify that the file exists and is nonempty:

```sh
test -s "$PCAP" ||
    {
        echo "Capture file is missing or empty: $PCAP" >&2
        exit 1
    }
```

Inspect metadata when `capinfos` is available:

```sh
if command -v capinfos >/dev/null; then
    capinfos "$PCAP"
fi
```

Ask TShark to read the first packets:

```sh
tshark -n -r "$PCAP" -c 10
```

A successful simulation does not guarantee that the configured recorder captured any packets.

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

## Export a filtered capture

Keep the original capture and write matches to another file:

```sh
FILTER='ip.addr == 10.0.0.1 && tcp.port == 5000'
FILTERED="logs/${CONFIG}-${RUN}-filtered.pcapng"

tshark \
  -n \
  -r "$PCAP" \
  -Y "$FILTER" \
  -w "$FILTERED"
```

Validate the result:

```sh
test -s "$FILTERED" ||
    {
        echo "No packets were written to: $FILTERED" >&2
        exit 1
    }

if command -v capinfos >/dev/null; then
    capinfos "$FILTERED"
fi
```

Do not overwrite the original capture.

---

## Assert that a packet exists

Use this pattern for an automated check:

```sh
set -o pipefail

FILTER='tcp.flags.syn == 1 && ip.dst == 10.0.0.2'

match_count=$(
  tshark \
    -n \
    -r "$PCAP" \
    -Y "$FILTER" \
    -T fields \
    -e frame.number |
  awk 'NF { count++ } END { print count + 0 }'
)

if (( match_count == 0 )); then
    echo "No packets matched: $FILTER" >&2
    exit 1
fi

echo "Matched packets: $match_count"
```

A zero match count means only that the selected capture contains no matching decoded packets. It does not prove that the packet did not exist elsewhere in the network.

---

## Compare sender and receiver captures

Capture both endpoints when investigating delivery or loss.

Use unique recorder configurations:

```sh
CLIENT='*.client'
SERVER='*.server'

CLIENT_PCAP="logs/${CONFIG}-${RUN}-client.pcapng"
SERVER_PCAP="logs/${CONFIG}-${RUN}-server.pcapng"

"${INET_RUN[@]}" \
  -c "$CONFIG" \
  -r "$RUN" \
  "--${CLIENT}.numPcapRecorders=1" \
  "--${CLIENT}.pcapRecorder[0].pcapFile=\"$CLIENT_PCAP\"" \
  "--${CLIENT}.pcapRecorder[0].timePrecision=9" \
  "--${CLIENT}.pcapRecorder[0].alwaysFlush=true" \
  "--${SERVER}.numPcapRecorders=1" \
  "--${SERVER}.pcapRecorder[0].pcapFile=\"$SERVER_PCAP\"" \
  "--${SERVER}.pcapRecorder[0].timePrecision=9" \
  "--${SERVER}.pcapRecorder[0].alwaysFlush=true"
```

Do not match packets by `frame.number`; each file has independent frame numbering.

Correlate packets using combinations of:

* Source and destination addresses.
* Source and destination ports.
* Protocol.
* IP identification or fragmentation fields.
* TCP sequence and acknowledgement numbers.
* TCP payload length.
* ICMP identifier and sequence number.
* Application-level identifier.
* Packet length.
* Payload data.
* Simulation timestamp.

A packet appearing in the sender capture but not the receiver capture shows only that it did not appear at the receiver’s selected observation point.

To determine where or why it disappeared, add:

* An intermediate router capture.
* A capture on another interface.
* Targeted Cmdenv logging.
* Packet-drop statistics.
* An OMNeT++ event log.

---

## Account for duplicate observations

The same logical packet may appear several times because it was:

* Recorded on transmission and reception signals.
* Recorded at several nodes or interfaces.
* Recorded before and after encapsulation.
* Forwarded through a router.
* Retransmitted.
* Copied for multicast or broadcast delivery.

Do not assume every PCAP frame represents a unique application packet.

Use:

* Capture filename and interface.
* Direction metadata when available.
* Addresses and ports.
* Protocol headers.
* Sequence numbers.
* Packet lengths.
* Simulation timestamps.

to distinguish repeated observations from distinct packets.

---

## Diagnose an empty capture

When the simulation completes but the capture is empty:

1. Confirm the file exists.
2. Confirm the node path matches an instantiated node.
3. Confirm the node supports `numPcapRecorders`.
4. Confirm `numPcapRecorders` is nonzero.
5. Confirm `pcapFile` is nonempty.
6. Confirm `moduleNamePatterns` matches an actual sibling module.
7. Confirm packets crossed the selected module.
8. Confirm the simulation ran long enough for traffic to begin.
9. Check `dumpProtocols`.
10. Check `packetFilter`.
11. Check whether the packet can be converted to the selected PCAP link type.
12. Run once with `PcapRecorder.verbose=true`.
13. Enable recorder logging at `debug`.
14. Search the Cmdenv log for recorder initialization messages.

Example search:

```sh
rg -n -i -C 5 \
  'pcap|recorder|subscrib|module .* not found|link type|convert' \
  "$LOG"
```

Temporarily broaden the interface pattern:

```sh
"--${NODE}.pcapRecorder[0].moduleNamePatterns=\"wlan[*] eth[*] ppp[*]\""
```

Do not leave the broad setting in place after locating the correct interface.

---

## Diagnose undecoded packets

If TShark reports only `Data`, an unexpected protocol, or missing fields:

1. Inspect the protocol hierarchy:

   ```sh
   tshark -n -q -r "$PCAP" -z io,phs
   ```

2. Inspect one frame fully:

   ```sh
   tshark \
     -n \
     -r "$PCAP" \
     -Y 'frame.number == 1' \
     -V \
     -x
   ```

3. Check the capture format and encapsulation:

   ```sh
   capinfos "$PCAP"
   ```

4. Review `dumpProtocols`.

5. Review the selected capture module.

6. Determine whether the protocol has a Wireshark dissector.

7. Determine whether the capture starts at Ethernet, raw IP, IEEE 802.11, PPP, or another link type.

8. Use PcapRecorder’s textual logging or an OMNeT++ event log when the required INET representation cannot be decoded by TShark.

Do not infer a header value from an empty field. The captured representation may not contain that header.

---

## Recommended investigation workflow

For “Why was this packet not received?”:

1. Reproduce one configuration and run number.
2. Capture at the sender and receiver.
3. Capture Cmdenv output with simulation timestamps.
4. Validate both capture files.
5. Identify the packet at the sender using exact protocol fields.
6. Search for the same packet at the receiver.
7. Compare simulation timestamps.
8. Inspect Cmdenv around those times.
9. Add captures at intermediate interfaces if necessary.
10. Enable targeted logging for the suspected protocol module.
11. Use an event log when simulator-level causality remains unclear.

For “Did this TCP exchange behave correctly?”:

1. List TCP conversations.
2. Select the relevant `tcp.stream`.
3. Export flags, lengths, sequence numbers, and acknowledgements.
4. Inspect retransmission and duplicate-ACK analysis.
5. Follow the stream when payload inspection matters.
6. Compare captures from both endpoints.
7. Correlate anomalies with TCP-module Cmdenv logs.
8. Report TShark heuristics separately from direct packet evidence.

For “Find a particular message or payload”:

1. Search decoded fields with an appropriate display filter.
2. Search packet bytes using `contains`.
3. Follow the relevant TCP or UDP stream.
4. Check whether the data is encoded or encrypted.
5. Check captures at different protocol layers.
6. Correlate the packet time with the Cmdenv log.

---

## Reporting requirements

Report:

* Exact simulation command.
* Working directory.
* `INET_ROOT`.
* INI file.
* Configuration and run number.
* Node paths captured.
* Interface or module patterns captured.
* `dumpProtocols`, `packetFilter`, `snaplen`, and checksum/FCS overrides when used.
* PCAP or PCAPng filenames.
* Cmdenv log filename.
* Simulation exit status.
* TShark version.
* Exact TShark commands.
* Display filters.
* TShark frame numbers.
* Packet simulation timestamps from `frame.time_epoch`.
* Relevant OMNeT++ event numbers.
* Relevant module paths.
* Addresses, ports, protocols, flags, and sequence identifiers.
* Whether the packet appeared at one or several observation points.
* Any ambiguity caused by capture position, timestamp precision, truncated packets, or missing dissectors.
* Whether a conclusion is direct packet evidence, Cmdenv evidence, or a TShark heuristic.

Do not:

* Confuse frame numbers with event numbers.
* Claim loss from one missing observation.
* Claim delivery from a sender-side capture.
* Claim retransmission solely because payloads are duplicated.
* Treat `frame.time_relative` as absolute simulation time.
* Search a binary capture directly with `grep` or `rg`.
* Use `-f` where a TShark display filter with `-Y` is required.
* Modify the original capture when producing a filtered artifact.
* Hide unsuccessful filters or empty captures from the final report.
