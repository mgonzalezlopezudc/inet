# HE rate-adaptation walkthrough

This walkthrough teaches how INET's HE Minstrel controller selects modulation
and coding scheme (MCS) and spatial streams per peer. Retained five-run
telemetry directly demonstrates changing selections and outcomes for a mobile
edge STA; it does not retain a matched five-run fixed-rate control.

## Learning objectives and feature primer

After completing this walkthrough, the reader can:

- distinguish controller choice, predicted probability, transmission outcome,
  MAC retry, and application delivery;
- align HE rate-control vectors by module and timestamp;
- verify known MCS values in radiotap without substituting INI defaults; and
- reproduce the mobile treatment and diagnose a frozen or invalid selector.

Rate adaptation chooses a PHY rate from recent success history. A higher MCS
can carry more bits per symbol but needs a better channel; the number of
spatial streams (NSS) is another dimension. Minstrel explores alternatives and
updates a success estimate. The validation outcome is a `PASS` that the
retained mobile run selected MCS 0--9 with valid outcome telemetry, and
`INCONCLUSIVE` for any comparative performance advantage.

## Scenario description

[HeRateAdaptationNetwork.ned](HeRateAdaptationNetwork.ned) uses one wired UDP
server, one stationary AP, and four STAs. [omnetpp.ini](omnetpp.ini) sends
saturated downlink traffic from 0.3--1.7 s on a 20 MHz 5 GHz channel. Hosts
0--2 are stationary; in `HeMinstrelMobile`, host 3 starts 230 m left of the AP
and moves right at 40 m/s. The AP's backlog-aware downlink scheduler consults
the HE rate-control module.

```text
server --> AP == HE downlink ==> host[0..2] stationary
                              ==> host[3] moving at 40 m/s
```

## Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.15.3 covers HE MCS, NSS, bandwidth, and DCM
selection (`80211ax-2024:chunk:09938`). The standard constrains valid PHY
operation; it does not mandate INET's Minstrel algorithm. `HeMinstrelRateControl`
and its probability are model choices. Controller vectors are authoritative
for internal choice; known radiotap HE bits are authoritative for captured
MCS; application vectors are outcomes. None alone proves that Minstrel
outperforms a fixed rate.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| controller selects multiple HE MCS values | `PASS` | `heRateSelectedMcs:vector` range 0--9 | mobile 0--4 | direct controller telemetry |
| selected NSS remains valid | `PASS` | `heRateSelectedNss:vector` all 1 | mobile 0--4 | single-stream retained scope |
| selected-attempt success is high | `PASS` | `heRateTxSuccess:vector` | mobile 0--4 | 0.998445 ± 0.001980 |
| Minstrel improves delivery | `INCONCLUSIVE` | only mobile five-run campaign | — | no matched five-run control |

## Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `FixedMcs` | control | no rate-control typename; configured 7.3125 Mbps | matched static topology | packet run 0 | captured data stays at fixed MCS |
| `HeMinstrel` | treatment | `HeMinstrelRateControl`, MCS 0--11, lookaround 0.1 | static topology | packet run 0 | controller emits choices |
| `HeMinstrelMobile` | stress treatment | extends Minstrel; host 3 moves 40 m/s | matched downlink load | packet run 0; scalar 0--4 | MCS changes with controller state |
| `HighCollisionRate` | unexecuted stress | adds uplink contention | bidirectional load | `NOT RUN` | failures/retries drive updates |

The packet session offers run-0 controls, while the five-run result campaign
contains only `HeMinstrelMobile`; these sessions cannot support a five-run
algorithm comparison. Seeds must be read from result attributes.

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| selected MCS stays in configured 0--11 | AP controller vector | out-of-range or empty value | HE Minstrel candidate table | query module path and rate-control logs |
| probability and outcome align to attempts | same-module timestamped vectors | missing/misaligned samples | controller instrumentation | export narrow interval as CSV-R |
| captured MCS is known | AP PCAP known bit + value | value present without known bit | recorder / typed HE decoder | inspect `radiotap.he.data_1.data_mcs_known` |
| retry outcome is consistent | controller plus MAC retry/drop vectors | contradictory counts/timestamps | rate control / HCF retry | correlate one attempt in co-recorded run |

## Reproduction

Run from the repository root:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/he_rate_adaptation/omnetpp.ini \
  -c HeMinstrelMobile -r 0 \
  --result-dir=examples/ieee80211ax/he_rate_adaptation/results/manual/HeMinstrelMobile
```

Status: `NOT RUN` during this rewrite. The retained packet run's
`cmdenv.stdout` reached 2 s and `End.`; original process exit status and exact
command were not retained. Suite regeneration (`NOT RUN` here):

```sh
python3 examples/ieee80211/analysis/analyze_pcap.py \
  --suite examples/ieee80211/analysis/suites/ax.json \
  --generate --subdir he_rate_adaptation --run 0
```

## Scalar and vector analysis

Inputs:
`results/scalar-vector/20260725T120411Z/HeMinstrelMobile/*.{sca,vec}`.
Figure provenance:
[rate-adaptation-timeline.png.json](../analysis/figures/rate/rate-adaptation-timeline.png.json).

```sh
opp_scavetool query -l \
  -f 'name =~ "heRateSelectedMcs:vector" OR name =~ "heRateSelectedNss:vector" OR name =~ "heRateSuccessProbability:vector" OR name =~ "heRateTxSuccess:vector" OR name =~ "heRateRetryCount:vector" OR name =~ "packetSentToPeerWithRetry:vector(packetBytes)" OR name =~ "packetDropRetryLimitReached:vector(packetBytes)" OR name =~ "packetReceived:vector(packetBytes)"' \
  examples/ieee80211ax/he_rate_adaptation/results/scalar-vector/20260725T120411Z/HeMinstrelMobile/*.sca \
  examples/ieee80211ax/he_rate_adaptation/results/scalar-vector/20260725T120411Z/HeMinstrelMobile/*.vec
```

| Metric | Source/units | Aggregation | Five-run result | Interpretation |
|---|---|---|---:|---|
| selected MCS | controller vector, index | observed range | 0--9 | mechanism choice |
| selected NSS | controller vector, streams | observed values | 1 only | mechanism choice |
| success probability | controller vector, ratio | observed range | 0.0158203--0.98 | internal estimate |
| attempt success | outcome vector, Boolean | fraction per run, then mean ± 95% t-CI | 0.998445 ± 0.001980 | direct attempt outcome |
| retry count | controller vector, count | observed values | 0 only | not a MAC retry substitute |
| aggregate goodput | app received bytes | per run, then mean ± 95% t-CI | 13.338 ± 1.053 Mbps | end-to-end outcome |

Vector samples are not independent repetitions. Retry diagnosis must include
the AP HCF `packetSentToPeerWithRetry` and retry-limit drop vectors at matched
timestamps; control-frame counts are not a substitute.

## PCAP statistics

Capture point: `HeRateAdaptationNetwork.ap.wlan[0]`

Capture:
`results/packet-statistics/20260724T175025Z/HeMinstrelMobile/HeMinstrelMobile-#0HeRateAdaptationNetwork.ap.wlan[0].pcap`

Scope: AP packet-signal observations in legacy PCAP with simulation
timestamps; TShark 4.6.4; FCS/checksum settings not retained.

```sh
tshark -n -r \
  'examples/ieee80211ax/he_rate_adaptation/results/packet-statistics/20260724T175025Z/HeMinstrelMobile/HeMinstrelMobile-#0HeRateAdaptationNetwork.ap.wlan[0].pcap' \
  -Y 'wlan.fc.type_subtype == 0x0028' \
  -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.ta -e wlan.ra \
  -e radiotap.he.data_1.data_mcs_known \
  -e radiotap.he.data_3.data_mcs -e wlan.fc.retry
```

The preserved generated block is exhaustive population evidence; its
controller check remains correctly `INCONCLUSIVE`.

<!-- REWRITE-PREFIX-END -->


<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
## 802.11 Packet Type Statistics
![802.11 Packet Type Statistics](packet_statistics.png)

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260724T175025Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %**: The sum of this frame type's estimated airtimes divided by the simulation time limit. Concurrent transmissions from multiple capture points are counted separately, so this value can exceed 100%; it is not the union of busy channel time.

### Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | FixedMcs produced protocol-visible wireless observations | 1624 AP/global transmission observations |
| **PASS** | HeMinstrel produced protocol-visible wireless observations | 4460 AP/global transmission observations |
| **PASS** | HeMinstrelMobile produced protocol-visible wireless observations | 4800 AP/global transmission observations |
| **INCONCLUSIVE** | Selected MCS/NSS, EWMA outcome and retries | The packet-type table is exchange evidence only; use the recorded feature vectors/results |

### Configuration: `FixedMcs`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1624**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eb314" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 1435 | 88.36% | 966.0 B | 0.0 B | 1092.8 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 99.63% | 78.41% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 84 | 5.17% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.15% | 0.12% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 84 | 5.17% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -76.2 dBm | - | 0.16% | 0.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.25% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -76.2 dBm | - | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.49% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -76.2 dBm | 13.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 9 | 0.55% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -75.0 dBm | 13.0 dBm | 0.04% | 0.03% |

### Configuration: `HeMinstrel`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4460**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eb314" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 353 | 7.91% | 966.0 B | 0.0 B | 1092.8 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 36.01% | 19.29% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 811 | 18.18% | 54.2 B | 2.6 B | 38.1 us | 0.9 us | 5010 MHz | - | 13.0 dBm | 2.88% | 1.54% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 50 | 1.12% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.13% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 50 | 1.12% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -75.9 dBm | - | 0.14% | 0.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 247 | 5.54% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz, 5015 MHz | -77.5 dBm | - | 2.50% | 1.34% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c3683" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 26-tone RU, GI 1.6 us, LDPC] | 1339 | 30.02% | 32.0 B | 0.0 B | 343.2 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz, 5014 MHz | -77.2 dBm | - | 42.89% | 22.98% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 778 | 17.44% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz, 5017 MHz | -74.0 dBm | - | 13.77% | 7.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.09% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -76.2 dBm | - | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.18% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -76.2 dBm | 13.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 9 | 0.20% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -75.0 dBm | 13.0 dBm | 0.06% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | 811 | 18.18% | 3.0 B | 0.0 B | 21.0 us | 0.0 us | - | 3278.2 dBm | - | 1.59% | 0.85% |

### Configuration: `HeMinstrelMobile`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4800**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#26ba2b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 2 | 0.04% | 966.0 B | 0.0 B | 1074.8 us | 18.0 us | 5010 MHz | - | 13.0 dBm | 0.22% | 0.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eb314" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 177 | 3.69% | 966.0 B | 0.0 B | 1092.8 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 19.53% | 9.67% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 916 | 19.08% | 54.7 B | 3.2 B | 38.2 us | 1.1 us | 5010 MHz | - | 13.0 dBm | 3.54% | 1.75% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 28 | 0.58% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.08% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 28 | 0.58% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -75.2 dBm | - | 0.09% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 293 | 6.10% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz | -77.7 dBm | - | 3.20% | 1.59% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c3683" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 26-tone RU, GI 1.6 us, LDPC] | 1610 | 33.54% | 32.0 B | 0.0 B | 343.2 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz, 5014 MHz, 5016 MHz | -77.3 dBm | - | 55.80% | 27.63% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 810 | 16.88% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz, 5017 MHz | -74.3 dBm | - | 15.51% | 7.68% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -76.2 dBm | - | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.17% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -76.2 dBm | 13.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 8 | 0.17% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -76.2 dBm | 13.0 dBm | 0.06% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | 916 | 19.08% | 3.0 B | 0.0 B | 21.0 us | 0.0 us | - | 3349.7 dBm | - | 1.94% | 0.96% |

<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## Frame exchange analysis

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.201124 s | AP → STA 1 | QoS Data, HE-SU | MCS known=1, MCS=0, retry=0 | warm-up transmission |
| 2 | 0.201184 s | STA 1 → AP | Ack | retry=0 | acknowledges frame 1 |
| 3 | 0.201236 s | AP → STA 1 | Action | management | block-ack setup |
| 5 | 0.201351 s | STA 1 → AP | Action | management | setup response |
| 7 | 0.202580 s | AP → STA 2 | QoS Data, HE-SU | MCS known=1, MCS=0, retry=0 | next peer transmission |

These frames directly establish known captured MCS values and exchange order.
They do not reveal Minstrel's probability or candidate-selection reason; those
are internal vectors. TShark frame numbers are not OMNeT++ event numbers.

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| HE Minstrel is active and varies MCS | `PASS` | controller typename and 0--11 bounds | observed 0--9, NSS 1 | known HE MCS visible | 13.338 ± 1.053 Mbps |
| selected attempts usually succeed | `PASS` | same treatment | success 0.998445 ± 0.001980 | representative retry bit 0 | delivery recorded |
| Minstrel beats fixed MCS | `INCONCLUSIVE` | fixed control exists | no five-run fixed telemetry | separate run-0 populations | no matched estimate |

The five-run results and run-0 PCAP session are separate and cannot establish
event-level vector-to-frame causality. The bounded verdict is that controller
selection and outcome observability work for the mobile treatment; comparative
algorithm efficacy is not demonstrated.
Evidence basis: selections, outcomes, and known radiotap fields are **direct
observations**; success fractions and goodput intervals are **derived
measurements**; a controller decision causing a separately captured frame
would be an **inference**.

## Limitations and inconclusive claims

- No five-run `FixedMcs` or static `HeMinstrel` result set is retained.
- The mobile campaign changes path geometry only for host 3, but the reported
  aggregate goodput pools all four destinations.
- Controller retry count and MAC retry signals need a same-attempt join.
- A matched three-configuration campaign with paired seeds and per-peer
  delivery would resolve the primary comparison gap.

## Further experiments

- Run `FixedMcs`, `HeMinstrel`, and `HeMinstrelMobile` with five paired seeds;
  predict selection variability only in adaptive rows.
- Sweep host-3 speed while retaining per-peer MCS, SNIR, delay, and delivery.
- Run `HighCollisionRate` and predict nonzero failure/retry telemetry before
  interpreting throughput.

## Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | no stable join among candidate choice, probability, frame, and MAC retry |
| Intended behavior | make one rate decision traceable through attempt and outcome |
| Smallest change surface | shared analysis correlation first; controller/HCF telemetry only if identifiers are absent |
| Observability | decision ID, peer, MCS/NSS, probability, attempt, retry, result |
| Validation | fixed/adaptive/mobile matched controls; boundary MCS checks and five paired seeds |
| Compatibility and risks | instrumentation must not perturb exploration timing or random streams |
| Architecture and sealing | apply architectural requirements before any production source change |
| Next handoff | HE rate-control owner and independent regression reviewer |

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| scalar/vector | `results/scalar-vector/20260725T120411Z` | mobile 0--4 | figure JSON and named vectors | hashes retained in JSON |
| PCAP/results/log | `results/packet-statistics/20260724T175025Z` | fixed, Minstrel, mobile; run 0 | shared analyzer; TShark 4.6.4 | separate from five-run campaign |
