---
name: inet-cmdenv-log-analysis
description: Analyze INET and OMNeT++ Cmdenv logs. Use when Codex needs to find module behavior, packet-processing decisions, drops, errors, warnings, event numbers, simulation times, or targeted log context in saved Cmdenv output.
---

# Analyzing Cmdenv logs

## General rules

* Prefer saved logs over scrolling terminal output.
* Use command-line logging overrides for temporary diagnostics; do not edit `omnetpp.ini` solely to enable logs.
* Keep logging narrow: target the relevant module subtree and level instead of enabling global `debug` or `trace`.
* Do not infer delivery, loss, retransmission, or causality from one log line without supporting context.

## Capture useful Cmdenv context

Add these options to the normal Cmdenv command:

```sh
--cmdenv-express-mode=false
--cmdenv-event-banners=false
'--cmdenv-log-prefix=[%l] event=%e time=%t module=%M: '
```

Add targeted module logging only after identifying the relevant module path:

```sh
'--**.cmdenv-log-level=off'
'--*.host[0].wlan[0].mac.**.cmdenv-log-level=debug'
```

Adapt paths to the instantiated model. Do not assume `host[0]`, `wlan[0]`, or `mac` exists.

## Search workflow

Start broad, then narrow:

```sh
rg -n -i 'error|warning|drop|fail|exception|runtime error' "$LOG"
rg -n -i -C 10 'packetName|sequence|address|retry|timeout|backoff|queue|transmit|receive' "$LOG"
rg -n -C 20 'event=12345|time=1\.234' "$LOG"
```

When correlating with packet captures, search around `frame.time_epoch` and inspect nearby events. The decision that caused a packet may appear before the capture timestamp.

Use event numbers from Cmdenv logs for simulator ordering. Do not confuse them with TShark frame numbers.

## Failure analysis

For runtime errors, initialization failures, or aborts:

1. Preserve the first error and the surrounding context.
2. Search earlier for the first warning or invalid state involving the same module, packet, or parameter.
3. Identify whether the failure occurs during initialization or event processing.
4. Switch to `inet-lldb-debugging` when source-level state is required.

For packet behavior:

1. Identify the packet or flow by name, address tuple, sequence number, or event interval.
2. Find enqueue, dequeue, transmission, reception, drop, timeout, retry, and state-transition lines.
3. Build a minimal timeline with simulation times and event numbers.
4. Confirm packet-visible facts with PCAPng/TShark when protocol headers matter.
5. Confirm aggregate behavior with scalars/vectors when counters matter.

Report the log path and targeted overrides, relevant event numbers, simulation times, module paths and packet identities, and which conclusions are direct log evidence or inference.
