## Contents

* Compare observation points
* Account for duplicate observations
* Diagnose empty or undecoded captures
* Build automated assertions

## Compare observation points

Capture both endpoints when investigating delivery, loss, or retransmission. Use separate files for each node and interface.

Do not match packets by `frame.number`; each capture has independent numbering. Correlate with the strongest available identity:

* Addresses, ports, and protocol.
* IP fragmentation fields or transport sequence numbers.
* ICMP or application identifiers.
* Payload length or contents.
* Simulation timestamp.

A packet present at the sender but absent at the receiver proves only that it was not observed at the receiver's selected capture point. Add an intermediate capture, targeted log, result counter, or event log to locate the first missing transition.

## Account for duplicate observations

One logical packet may appear several times because recorders observe transmission and reception signals, multiple interfaces, encapsulation boundaries, forwarding, multicast copies, or retransmissions. Distinguish observations using the capture point, direction metadata, headers, sequence identifiers, length, and simulation time.

## Diagnose empty or undecoded captures

For an empty capture, verify task artifacts and configuration rather than tool installation:

1. Confirm the file exists and is nonempty.
2. Confirm the node path is instantiated and supports `numPcapRecorders`.
3. Confirm `moduleNamePatterns` matches a relative interface path crossed by packets.
4. Check `dumpProtocols`, `packetFilter`, the simulation interval, and link-type conversion.
5. Run once with recorder verbosity and targeted recorder logging.

For undecoded frames, inspect the encapsulation, protocol hierarchy, one full frame, the selected dump protocol, and whether a dissector exists. An empty decoded field is not evidence for a header value.

## Build automated assertions

Use a decoded field export and test its match count. Keep the display filter and capture point in the failure message. A zero count means only that no matching decoded frame exists in that capture.

When writing a filtered capture, preserve the original and validate the derived file before using it as evidence:

```sh
tshark -n -r "$PCAP" -Y "$FILTER" -w "$FILTERED"
test -s "$FILTERED"
capinfos "$FILTERED"
```

Report capture points and files, recorder options that affect interpretation, filters, matching frames and `frame.time_epoch` values, packet identifiers, observation-point differences, decoding limitations, and whether each conclusion is direct packet evidence, another evidence source, or a TShark heuristic.

## Walkthrough Tables for IEEE 802.11ax Examples

When asked to generate, update, or analyze 802.11 frame type statistics, packet sizes, or estimated airtime percentages inside any of the `walkthrough.md` files under `examples/ieee80211ax/`, run the project's pre-existing automated Python analysis script:

```sh
python3 examples/ieee80211ax/analysis/analyze_pcap_types.py
```

This script automatically:
- Identifies the runnable configurations mentioned in each example's `walkthrough.md`.
- Generates any missing PCAPs by executing short Cmdenv simulations with PCAP overrides.
- Parses the captures, aggregates frame type distribution, calculates mean and standard deviation of sizes, and estimates airtime percentages.
- For asymmetric traffic cases (`BacklogBased` and `HoLMinDelay` in `dl_ofdma`), separates traffic flows into individual tables for `host[0]`, `host[1]`, and `host[2]`.
- Overwrites the `## 802.11 Packet Type Statistics` sections in all `walkthrough.md` files with the correct tables.

