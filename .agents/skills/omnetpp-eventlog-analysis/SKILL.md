---
name: omnetpp-eventlog-analysis
description: Reconstruct OMNeT++ simulator-level message and event causality from event logs. Use when Codex needs to trace scheduling, sending, delivery, cancellation, timer behavior, self-messages, or event ordering that Cmdenv logs and packet captures do not explain.
---

# Analyzing OMNeT++ event logs

A packet capture shows protocol-visible packets at observation points. A Cmdenv log shows selected module logging. An event log shows simulator events and message movements. Use the event log when timer or message causality is the missing evidence.

## General rules

* Enable event logs only for a narrow run, time interval, or diagnostic reproduction.
* Prefer command-line overrides; do not edit `omnetpp.ini` solely for temporary event logging.
* Run one configuration and one run number at a time.
* Do not record a full long campaign unless the defect cannot be reproduced narrowly.

## Create an event log

Add these overrides to the normal simulation command:

```sh
--record-eventlog=true
--eventlog-file="logs/${CONFIG}-${RUN}.elog"
```

If the problem occurs in a known time window, also restrict the run with a suitable simulation-time limit or start from a smaller reproducer. Do not add a limit that prevents the failure.

## Inspect the event log

Useful investigation targets include:

* Event number and simulation time.
* Module path handling the event.
* Message name, kind, class, tree id, encapsulation id, or pointer-like id shown by the tool.
* Self-message scheduling, rescheduling, cancellation, and delivery.
* Direct sends, delayed sends, gate sends, and arrivals.
* Packet ownership, deletion, duplication, and encapsulation changes.

Use text search only after understanding the event-log format produced by the installed OMNeT++ version:

```sh
rg -n -i 'packetName|timer|timeout|send|schedule|cancel|delete|drop' logs/*.elog
```

## Causality workflow

1. Reproduce the issue with the event log enabled.
2. Identify the observed wrong event, missing event, timeout, or packet transition.
3. Locate the event number and simulation time.
4. Trace backward to the event that scheduled or sent the message.
5. Trace forward to delivery, cancellation, deletion, timeout, or drop.
6. Correlate with Cmdenv logs using event numbers and simulation times.
7. Correlate with PCAPng/TShark using packet timestamps when protocol-visible packets matter.
8. Use LLDB only after identifying the source path or module state that needs inspection.

Report the event-log path and options, relevant events and message identifiers, the causal chain, correlated evidence, and which conclusions are direct evidence or inference.
