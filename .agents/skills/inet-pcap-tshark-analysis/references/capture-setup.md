## Contents

* General rules
* Prerequisites
* Define the common simulation command
* Determine the capture point
* Fast packet capture without detailed Cmdenv logging
* Capture packets and Cmdenv context together
* Print recorder packet descriptions in Cmdenv
* Choose the recorded protocol representation
* Checksums and frame check sequences
* Restrict packet recording inside INET
* Limit interfaces
* Limit captured size
* Filter packet classes or fields
* Record multiple interfaces separately

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

