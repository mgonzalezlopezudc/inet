---
name: inet-80211-packet-debugging
description: Debug IEEE 802.11 PHY and MAC packet exchanges in OMNeT++/INET without Qtenv. Use for Wi-Fi packet generation, channel access, transmission, reception, ACK/RTS/CTS/Block Ack, aggregation, retransmission, association, roaming, AP forwarding, PHY interference, rate-control, or packet-drop investigations that need Cmdenv, PCAPng, TShark, selective logs, event logs, opp_scavetool, source inspection, or LLDB.
---

# Debugging IEEE 802.11 packet exchanges

Use this skill when an INET simulation has an IEEE 802.11 packet-exchange problem and the investigation needs reproducible command-line evidence rather than Qtenv-only inspection.

## Core rule

Find the first layer where the observed exchange diverges from the expected exchange:

```text
application/network packet
802.11 management or data service
MAC queue and QoS classification
DCF, EDCA, or other coordination function
frame exchange: DATA, RTS, CTS, ACK, Block Ack, management
PHY transmission construction
radio medium, propagation, attenuation, noise, interference
receiver detection, synchronization, decoding, error decision
recipient MAC processing
upper-layer delivery
```

Do not jump from "the sender generated a packet" to "the receiver lost it." Prove the first missing or incorrect transition with logs, captures, event logs, results, source inspection, or LLDB.

## Reuse other skills first

Use these skills instead of duplicating their generic procedures:

* `inet-simulation-run`: validate the run command, NED path, INET library, Cmdenv/Qtenv selection, and release/debug mode.
* `inet-pcap-tshark-analysis`: configure generic PcapRecorder captures and analyze non-802.11-specific TShark fields.
* `omnetpp-result-analysis`: query and export `.sca` and `.vec` files.
* `inet-lldb-debugging`: launch debug builds under LLDB and inspect runtime errors, breakpoints, watchpoints, and local variables.
* `ieee80211-standards`: check normative 802.11 behavior before reading PDFs directly.

If `inet-cmdenv-log-analysis` or `omnetpp-eventlog-analysis` are not available in this checkout, use targeted `rg` over saved Cmdenv logs and OMNeT++ event-log command-line options directly.

## Evidence ladder

Prefer the cheapest evidence that can answer the question:

1. Inspect the actual NED types and INI overrides for the instantiated station, AP, MAC, management, radio, and radio medium.
2. Run one configuration and one run number in Cmdenv with command-line diagnostic overrides.
3. Capture at sender, receiver, AP, and relevant intermediate interfaces when delivery, loss, or retransmission is disputed.
4. Decode frame type, address fields, retry bit, sequence/fragment numbers, TID, timing, and PHY metadata when available.
5. Add selective Cmdenv logs for the suspected 802.11 component.
6. Query scalars and vectors for drops, retries, receptions, SNIR, queue state, radio state, and channel access.
7. Use a narrow event log when timer scheduling or message causality is unclear.
8. Inspect the installed INET source for the exact policy or state-machine decision.
9. Use LLDB only after identifying a suspicious module, packet, event, or source path.

Use command-line overrides for temporary logging, packet capture, event logs, and result inspection. Create a dedicated debug configuration only when repeated or complex investigation would otherwise become error-prone, and keep it separate from the normal experiment configuration.

## Required starting facts

Discover or record only the facts needed for the current problem:

* OMNeT++ version, INET commit, working directory, INI file, configuration, run number, and seed-set.
* Sender, receiver, AP, radio-medium, and wireless-interface module paths.
* MAC mode, management type, radio type, radio-medium type, mode set, frequency/channel/bandwidth, transmit power, sensitivity, energy-detection threshold, noise, propagation, path-loss, fading, obstacle, and antenna models when PHY behavior matters.
* SSID, BSSID, STA/AP MAC addresses, To DS/From DS direction, TID/access category, RTS threshold, fragmentation threshold, retry limits, aggregation, ACK, and Block Ack policies when MAC behavior matters.
* Packet name, protocol tuple, sequence number, simulation-time interval, or event number that identifies the exchange under investigation.

Treat the checked-out INET source as authoritative for implementation behavior. Treat the applicable IEEE standard as authoritative for normative protocol behavior. Never assume that a standard feature is implemented or enabled.

## Read references on demand

Load only the reference files needed for the current question:

* [overview.md](references/overview.md): original purpose, layered mental model, and scope notes.
* [scope-and-reproducibility.md](references/scope-and-reproducibility.md): required inputs, version checks, instantiated-interface inspection, feature-support matrix, and reproducibility practices.
* [frame-model.md](references/frame-model.md): management/control/data frames, 802.11 address fields, sequence/fragment numbers, Duration/NAV, FCS, and bit-error distinctions.
* [phy-carrier-sense-and-timing.md](references/phy-carrier-sense-and-timing.md): radio/medium compatibility, half-duplex state, frequency, power, antenna, propagation, thresholds, noise, interference, capture effect, preamble/header/payload, carrier sensing, and timing.
* [mac-retry-aggregation-and-rate-control.md](references/mac-retry-aggregation-and-rate-control.md): DCF, RTS/CTS, EDCA/QoS/HCF, ACK policies, retransmission, fragmentation, A-MSDU, A-MPDU, Block Ack, and rate selection/control.
* [management-forwarding-and-feature-gates.md](references/management-forwarding-and-feature-gates.md): scanning, authentication, association, handover, AP forwarding, broadcast/multicast, power save, and HT/VHT/HE/EHT feature checks.
* [evidence-tools.md](references/evidence-tools.md): 802.11-specific PcapRecorder setup, TShark fields, selective Cmdenv logging, event logs, opp_scavetool, and source-inspection searches.
* [lldb-80211-breakpoints.md](references/lldb-80211-breakpoints.md): 802.11-specific breakpoint targets and LLDB workflow additions.
* [scenario-playbooks.md](references/scenario-playbooks.md): playbooks for no beacon, no probe response, association failure, queued-but-not-transmitted data, no ACK, no CTS, excessive retransmissions, QoS delay, Block Ack stalls, AP forwarding failures, roaming outage, interference surprises, capture-effect limitations, and broadcast issues.
* [reporting-verification-and-safety.md](references/reporting-verification-and-safety.md): causal timeline format, diagnostic invariants, fix categories, verification checklist, final-report template, preferred execution sequence, and safety rules.

For reference files over 100 lines, use their headings or `rg -n '^#|keyword'` before reading large ranges.

## Common pitfalls

Do not:

* Infer PHY reception solely from a MAC capture.
* Infer collision solely from a missing ACK.
* Infer data loss when ACK loss could explain a retry.
* Infer hidden nodes without carrier-sense and interference evidence.
* Compare Wi-Fi captures by capture frame number alone.
* Assume Address 1 is the final destination.
* Assume nominal bitrate determines airtime.
* Enable unrestricted trace logs or event logs by default.
* Replace a realistic radio with an ideal radio as the final fix.
* Increase power, sensitivity, thresholds, timeouts, retry limits, or queue sizes without causal evidence.

Always distinguish direct packet evidence, Cmdenv evidence, event-log evidence, result evidence, source-code evidence, debugger evidence, and inference.
