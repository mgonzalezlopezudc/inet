---
name: inet-pcap-tshark-analysis
description: Record and analyze packet exchanges in INET simulations using PcapRecorder, Cmdenv, TShark, and capinfos. Use when asked to find packets, inspect protocol headers, analyze TCP streams or retransmissions, compare captures from different nodes or interfaces, verify whether an exchange occurred, or correlate network packets with Cmdenv simulation logs.
---

# Analyzing INET packet exchanges

Use this skill for protocol-visible packets exchanged by an INET simulation. A capture proves what was observed at selected capture points; it does not by itself explain routing, queueing, MAC, PHY, or drop decisions.

## Use adjacent skills

* `inet-simulation-run`: establish the base Cmdenv command, NED path, image path, and model libraries.
* `inet-cmdenv-log-analysis`: inspect internal module decisions and log context.
* `omnetpp-eventlog-analysis`: reconstruct simulator-level scheduling and message causality.
* `omnetpp-result-analysis`: inspect counters, drops, vectors, and aggregate statistics.
* `inet-lldb-debugging`: inspect source-level state after logs/captures identify a suspicious code path.
* `inet-80211-packet-debugging`: handle Wi-Fi-specific frame fields, carrier sensing, ACK/RTS/CTS, aggregation, and PHY evidence.

## Core rules

* Use PCAPng unless compatibility requires legacy PCAP.
* Configure temporary packet recording with command-line overrides; do not edit `omnetpp.ini` solely for diagnostics.
* Capture only the needed nodes, interfaces, protocols, and time interval.
* Use unique filenames that encode configuration, run, node, interface, and recorder.
* Use `tshark -n` for reproducible automated analysis.
* Use TShark display filters with `-Y` when reading an existing capture; do not use live-capture `-f` filters for offline filtering.
* Preserve original captures when creating filtered or converted artifacts.
* Do not confuse TShark `frame.number` with OMNeT++ event number.
* Do not claim delivery from a sender-side capture.
* Do not claim loss or drop from absence in a single capture.

## Minimal workflow

1. Start from a known-good `opp_run -u Cmdenv` command.
2. Inspect the NED and INI configuration to find the real node path, interface module, and whether `numPcapRecorders` is supported.
3. Add the narrowest useful PcapRecorder command-line overrides.
4. Run one configuration and one run number.
5. Verify the capture exists, is nonempty, and decodes with `tshark -n -r "$PCAP" -c 10`.
6. Use `-T fields` exports for timelines and exact header values.
7. Correlate packet `frame.time_epoch` with Cmdenv `%t` and event numbers from logs.
8. Capture both sender and receiver when investigating delivery, loss, or retransmission.
9. Report whether conclusions are direct packet evidence, TShark heuristics, log evidence, result evidence, or inference.

## INET-specific reminders

`moduleNamePatterns` is relative to the node containing the recorder. Use values such as `eth[0]`, `wlan[0]`, or `ppp[0]`, not full network paths such as `Network.host[0].eth[0]`.

`dumpProtocols` selects which protocol representation is written. `moduleNamePatterns` selects where packet signals are observed. Do not conflate them.

`frame.time_epoch` represents the recorded simulation timestamp. `frame.time_relative` is relative to the capture and is usually wrong for direct Cmdenv correlation.

A successful simulation does not guarantee that a configured recorder captured packets. Always validate the output file before reasoning from it.

## Read references on demand

Load only the reference needed for the current problem:

* [capture-setup.md](references/capture-setup.md): prerequisites, common simulation command, capture-point selection, PcapRecorder overrides, protocol representation, checksum/FCS settings, capture limits, multiple recorders, and validation.
* [tshark-inspection.md](references/tshark-inspection.md): basic inspection, display filters, timelines, Cmdenv correlation, time filters, TCP analysis, endpoints, conversations, and protocol hierarchy.
* [comparison-diagnostics-reporting.md](references/comparison-diagnostics-reporting.md): filtered captures, automated packet assertions, sender/receiver comparison, duplicate observations, empty or undecoded captures, investigation workflows, and reporting requirements.

Use `rg -n '^##|keyword'` in reference files before reading large ranges.

## Report essentials

Include exact simulation command, working directory, INI file, configuration, run number, node paths, interface/module patterns, PCAP filenames, Cmdenv log filename if used, exit status, TShark version, exact TShark commands and filters, relevant frame numbers, simulation timestamps, event numbers when known, and remaining ambiguity caused by capture position or decoding limits.
