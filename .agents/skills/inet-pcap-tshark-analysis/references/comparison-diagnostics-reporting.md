## Contents

* Export a filtered capture
* Assert that a packet exists
* Compare sender and receiver captures
* Account for duplicate observations
* Diagnose an empty capture
* Diagnose undecoded packets
* Recommended investigation workflow
* Reporting requirements

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
