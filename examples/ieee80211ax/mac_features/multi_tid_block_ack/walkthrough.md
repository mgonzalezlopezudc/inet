# Walkthrough: HE Multi-TID Block Ack

This example exercises downlink (DL) and uplink (UL) Block Ack arrangements
involving multiple traffic identifiers (TIDs). Retained run-0 results establish
the offered application flows and protocol-visible BAR/BA exchanges, but the
available decode does not prove that a single acknowledgment contains multiple
TID records. The feature verdict is therefore deliberately `INCONCLUSIVE`.

## Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain why an aggregate containing several QoS TIDs needs acknowledgment
  state for each TID;
- distinguish a Multi-TID BAR/Block Ack from a Multi-STA Block Ack;
- locate the split DL and UL entry points and inherited configurations; and
- reproduce the first-line result and packet queries without treating traffic
  delivery as proof of the acknowledgment variant.

A TID identifies a QoS traffic stream. IEEE 802.11 High Efficiency (HE)
multi-TID aggregation can place MAC protocol data units (MPDUs) from more than
one TID in an aggregate. The acknowledgment context must then identify the
relevant per-TID sequence state. In the UL multi-user case, one Multi-STA Block
Ack may instead carry per-station AID/TID information after Trigger-based
transmissions.

## Scenario description

There is no local `omnetpp.ini`. [downlink.ini](downlink.ini) includes
[`../../dl_ofdma/omnetpp.ini`](../../dl_ofdma/omnetpp.ini), while
[uplink.ini](uplink.ini) includes
[`../../ul_ofdma/omnetpp.ini`](../../ul_ofdma/omnetpp.ini).

```text
DL: server -- Ethernet -- AP ))) host[0], host[1]
UL: host[0..1] ))) AP -- Ethernet -- server
```

All nodes are stationary. Both inherited networks use 5 GHz/20 MHz HE radios.
The DL run lasts 1 s with a 0.25 s warm-up; the UL runs last 2 s. DL sends
ports 5000 and 5001 (TIDs 6 and 7) to two stations. UL-SU sends two TIDs from
one station; UL-MU sends one TID from each of two stations and disables
`host[2]` traffic.

## Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.6.3 defines multi-TID A-MPDU operation; Clause
10.25.5 selects BlockAck/BlockAckReq variants; Clause 9.3.1.8.6 defines the
Multi-STA BlockAck variant. The corpus evidence used here is
`80211ax-2024:chunk:09842`, `:05346`, and `:01597`.

INET configuration advertises HE multi-TID transmit/receive capability at AP
and stations. The checked-out model contains Multi-TID BAR/BA record
structures, but configuration and source availability are not runtime proof.
The retained native MAC capture exposes QoS TIDs and generic BAR/BA labels but
does not authoritatively expose all required per-TID records.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Split entry points resolve to the intended networks/configurations | `PASS` | include chains and `.sca` metadata | run 0/seed 0 | Configuration provenance only |
| Two DL/UL-SU application TIDs are active | `PASS` | application vectors and decoded `wlan.qos.tid` | run 0/seed 0 | TIDs 6 and 7 are observed |
| BAR/BA or Trigger/BA exchanges occur | `PASS` | retained AP PCAPs | run 0/seed 0 | Observation counts, not variant proof |
| One acknowledgment covers multiple TIDs | `INCONCLUSIVE` | required BA variant and per-TID records are absent from the retained decode | run 0/seed 0 | Central feature invariant unresolved |
| Single-TID negative control | `NOT RUN` | `SingleTidBlockAckComparison` exists in the included DL INI | none | No retained matched artifacts |
| Shared AX analyzer can regenerate this scenario | `FAIL` | `ax.json` maps this scenario to nonexistent `omnetpp.ini` | none | Tooling descriptor is stale |

## Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `MultiTidBlockAck` via `downlink.ini` | DL treatment | capability on; sequential BAR | two TIDs to each of two STAs, 20 MHz | 0/0 | BAR/BA carries both TID contexts |
| `UlSuMultiTidBlockAck` via `uplink.ini` | UL-SU treatment | capability on; UL MU disabled | two TIDs from one STA, 20 MHz | 0/0 | one response covers both TIDs |
| `UlMuMultiTidBlockAck` via `uplink.ini` | UL-MU treatment | capability on; scheduled UL MU | one TID from each of two STAs | 0/0 | one Multi-STA BA covers both AID/TID contexts |
| `SingleTidBlockAckComparison` via `downlink.ini` | Negative control | multi-TID capability off | matched DL traffic | `NOT RUN` | separate single-TID acknowledgment contexts |

The assignments in the included DL/UL INIs are the winning feature settings;
the one-line wrappers add no later overrides.

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Both QoS TIDs enter the exchange | AP/STA PCAP `wlan.qos.tid` | only one TID | classifier/queues | inspect `ExampleQosClassifier` and per-AC queues |
| BAR/BA variant contains multiple TID records | BAR/BA control plus record array | generic BA only or one record | frame construction/serialization | targeted `HeDlMuTxOpFs` / `RecipientBlockAckProcedure` log or model signal |
| UL-MU response covers scheduled stations | Trigger AIDs correlated with Multi-STA BA AID/TID entries | missing station/TID | UL scheduler/BA builder | co-record Trigger fields, BA fields, and HCF log |
| Application outcome remains intact | sender/receiver application vectors | unexpected receive deficit | MAC retry/queue/upper path | correlate QoS sequence/retry fields and result vectors |

## Reproduction

Run from the INET repository root. These illustrative commands were not
executed during this documentation revision, so their exit status is
`NOT RUN`:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/mac_features/multi_tid_block_ack/downlink.ini \
  -c MultiTidBlockAck -r 0 --seed-set=0 \
  --result-dir=examples/ieee80211ax/mac_features/multi_tid_block_ack/results/validation/dl

bin/inet -u Cmdenv -f examples/ieee80211ax/mac_features/multi_tid_block_ack/uplink.ini \
  -c UlMuMultiTidBlockAck -r 0 --seed-set=0 \
  --result-dir=examples/ieee80211ax/mac_features/multi_tid_block_ack/results/validation/ul-mu
```

The retained packet-statistics logs end normally, but their original full
commands and process exit codes were not retained; do not reinterpret that as
a newly observed exit status.

The shared command is currently `NOT RUN` and expected to fail before launch:

```sh
python3 examples/ieee80211/analysis/analyze_pcap.py \
  --suite examples/ieee80211/analysis/suites/ax.json \
  --generate --subdir mac_features/multi_tid_block_ack --run 0
```

`ax.json` must first represent both real INI entry points instead of
`mac_features/multi_tid_block_ack/omnetpp.ini`.

## Scalar and vector analysis

Inputs are under `results/scalar-vector/20260725T120411Z/`.

```sh
opp_scavetool query -l \
  -f 'type =~ vector and module =~ "*.app[*]" and (name =~ "packetSent:vector(packetBytes)" or name =~ "packetReceived:vector(packetBytes)")' \
  examples/ieee80211ax/mac_features/multi_tid_block_ack/results/scalar-vector/20260725T120411Z/UlMuMultiTidBlockAck/UlMuMultiTidBlockAck-\#0.vec
```

| Configuration | Per-run application observation | Interpretation |
|---|---|---|
| `MultiTidBlockAck` | sends 141/71/141/71; receives 134/68 and 133/68 | two payload sizes/flows reach both DL receivers |
| `UlSuMultiTidBlockAck` | sends 341/171; receives 340/170 | two UL flows reach the server |
| `UlMuMultiTidBlockAck` | each active STA sends 341; server receives 340+340 | both scheduled sources deliver traffic |

Counts are single-run samples over each full recorded run; no uncertainty is
claimed. They are outcome evidence only: no scalar/vector result classifies the
Block Ack variant or per-TID acknowledgment records.

## PCAP statistics

Capture point: AP `wlan[0]`; PCAPng with radiotap, microsecond precision;
computed checksum/FCS settings are recorded by the session. Decode used TShark
4.6.4. Rows count capture observations, not de-duplicated transmissions.

| Configuration | Observations | Relevant frame summary | Interpretation limit |
|---|---:|---|---|
| `MultiTidBlockAck` | 1,224 | 401 QoS Data, 401 BAR, 401 BA | generic labels do not prove multi-TID records |
| `UlSuMultiTidBlockAck` | 921 | 510 QoS Data, 33 BAR, 33 BA, 343 Ack | no per-record decode shown |
| `UlMuMultiTidBlockAck` | 1,268 | 1,176 QoS Data, 45 QoS Null, 16 Trigger, 15 BA | no authoritative AID/TID BA table |

## Frame exchange analysis

```sh
tshark -n \
  -r 'examples/ieee80211ax/mac_features/multi_tid_block_ack/results/packet-statistics/20260724T175025Z/MultiTidBlockAck/MultiTidBlockAck-#0Lan80211AxDlOfdma.ap.wlan[0].pcap' \
  -Y 'wlan.fc.type_subtype == 0x28 || wlan.fc.type_subtype == 0x18 || wlan.fc.type_subtype == 0x19' \
  -T fields -E header=y -E separator='|' -E occurrence=a \
  -e frame.number -e frame.time_epoch -e wlan.sa -e wlan.da \
  -e wlan.fc.type_subtype -e wlan.qos.tid -e wlan.ba.control.ba_type \
  -e wlan.ba.multi_sta.aid11 -e wlan.ba.multi_sta.tid -e _ws.col.Info
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.300644 s | AP → host 0 | QoS Data | TID 6 | first voice-flow observation |
| 7 | 0.301158 s | AP → host 0 | QoS Data | TID 7 | second TID is directly observed |
| later | run 0 | AP ↔ station | BAR/BA | variant records undecoded | acknowledgment exchange is present but central invariant remains unresolved |

TShark frame numbers are capture indices, not OMNeT++ event numbers. A
co-recorded model signal or richer serializer decode is required to correlate
the TID data with the exact BAR/BA record array.

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| Multi-TID capability was requested | `PASS` | all peers enable TX/RX capability | none retained | capability negotiation not decoded here | n/a |
| Multiple TIDs and BA exchanges occur | `PASS` | two application classes | none retained | TIDs 6/7 and BAR/BA observations | sends/receives recorded |
| The exchange uses a multi-TID acknowledgment context | `INCONCLUSIVE` | requested only | absent | decisive BA records not exposed | application delivery is not mechanism proof |

The bounded verdict is `INCONCLUSIVE`: adjacent configuration, packet, and
outcome evidence exists, but no retained direct observation proves the
multi-TID acknowledgment content.

## Limitations and inconclusive claims

- No matched retained negative-control run exists.
- Separate scalar/vector and packet sessions cannot establish event-level
  causality or exact count agreement.
- PHY width, guard interval, NSS, coding, and aggregate membership are not
  claimed where the typed capture profile does not expose them.
- The smallest resolving run co-records BAR/BA record arrays, QoS TIDs, HCF
  decisions, and application vectors for treatment and
  `SingleTidBlockAckComparison` with seed 0.

## Further experiments

- Run the DL negative control with identical seed/load and compare the number
  and record count of BAR/BA contexts.
- Vary only the second TID's offered load; the multi-TID record should appear
  only when both queues contribute to the acknowledged aggregate.
- Repeat several seeds after the deterministic record invariant is observable;
  aggregate application outcomes per run before estimating variability.

## Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | BA per-TID records and shared-suite entry-point selection are not observable/valid |
| Intended behavior | expose the Clause 26.6.3/10.25.5 acknowledgment context without inferring it |
| Smallest change surface | AX suite descriptor plus block-ack feature plugin; serializer fields only if the capture format can carry them authoritatively |
| Observability | record BA variant, record count, AID/TID, starting sequence, and correlated frame time |
| Validation | DL treatment/control and UL-SU/UL-MU seed-0 checks, then bounded seed coverage |
| Compatibility and risks | preserve single-TID and existing split INI entry points; do not fill absent fields from configuration |
| Architecture and sealing | apply `inet-architectural-requirements` before any `src/inet` change; no source change is authorized here |
| Next handoff | analysis-suite owner first; Wi-Fi MAC owner only if serializer observability is insufficient |

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/scalar-vector/20260725T120411Z/` | three configs, run 0/seed 0 | `opp_scavetool` application filters; full run | `.sca` binds each real split INI |
| PCAP/log | `results/packet-statistics/20260724T175025Z/` | three configs, run 0 | TShark 4.6.4; AP `wlan[0]` | separate session from scalar/vector evidence |
| Standards | corpus `80211ax-2024` | IEEE Std 802.11-2024 | clauses/chunks named above | PDF not needed |
