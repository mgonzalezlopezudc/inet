# Walkthrough: 802.11ax Uplink OFDMA and UORA

This example provides scheduled-uplink, mixed-access, and UORA configurations.
The retained evidence includes a five-run UORA comparison and a separate run-0
packet-exchange appendix.

## Evidence status

- Scalar/vector session: [`results/scalar-vector/20260725T181500Z`](results/scalar-vector/20260725T181500Z). `MixedUora`, `UoraLightContention`, `UoraHeavyContention`, and `UoraMoreRandomAccessRus` each have complete `.sca` and `.vec` files for runs 0–4.
- Dashboard: [UORA figure](../analysis/figures/uora/uora-dashboard.png) and its [provenance](../analysis/figures/uora/uora-dashboard.png.json). The provenance records all four conditions, input hashes, scalar filters, run-level aggregation, and confidence-interval method.
- Packet session: [`results/packet-statistics/20260724T175025Z`](results/packet-statistics/20260724T175025Z). It contains run-0 AP-interface PCAPng files for `EdcaBaseline`, `EqualRus`, `MixedUora`, `ScheduledOnly`, `UoraLightContention`, and `UoraMoreRandomAccessRus`. The exact generated tables appear below.
- Supplemental heavy-load packet session: [`results/packet-statistics/20260725T182100Z`](results/packet-statistics/20260725T182100Z). It adds the missing run-0 `UoraHeavyContention` AP-interface capture.

The scalar/vector campaign and packet sessions are separate experiments.
PcapRecorder instrumentation changes the event trajectory, so exact run-0
counter values are not expected to match between them. Compare configurations
within a session, and use PCAP for frame-level behavior rather than substituting
its frame counts for the UORA outcome counters.

## Configuration matrix

The configuration inputs in this table are defined by [the INI file](omnetpp.ini).

| Config | Stations and measured traffic | UL access | Configured RA-RUs |
|---|---|---|---:|
| `EdcaBaseline` | 3, 1000 B / 5 ms | EDCA | 0 |
| `ScheduledOnly` | 3, 1000 B / 5 ms | backlog-scheduled | 0 |
| `EqualRus` | 3, 1000 B / 5 ms | equal-sized scheduled RUs | 0 |
| `MixedUora` | 3, 1000 B / 5 ms | scheduled plus UORA | 1–3 |
| `UoraLightContention` | 8, 100 B / 4 ms | scheduled plus UORA | 1 |
| `UoraHeavyContention` | 8, 100 B / 1 ms | scheduled plus UORA | 1 |
| `UoraMoreRandomAccessRus` | 8, 100 B / 1 ms | scheduled plus UORA | 5 |

The last two rows have the same station count, packet size, packet interval,
measurement window, and scheduler family; their configured RA-RU count differs.
This makes them the matched heavy-load comparison documented below.

## Five-run UORA comparison

The evidence is the per-STA scalar pair
`heUlRandomAccessAttempt:count` and `heUlRandomAccessSuccess:count` in the 15
`.sca` files under the three eight-STA configuration directories and the five
`.sca` files under `MixedUora`. Success probability is the run total of
successes divided by the run total of attempts.
Fairness is the Jain index over the per-STA success counts—three for
`MixedUora`, eight for the other conditions—and is undefined for an
all-zero-success run. These counters cover the complete `0–2 s` simulation;
the INI file defines application phases but no OMNeT++ `warmup-period`.

![Five-run UORA comparison](../analysis/figures/uora/uora-dashboard.png)

| Condition | Mean attempts | Mean success probability | Mean successful transmissions | Success fairness |
|---|---:|---:|---:|---:|
| Mixed, adaptive 1–3 RA-RUs | 68.6 ± 11.7 | 1.000 ± 0.000 | 68.6 ± 11.7 | 0.661 ± 0.003 |
| Light, 1 RA-RU | 213.4 ± 55.6 | 0.078 ± 0.029 | 16.2 ± 5.1 | 0.475 ± 0.181 |
| Heavy, 1 RA-RU | 9.4 ± 5.1 | 0.120 ± 0.157 | 0.8 ± 0.6 | 0.125 over four defined runs |
| Heavy, 5 RA-RUs | 19.2 ± 7.8 | 0.370 ± 0.123 | 7.2 ± 4.0 | 0.543 ± 0.184 |

Means and two-sided 95% Student-t confidence intervals use runs 0–4. The
heavy-contention, one-RA-RU fairness row excludes run 1 because all eight
success counts are zero; the other four defined fairness values are each
0.125. `MixedUora` is a reference, not a matched fourth treatment: it has three
stations, 1000-byte packets every 5 ms, and an adaptive 1–3 RA-RU range. The
other conditions have eight stations and 100-byte packets. Only
`UoraHeavyContention` and `UoraMoreRandomAccessRus` hold station count, offered
load, packet size, scheduler, seeds, and run duration fixed while changing the
RA-RU count.

### Scalar and vector interpretation by configuration

The UORA attempt/success signals have `count` scalar recorders but no
attempt/success vectors. The temporal evidence therefore comes from
`endToEndDelay:vector` at `server.app[0]`. The table below first computes one
mean over delivered packets in `[0.3 s, 2 s)` for each run, then reports the
mean and 95% t interval over the five run means. It is delivery-conditioned:
packets still queued at 2 s do not contribute.

| Configuration | Mean end-to-end delay | Run-0 delivered samples | Run-0 delay range |
|---|---:|---:|---:|
| `MixedUora` | 3.115 ± 0.211 ms | 1018 | 0.666–13.729 ms |
| `UoraLightContention` | 22.615 ± 4.905 ms | 3357 | 0.208–165.869 ms |
| `UoraHeavyContention` | 136.335 ± 37.209 ms | 6920 | 0.233–1211.051 ms |
| `UoraMoreRandomAccessRus` | 126.196 ± 12.746 ms | 7322 | 0.182–1143.597 ms |

- **`MixedUora`:** `maxMuStations=2` lets the scheduler serve one part of the
  three-STA workload while the advertised random-access RU serves the other
  part. Across all five runs, host 0 records no UORA attempts and hosts 1–2
  account for all 68.6 mean successes. The perfect UORA success probability
  therefore describes a lightly contended random-access path, while the Jain
  value near two thirds mechanically reflects the scheduled/UORA role split;
  it is not evidence of unfair total service. The low delay vector is
  consistent with the modest three-STA load.

- **`UoraLightContention`:** fixing one RA-RU while expanding to eight STAs
  creates many eligibility events—213.4 attempts per run—but only 7.8% become
  recorded successes. Successes are spread unevenly across STAs, producing
  0.475 Jain fairness. The 22.6 ms mean delay is well below the heavy-load
  values because 100-byte packets arrive every 4 ms rather than every 1 ms.

- **`UoraHeavyContention`:** only the arrival interval changes from 4 ms to
  1 ms. The heavy backlog keeps the medium busy and sharply reduces the number
  of Basic Trigger opportunities, so the UORA attempt count falls rather than
  rising with offered load. One run has no success and every other run has a
  success at only one STA, explaining both the wide probability interval and
  Jain value 0.125. The delay vector shows the consequence of overload: a
  136 ms five-run mean and a run-0 tail beyond 1.2 s.

- **`UoraMoreRandomAccessRus`:** this is the matched heavy-load treatment.
  Five advertised RA-RUs give eligible stations more choices per Basic Trigger.
  The retained sample records about nine times as many UORA successes as the
  one-RU heavy condition (7.2 versus 0.8), a higher success probability, and
  success across more STAs. Its delivery-conditioned mean delay is only
  modestly lower and the confidence intervals overlap: reserving additional
  random-access RUs also leaves fewer RUs for scheduled service, and the
  workload remains overloaded. The evidence supports increased UORA capacity
  in this sample, not an optimal RA-RU count.

### PCAP/TShark interpretation by configuration

The captures observe `ap.wlan[0]`. A Basic Trigger's decoded
`wlan.trigger.he.user_info.aid12` values expose advertised random-access RUs:
`AID12=0` is the random-access allocation. HE-TB QoS Null frames at the same
timestamp and frequency are direct evidence of simultaneous transmissions on
one RU, but only the coordinator scalars establish whether INET classified an
attempt as successful.

| Configuration | Basic / BSRP Triggers | AID-0 entries per Basic Trigger | HE-TB QoS Null | Same-time, same-RU groups | QoS Data | BA |
|---|---:|---:|---:|---:|---:|---:|
| `MixedUora` | 378 / 103 | 1 | 960 | 0 groups / 0 frames | 1462 | 480 |
| `UoraLightContention` | 440 / 101 | 1 | 832 | 68 groups / 226 frames | 8459 | 894 |
| `UoraHeavyContention` | 55 / 101 | 1 | 317 | 4 groups / 12 frames | 13437 | 336 |
| `UoraMoreRandomAccessRus` | 50 / 101 | 5 | 323 | 6 groups / 12 frames | 12810 | 341 |

- **`MixedUora`:** the adaptive 1–3 range resolves to one AID-0 RU in every
  run-0 Basic Trigger. Basic Triggers continue from 0.208 s to 1.999 s, and no
  duplicate-RU response group is observed. This agrees with the separate
  scalar campaign's collision-free UORA result, without treating the two
  sessions' exact counts as interchangeable.

- **`UoraLightContention`:** one AID-0 RU is repeatedly exposed through the
  whole data phase (last Basic Trigger at 1.999 s). The 68 duplicate-RU groups
  make contention visible in the capture and explain why numerous scalar
  attempts coexist with a low success probability.

- **`UoraHeavyContention`:** the AP emits only 55 Basic Triggers, ending at
  0.806 s, while the capture is dominated by 13437 QoS Data frames and 7584
  retry-bit observations. For example, frames 621 and 622 at 0.302547 s are
  distinct STA HE-TB responses on 5006 MHz after the same Basic Trigger. The
  single advertised RA-RU is directly oversubscribed; the scalar counter
  supplies the resulting 13 attempts and one success for this packet run.

- **`UoraMoreRandomAccessRus`:** every Basic Trigger carries five AID-0 user
  entries. Only six duplicate-RU groups are observed even though five RUs allow
  more simultaneous choices; the run-0 packet session records 21 attempts and
  eight successes. Basic Triggers occur only through 0.310 s, after which the
  trace is dominated by QoS Data/BAR exchanges. The five-RU treatment changes
  contention capacity, not merely the total frame count.

Reproduce the subtype counts for any of these captures with:

```sh
tshark -n -r "$PCAP" -q \
  -z io,stat,0,'wlan.trigger.he.trigger_type == 0','wlan.trigger.he.trigger_type == 4','wlan.fc.type_subtype == 0x2c && radiotap.he.data_1.ppdu_format == 3','wlan.fc.type_subtype == 0x28 && radiotap.he.data_1.ppdu_format == 0','wlan.fc.type_subtype == 0x18','wlan.fc.type_subtype == 0x19','wlan.fc.retry == 1'
```

For a Trigger/HE-TB/Block-Ack timeline, use:

```sh
tshark -n -r "$PCAP" \
  -Y 'wlan.trigger.he.trigger_type == 0 || (wlan.fc.type_subtype == 0x2c && radiotap.he.data_1.ppdu_format == 3) || wlan.fc.type_subtype == 0x19' \
  -T fields -E separator='|' -E occurrence=a \
  -e frame.number -e frame.time_epoch -e frame.packet_flags_direction \
  -e wlan.fc.type_subtype -e wlan.ta -e wlan.ra \
  -e radiotap.he.data_1.ppdu_format -e radiotap.channel.freq \
  -e wlan.trigger.he.user_info.aid12 -e _ws.col.Info
```

## Reproduction and inspection

Run one configuration from the repository root:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/ul_ofdma/omnetpp.ini \
  -c UoraMoreRandomAccessRus -r 0
```

Query the completed scalar evidence directly:

```sh
opp_scavetool query -l \
  -f 'name =~ "heUlRandomAccessAttempt:count" OR name =~ "heUlRandomAccessSuccess:count"' \
  examples/ieee80211ax/ul_ofdma/results/scalar-vector/20260725T181500Z/MixedUora/*.sca \
  examples/ieee80211ax/ul_ofdma/results/scalar-vector/20260725T181500Z/UoraLightContention/*.sca \
  examples/ieee80211ax/ul_ofdma/results/scalar-vector/20260725T181500Z/UoraHeavyContention/*.sca \
  examples/ieee80211ax/ul_ofdma/results/scalar-vector/20260725T181500Z/UoraMoreRandomAccessRus/*.sca
```

Regenerate the four-condition dashboard with:

```sh
python3 examples/ieee80211ax/analysis/run_campaign.py uora -j$(nproc)
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/first_tranche.py uora
```

Regenerate the packet appendix:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/analyze_pcap_types.py \
  --generate --subdir ul_ofdma
```

For a direct decode of the retained five-RA-RU capture:

```sh
tshark -n -r \
  'examples/ieee80211ax/ul_ofdma/results/packet-statistics/20260724T175025Z/UoraMoreRandomAccessRus/UoraMoreRandomAccessRus-#0Lan80211AxUlOfdma.ap.wlan[0].pcap' \
  -c 20
```

The generated run-0 tables establish observed frame subtypes and radiotap
fields at `ap.wlan[0]`. For example, the `EdcaBaseline` table has no Trigger
row, while `EqualRus`, `MixedUora`, `ScheduledOnly`, `UoraLightContention`, and
`UoraMoreRandomAccessRus` contain Trigger rows. Frame subtype totals alone do
not distinguish scheduled from random access. The Trigger's AID12 fields expose
advertised AID-0 RA-RUs, while the UORA scalar counters above remain the
evidence for attempts and successes.

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
| **PASS** | EdcaBaseline produced protocol-visible wireless observations | 2483 AP/global transmission observations |
| **PASS** | EqualRus produced protocol-visible wireless observations | 2797 AP/global transmission observations |
| **PASS** | MixedUora produced protocol-visible wireless observations | 4375 AP/global transmission observations |
| **PASS** | ScheduledOnly produced protocol-visible wireless observations | 2797 AP/global transmission observations |
| **PASS** | UoraLightContention produced protocol-visible wireless observations | 11619 AP/global transmission observations |
| **PASS** | UoraMoreRandomAccessRus produced protocol-visible wireless observations | 14027 AP/global transmission observations |

### Configuration: `EdcaBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2483**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1460 | 58.80% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -62.9 dBm | - | 97.29% | 45.35% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1023 | 41.20% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.71% | 1.26% |

### Configuration: `EqualRus`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2797**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1054 | 37.68% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.7 dBm | - | 76.23% | 32.74% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 435 | 15.55% | 34.0 B | 0.0 B | 386.6 us | 17.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 19.58% | 8.41% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 146 | 5.22% | 46.4 B | 3.1 B | 35.5 us | 1.0 us | 5010 MHz | - | 10.0 dBm | 0.60% | 0.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 146 | 5.22% | 58.0 B | 0.0 B | 39.3 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.67% | 0.29% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1016 | 36.32% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.92% | 1.25% |

### Configuration: `MixedUora`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4375**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1462 | 33.42% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.5 dBm | - | 68.24% | 45.42% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 960 | 21.94% | 34.0 B | 0.0 B | 378.4 us | 17.9 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 27.29% | 18.16% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 481 | 10.99% | 51.8 B | 11.1 B | 37.3 us | 3.7 us | 5010 MHz | - | 10.0 dBm | 1.35% | 0.90% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 480 | 10.97% | 47.6 B | 4.1 B | 35.9 us | 1.4 us | 5010 MHz | - | 10.0 dBm | 1.29% | 0.86% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 992 | 22.67% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 1.84% | 1.22% |

### Configuration: `ScheduledOnly`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2797**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1054 | 37.68% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.7 dBm | - | 76.22% | 32.74% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 435 | 15.55% | 34.0 B | 0.0 B | 387.1 us | 16.8 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 19.60% | 8.42% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 146 | 5.22% | 46.4 B | 3.1 B | 35.5 us | 1.0 us | 5010 MHz | - | 10.0 dBm | 0.60% | 0.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 146 | 5.22% | 58.0 B | 0.0 B | 39.3 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.67% | 0.29% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1016 | 36.32% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.92% | 1.25% |

### Configuration: `UoraLightContention`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **11619**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 7890 | 67.91% | 167.5 B | 1.9 B | 94.7 us | 10.5 us | 5010 MHz | -58.9 dBm | - | 61.01% | 37.35% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 569 | 4.90% | 184.2 B | 112.3 B | 136.8 us | 61.4 us | 5010 MHz | -56.3 dBm | - | 6.36% | 3.89% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 832 | 7.16% | 34.0 B | 0.0 B | 389.2 us | 15.8 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 26.45% | 16.19% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 541 | 4.66% | 51.0 B | 10.5 B | 37.0 us | 3.5 us | 5010 MHz | - | 10.0 dBm | 1.64% | 1.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 834 | 7.18% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -57.7 dBm | - | 1.91% | 1.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 894 | 7.69% | 40.8 B | 7.4 B | 33.6 us | 2.5 us | 5010 MHz | - | 10.0 dBm | 2.45% | 1.50% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 26 | 0.22% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.05% | 0.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.14% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.03% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 17 | 0.15% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.10% | 0.06% |

### Configuration: `UoraMoreRandomAccessRus`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **14027**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 12709 | 90.60% | 166.1 B | 0.7 B | 91.7 us | 5.9 us | 5010 MHz | -57.6 dBm | - | 87.20% | 58.30% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 101 | 0.72% | 250.2 B | 256.4 B | 172.9 us | 140.3 us | 5010 MHz | -58.3 dBm | - | 1.31% | 0.87% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 323 | 2.30% | 34.0 B | 0.0 B | 384.6 us | 17.6 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz, 5014 MHz | -75.0 dBm | - | 9.29% | 6.21% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 151 | 1.08% | 72.0 B | 1.4 B | 44.0 us | 0.5 us | 5010 MHz | - | 10.0 dBm | 0.50% | 0.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 319 | 2.27% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -56.8 dBm | - | 0.67% | 0.45% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 341 | 2.43% | 38.5 B | 7.8 B | 32.8 us | 2.6 us | 5010 MHz | - | 10.0 dBm | 0.84% | 0.56% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 51 | 0.36% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.09% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.11% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.03% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 16 | 0.11% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.08% | 0.06% |

### Analysis of Packet Distribution
The `EdcaBaseline` table has no Trigger row. The scheduled and mixed-access
tables record **Trigger**, HE-TB, and AP **Block Ack** observations. Frame
subtype counts alone do not distinguish an AID-0 random-access attempt from
scheduled access or prove a collision. The Trigger user-info AID12 fields do
identify advertised AID-0 RA-RUs, and same-time/same-frequency HE-TB responses
expose overlapping use of an RU; the per-STA
`heUlRandomAccessAttempt:count` and `heUlRandomAccessSuccess:count` scalars are
the attempt and success evidence.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->
