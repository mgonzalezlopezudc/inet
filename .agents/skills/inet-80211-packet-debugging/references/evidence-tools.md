# IEEE 802.11 evidence tools

Use the generic capture, log, event-log, result, and LLDB skills for their workflows. This reference contains only Wi-Fi-specific additions.

## Capture representation and limits

Capture at the sender, receiver, AP, or wired AP interface needed to prove the disputed transition. For native MAC analysis, configure `PcapRecorder.dumpProtocols` for the installed IEEE 802.11 MAC representation and keep files separate by observation point.

Confirm whether the capture contains native 802.11, Ethernet-converted frames, radiotap-like metadata, bad frames, and transmit or receive observations. A MAC capture usually cannot prove:

* Energy below detection or failed preamble synchronization.
* Complete interference or SNIR history.
* Radio state, NAV, or frozen backoff state.
* Every unsuccessful PHY reception attempt.

Use PHY logs, vectors, event logs, or LLDB for those questions.

## Discover and export Wi-Fi fields

Field availability depends on the capture encapsulation and TShark dissector. Confirm unfamiliar names:

```sh
tshark -G fields |
  awk -F '\t' '$3 ~ /^(wlan|wlan_radio|radiotap)\./ {print $3}' |
  sort -u
```

Build a timeline from supported fields:

```sh
tshark \
  -n \
  -r "$PCAP" \
  -Y 'wlan' \
  -T fields \
  -E header=y \
  -E separator=$'\t' \
  -E occurrence=a \
  -e frame.number \
  -e frame.time_epoch \
  -e wlan.fc.type_subtype \
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

Use subtype filters for management, control, or data frames only after confirming the installed dissector's values. Group retries by TA, RA, TID, sequence, and fragment—not capture frame number.

Extract radiotap or other PHY metadata only when the encapsulation contains it. An absent signal, noise, rate, MCS, HE, or EHT field is unavailable evidence, not zero.

## Correlate evidence

Use `frame.time_epoch` for simulation-time correlation. Build a compact timeline whose rows identify their source:

```text
t=... [PCAP]    DATA seq=... retry=0 transmitted by ...
t=... [CMdenv] receiver rejects reception because ...
t=... [VECTOR] SNIR or received-power value ...
t=... [EVENT]  ACK timeout schedules retry ...
t=... [LLDB]   recovery state before counter update ...
```

Distinguish direct fields, simulator decisions, TShark heuristics, source-code interpretation, and inference.

## Target source inspection

Search the checked-out implementation rather than assuming module or policy names:

```sh
rg -n 'Dcf|Edca|Hcf|Txop|Rts|Cts|Ack|BlockAck|RecoveryProcedure' \
  src/inet/linklayer/ieee80211

rg -n 'Ieee80211.*Radio|Receiver|Transmitter|ErrorModel|SNIR|Sensitivity|energyDetection' \
  src/inet/physicallayer

rg -n 'rtsThreshold|retryLimit|fragmentation|aggregation|qosStation|txopLimit|cwMin|cwMax|aifs|sifs|slotTime' \
  src/inet/linklayer/ieee80211

rg -n 'Ieee80211.*Header|SequenceControl|BlockAck|Addba|QosControl|Duration' \
  src/inet
```

Locate the code that decides whether RTS or an ACK is required, which retry counter and mode apply, whether aggregation or fragmentation occurs, whether reception is attempted/succeeds, and why a frame is dropped. NED parameter names alone do not prove runtime policy.
