## Contents

* 21. Packet Capture with PcapRecorder
* 21.1 Add recorders
* 21.2 Verify capture encapsulation
* 21.3 Capture-point limitations
* 22. TShark Analysis
* 22.1 Discover available fields
* 22.2 Basic frame timeline
* 22.3 Management frames
* 22.4 RTS, CTS, and ACK
* 22.5 Retries
* 22.6 QoS frames
* 22.7 PHY metadata
* 22.8 FCS and malformed frames
* 22.9 Compare multiple captures
* 23. Selective Cmdenv Logging
* 23.1 Disable express-mode suppression
* 23.2 Disable global noise
* 23.3 Enable only relevant components
* 23.4 Search logs
* 23.5 Required log observations
* 24. OMNeT++ Event Log
* 25. Scalar and Vector Analysis with opp_scavetool
* 26. Source Inspection
* 26.1 Locate MAC components
* 26.2 Locate PHY components
* 26.3 Locate parameters and signals
* 26.4 Locate frame definitions
* 26.5 Identify actual policy decisions

---

# 21. Packet Capture with PcapRecorder

INET’s `PcapRecorder` can subscribe to wireless-interface signals, record IEEE 802.11 MAC frames, write PCAPng, preserve bad frames when configured, and flush after every packet. Exact defaults are version-dependent.

## 21.1 Add recorders

Example:

```ini
[Config WifiDebug]
extends = ExistingConfiguration

*.client.numPcapRecorders = 1
*.client.pcapRecorder[0].moduleNamePatterns = "wlan[*]"
*.client.pcapRecorder[0].pcapFile = \
    "${resultdir}/${configname}-#${runnumber}-client.pcapng"
*.client.pcapRecorder[0].fileFormat = "pcapng"
*.client.pcapRecorder[0].dumpProtocols = "ieee80211mac"
*.client.pcapRecorder[0].dumpBadFrames = true
*.client.pcapRecorder[0].alwaysFlush = true
*.client.pcapRecorder[0].verbose = false

*.accessPoint.numPcapRecorders = 1
*.accessPoint.pcapRecorder[0].moduleNamePatterns = "wlan[*] eth[*]"
*.accessPoint.pcapRecorder[0].pcapFile = \
    "${resultdir}/${configname}-#${runnumber}-ap.pcapng"
*.accessPoint.pcapRecorder[0].fileFormat = "pcapng"
*.accessPoint.pcapRecorder[0].dumpProtocols = "ieee80211mac ethernetmac"
*.accessPoint.pcapRecorder[0].dumpBadFrames = true
*.accessPoint.pcapRecorder[0].alwaysFlush = true
*.accessPoint.pcapRecorder[0].verbose = false

*.server.numPcapRecorders = 1
*.server.pcapRecorder[0].moduleNamePatterns = "eth[*]"
*.server.pcapRecorder[0].pcapFile = \
    "${resultdir}/${configname}-#${runnumber}-server.pcapng"
*.server.pcapRecorder[0].fileFormat = "pcapng"
*.server.pcapRecorder[0].alwaysFlush = true
*.server.pcapRecorder[0].verbose = false
```

Adapt paths and parameters to the installed node and interface definitions.

## 21.2 Verify capture encapsulation

Run:

```sh
capinfos capture.pcapng
```

Then:

```sh
tshark -r capture.pcapng -c 10 -V
```

Determine whether the capture contains:

* Native IEEE 802.11 MAC frames
* Ethernet-converted frames
* Radiotap-like metadata
* Bad frames
* Only successful MAC receptions
* Transmit and receive directions

Do not expect radiotap fields unless the capture actually includes a radiotap header or equivalent metadata.

## 21.3 Capture-point limitations

A MAC-level capture may not show:

* Signals below energy detection
* Failed preamble detection
* Receiver synchronization decisions
* Interference-only energy
* Every unsuccessful PHY attempt
* Complete SNIR history
* Radio-state transitions
* NAV state
* Frozen backoff state

Use PHY logs, signals, vectors, and event logs for these.

---

# 22. TShark Analysis

TShark can read PCAPng captures, apply display filters, and export selected fields. Wireshark maintains extensive `wlan` and `radiotap` field namespaces, but available fields depend on the installed version and capture encapsulation.

## 22.1 Discover available fields

Do not guess field names.

```sh
tshark -G fields |
  awk -F '\t' '$3 ~ /^(wlan|wlan_radio|radiotap)\./ {print $3}' |
  sort -u
```

Search specific concepts:

```sh
tshark -G fields |
  grep -Ei $'\t(wlan|wlan_radio|radiotap)\\..*(mcs|signal|noise|channel|retry|sequence|qos|duration|fcs)'
```

## 22.2 Basic frame timeline

```sh
tshark \
  -r capture.pcapng \
  -Y 'wlan' \
  -T fields \
  -E header=y \
  -E separator=$'\t' \
  -E occurrence=a \
  -e frame.number \
  -e frame.time_relative \
  -e frame.len \
  -e wlan.fc.type \
  -e wlan.fc.subtype \
  -e wlan.fc.retry \
  -e wlan.fc.morefrag \
  -e wlan.fc.tods \
  -e wlan.fc.fromds \
  -e wlan.ta \
  -e wlan.ra \
  -e wlan.sa \
  -e wlan.da \
  -e wlan.bssid \
  -e wlan.seq \
  -e wlan.frag \
  -e wlan.duration \
  -e wlan.qos.tid \
  -e _ws.col.Info
```

Remove fields not supported by the installed TShark.

## 22.3 Management frames

```sh
tshark \
  -r capture.pcapng \
  -Y 'wlan.fc.type == 0' \
  -T fields \
  -E header=y \
  -E separator=$'\t' \
  -e frame.number \
  -e frame.time_relative \
  -e wlan.fc.type_subtype \
  -e wlan.ta \
  -e wlan.ra \
  -e wlan.bssid \
  -e wlan.ssid \
  -e wlan.fixed.status_code \
  -e wlan.fixed.reason_code \
  -e _ws.col.Info
```

Useful filters commonly include:

```text
wlan.fc.type_subtype == 0x08    Beacon
wlan.fc.type_subtype == 0x04    Probe Request
wlan.fc.type_subtype == 0x05    Probe Response
wlan.fc.type_subtype == 0x0b    Authentication
wlan.fc.type_subtype == 0x00    Association Request
wlan.fc.type_subtype == 0x01    Association Response
wlan.fc.type_subtype == 0x02    Reassociation Request
wlan.fc.type_subtype == 0x03    Reassociation Response
wlan.fc.type_subtype == 0x0a    Disassociation
wlan.fc.type_subtype == 0x0c    Deauthentication
```

Confirm subtype values against the installed Wireshark field reference.

## 22.4 RTS, CTS, and ACK

```sh
tshark \
  -r capture.pcapng \
  -Y 'wlan.fc.type == 1' \
  -T fields \
  -E header=y \
  -E separator=$'\t' \
  -e frame.number \
  -e frame.time_relative \
  -e wlan.fc.type_subtype \
  -e wlan.ra \
  -e wlan.ta \
  -e wlan.duration \
  -e _ws.col.Info
```

Common control subtype filters include:

```text
wlan.fc.type_subtype == 0x1b    RTS
wlan.fc.type_subtype == 0x1c    CTS
wlan.fc.type_subtype == 0x1d    ACK
```

Verify using the installed dissector.

## 22.5 Retries

```sh
tshark \
  -r capture.pcapng \
  -Y 'wlan.fc.retry == 1' \
  -T fields \
  -E header=y \
  -E separator=$'\t' \
  -e frame.number \
  -e frame.time_relative \
  -e wlan.ta \
  -e wlan.ra \
  -e wlan.seq \
  -e wlan.frag \
  -e wlan.qos.tid \
  -e _ws.col.Info
```

Group by:

```text
TA, RA, TID, sequence number, fragment number
```

## 22.6 QoS frames

```sh
tshark \
  -r capture.pcapng \
  -Y 'wlan.qos' \
  -T fields \
  -E header=y \
  -E separator=$'\t' \
  -e frame.number \
  -e frame.time_relative \
  -e wlan.ta \
  -e wlan.ra \
  -e wlan.qos.tid \
  -e wlan.qos.ack \
  -e wlan.seq \
  -e wlan.fc.retry \
  -e _ws.col.Info
```

Use field discovery if `wlan.qos.ack` is unavailable or named differently.

## 22.7 PHY metadata

When radiotap or equivalent fields exist:

```sh
FIELDS=$(
  tshark -G fields |
    awk -F '\t' '$3 ~ /^radiotap\./ {print $3}'
)

printf '%s\n' "$FIELDS" |
  grep -Ei 'channel|freq|rate|mcs|vht|he|eht|signal|noise|antenna'
```

Then extract only confirmed fields.

Example:

```sh
tshark \
  -r capture.pcapng \
  -T fields \
  -E header=y \
  -E separator=$'\t' \
  -e frame.number \
  -e frame.time_relative \
  -e radiotap.channel.freq \
  -e radiotap.datarate \
  -e radiotap.dbm_antsignal \
  -e radiotap.dbm_antnoise \
  -e _ws.col.Info
```

Do not report absent radiotap information as zero.

## 22.8 FCS and malformed frames

Discover:

```sh
tshark -G fields |
  grep -Ei $'\twlan\\..*(fcs|checksum|malformed|error)'
```

Then filter bad frames using confirmed fields.

Also inspect INET-specific packet-error annotations in logs and result vectors because capture-level FCS status may not expose every simulator reception decision.

## 22.9 Compare multiple captures

Normalize relevant fields:

```sh
extract_wifi() {
  input=$1
  output=$2

  tshark \
    -r "$input" \
    -Y 'wlan' \
    -T fields \
    -E header=y \
    -E separator=$'\t' \
    -e frame.time_relative \
    -e wlan.fc.type_subtype \
    -e wlan.ta \
    -e wlan.ra \
    -e wlan.sa \
    -e wlan.da \
    -e wlan.bssid \
    -e wlan.seq \
    -e wlan.frag \
    -e wlan.fc.retry \
    -e wlan.qos.tid \
    -e frame.len \
    -e _ws.col.Info \
    >"$output"
}

extract_wifi sender.pcapng sender.tsv
extract_wifi receiver.pcapng receiver.tsv
```

Do not directly `diff` captures without accounting for:

* Different observation times
* Different headers
* Retransmissions
* Address transformation at the AP
* Encapsulation and decapsulation
* Failed frames captured only at one location
* Independent frame numbering

---

# 23. Selective Cmdenv Logging

Use logging after captures identify the relevant node or layer.

## 23.1 Disable express-mode suppression

```ini
cmdenv-express-mode = false
cmdenv-event-banners = true
cmdenv-event-banner-details = true
cmdenv-log-prefix = "[%l] t=%t ev=%e %C for %E: %|"
```

## 23.2 Disable global noise

```ini
**.cmdenv-log-level = off
```

## 23.3 Enable only relevant components

Example patterns:

```ini
*.client.wlan[*].mac.**.cmdenv-log-level = trace
*.client.wlan[*].radio.**.cmdenv-log-level = debug

*.accessPoint.wlan[*].mac.**.cmdenv-log-level = trace
*.accessPoint.wlan[*].radio.**.cmdenv-log-level = debug
*.accessPoint.bridging.**.cmdenv-log-level = debug

*.server.eth[*].**.cmdenv-log-level = debug
```

Module paths vary.

Start with `debug`. Escalate narrowly to `trace`.

## 23.4 Search logs

```sh
grep -nEi \
  'backoff|contention|channel|medium|NAV|RTS|CTS|ACK|BlockAck|ADDBA|retry|drop|collision|reception|SNIR|association|authentication|probe|beacon|queue|TXOP' \
  "$RESULT_DIR/cmdenv.log"
```

Search packet identity:

```sh
grep -nF 'packetNameOrSequence' "$RESULT_DIR/cmdenv.log"
```

## 23.5 Required log observations

Record:

* Event number
* Simulation time
* Module path
* Radio state
* Reception state
* Channel busy/idle transition
* NAV transition
* Queue state
* Backoff value
* Contention-window value
* Retry counter
* Selected frame-exchange sequence
* Selected rate or mode
* PHY result
* Drop reason
* Management state

Do not paste unbounded logs into the final diagnosis. Extract the minimal causal sequence.

---

# 24. OMNeT++ Event Log

Use the event log when the relationship among timers, sends, deliveries, and callbacks is unclear.

```ini
record-eventlog = true
eventlog-file = "${resultdir}/${configname}-#${runnumber}.elog"
```

Restrict the interval:

```ini
eventlog-recording-intervals = 1.200s..1.240s
```

Restrict modules:

```ini
*.client.wlan[*].**.module-eventlog-recording = true
*.accessPoint.wlan[*].**.module-eventlog-recording = true
*.accessPoint.bridging.**.module-eventlog-recording = true
**.module-eventlog-recording = false
```

Use it to answer:

* Which event scheduled the backoff timer?
* Which busy transition froze contention?
* Which event granted channel access?
* Which transmission created this reception?
* Which timeout caused retransmission?
* Which event deleted or dropped the frame?
* Which callback changed association state?
* Why did an ACK timeout fire despite an ACK transmission?

Do not record a full long simulation unless targeted recording cannot reproduce the defect.

---

# 25. Scalar and Vector Analysis with opp_scavetool

After the simulation produces `.sca` and `.vec` files, first discover available result names.

```sh
opp_scavetool query -l \
  "$RESULT_DIR"/*.sca \
  "$RESULT_DIR"/*.vec
```

Filter to wireless modules:

```sh
opp_scavetool query -l \
  -f 'module =~ "**.wlan[*].**"' \
  "$RESULT_DIR"/*.sca \
  "$RESULT_DIR"/*.vec
```

Search likely names without assuming exact spelling:

```sh
opp_scavetool query -l \
  -f 'name =~ "*retry*" OR name =~ "*drop*" OR name =~ "*reception*" OR name =~ "*transmission*" OR name =~ "*snir*" OR name =~ "*signal*" OR name =~ "*backoff*" OR name =~ "*queue*"' \
  "$RESULT_DIR"/*.sca \
  "$RESULT_DIR"/*.vec
```

Relevant result categories may include:

* Packet transmissions
* Successful and failed receptions
* Packet drops by reason
* Retry-limit drops
* Bit-error receptions
* Signal power
* Noise power
* Interference power
* SNIR
* PHY mode or bitrate
* Queue length
* Queueing delay
* Backoff or contention statistics
* Radio state
* Channel utilization
* Association events
* Handover timing
* Throughput and packet delay

Export selected results only after discovering their exact names:

```sh
opp_scavetool export \
  -F CSV-R \
  -o "$RESULT_DIR/wifi-debug.csv" \
  -f 'module =~ "**.wlan[*].**" AND (name =~ "*retry*" OR name =~ "*drop*")' \
  "$RESULT_DIR"/*.sca \
  "$RESULT_DIR"/*.vec
```

Correlate vector timestamps with:

* PCAP timestamps
* Cmdenv log timestamps
* Event numbers
* LLDB breakpoints

Do not rely only on aggregate packet counts when debugging a specific frame exchange.

---

# 26. Source Inspection

INET module paths, parameter names, signals, and policies change between releases.

Use the source as the authority for the installed version.

## 26.1 Locate MAC components

```sh
rg -n \
  'class .*Dcf|class .*Edca|class .*Hcf|Dcaf|Edcaf|Txop|Rts|Cts|Ack|BlockAck|RecoveryProcedure' \
  "$INET_ROOT/src/inet/linklayer/ieee80211"
```

## 26.2 Locate PHY components

```sh
rg -n \
  'Ieee80211.*Radio|Ieee80211.*Receiver|Ieee80211.*Transmitter|ErrorModel|SNIR|Sensitivity|energyDetection' \
  "$INET_ROOT/src/inet/physicallayer"
```

## 26.3 Locate parameters and signals

```sh
rg -n \
  'rtsThreshold|retryLimit|fragmentation|aggregation|qosStation|txopLimit|cwMin|cwMax|aifs|sifs|slotTime' \
  "$INET_ROOT/src/inet/linklayer/ieee80211"
```

```sh
rg -n \
  '@signal|emit\\(|packetDropped|receptionStateChanged|transmissionStateChanged|radioChannelChanged' \
  "$INET_ROOT/src/inet/linklayer/ieee80211" \
  "$INET_ROOT/src/inet/physicallayer"
```

## 26.4 Locate frame definitions

```sh
rg -n \
  'Ieee80211.*Header|SequenceControl|BlockAck|Addba|QosControl|Duration' \
  "$INET_ROOT/src/inet"
```

## 26.5 Identify actual policy decisions

Read the code that decides:

* Whether RTS is used
* Whether ACK or Block Ack is expected
* Which frame is selected
* Which retry counter applies
* Which rate is selected
* Whether aggregation occurs
* Whether fragmentation occurs
* Whether reception is attempted
* Whether reception is successful
* Why a frame is dropped

Do not infer these decisions only from NED parameter names.

---

