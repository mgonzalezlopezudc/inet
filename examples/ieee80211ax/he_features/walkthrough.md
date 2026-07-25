# HE preamble puncturing walkthrough

This walkthrough teaches how an 802.11ax access point (AP) disables a
20 MHz part of a wider channel and keeps scheduled resource units (RUs) out
of that region. The retained evidence directly shows INET's mask and
allocation telemetry and five-run delivery outcomes; the packet capture
shows HE exchanges but does not export the puncturing mask.

## Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain why preamble puncturing preserves usable spectrum around a busy
  secondary subchannel;
- identify INET's punctured-mask and RU-placement vectors;
- distinguish model telemetry from radiotap-decoded PHY facts; and
- reproduce the treatment and diagnose a mask/allocation violation.

Preamble puncturing lets an HE multi-user (HE-MU) transmission omit selected
20 MHz subchannels of an 80 or 160 MHz channel. The scheduler must then place
RUs only in enabled frequency regions. The learning outcome is to trace
configured mask → recorded mask → RU placement → delivery. The validation
outcome is a scoped `PASS` for recorded runtime mask changes and retained
delivery, but only `INCONCLUSIVE` packet-level mask validation.

## Scenario description

[Lan80211AxHeFeatures.ned](Lan80211AxHeFeatures.ned) extends the common
single-BSS topology with one wired UDP server, one AP, four stationary STAs,
and an optional legacy interferer. [omnetpp.ini](omnetpp.ini) sends downlink
UDP to all four STAs from 0.3 s over an 80 MHz, 5.2 GHz channel. The
interference treatments place a continuous 20 MHz 802.11a transmitter on the
second subchannel; `DynamicPuncturing` activates both jammer and mask during
the middle of the 1 s run.

```text
server -- Ethernet --> AP == 80 MHz HE MU ==> host[0..3]
                         X second 20 MHz <== legacy interferer
```

## Standards and INET model boundary

IEEE Std 802.11-2024 describes HE PHY preamble puncturing in Clause 27 and
enumerates 80/160 MHz punctured `CH_BANDWIDTH` values in Table 27-1
(`80211ax-2024:chunk:10001`); Table 27-21 carries the corresponding HE-SIG-A
encoding (`80211ax-2024:chunk:10158`). INET represents the requested mask as
an HCF string and records mask and RU tone geometry as model vectors.
Radiotap HE fields authoritatively expose only known MCS/BW/RU facts; the
retained analyzer explicitly reports that subtype counts cannot establish
mask transitions or RU non-overlap.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| dynamic puncturing changes runtime mask | `PASS` | `hePuncturedSubchannelMask:vector` values 0 and 2 | `DynamicPuncturing` 0--4 | direct model telemetry |
| RU placement can be checked against mask | `PASS` | timestamp-aligned mask, tone offset/size, STA ID vectors | retained campaign | model-level allocation evidence |
| capture directly decodes puncturing mask | `INCONCLUSIVE` | AP PCAP and generated evidence check | `PreamblePuncturing` run 0 | decisive mask absent from table |
| puncturing improves goodput under interference | `INCONCLUSIVE` | five-run goodput CIs | four configs, 0--4 | interference CIs overlap |

## Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `CleanChannelBaseline` | clean reference | no jammer, no mask | downlink; 80 MHz | 0--4 | full-band delivery |
| `LegacyInterferenceWithoutPuncturing` | negative control | jammer, empty mask | matched 80 MHz load | 0--4 | no puncture telemetry |
| `PreamblePuncturingUnderInterference` | treatment | jammer; static `"0100"` mask | matched 80 MHz load | 0--4 | allocations avoid subchannel index 1 |
| `DynamicPuncturing` | transition treatment | mask 0→2→0; jammer 0.3--0.7 s | matched 80 MHz load | 0--4 | mask changes during active interval |

The interference rows inherit `BccBaseline` and then override width, bitrate,
LDPC support, explicit interferer, and offered load. `CleanChannelBaseline`
does not inherit the 0.5 ms interference workload override and its 16 Mbps
goodput is therefore confounded; it is not a capacity control for the other
three rows. Seeds are retained as result attributes, not inferred here.

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| mask follows configured interval | AP `hePuncturedSubchannelMask:vector` | missing 0→2→0 transition | HCF dynamic puncturing | inspect effective times and HCF logs |
| no RU overlaps masked 20 MHz region | aligned tone offset/size and mask vectors | allocation crosses disabled region | HE DL scheduler / RU allocator | export same-timestamp tuples |
| HE frames remain protocol-visible | AP PCAP known HE fields | empty/undecoded capture | recorder / radiotap encoding | inspect `radiotap.he.data_*_known` |

## Reproduction

Run from the repository root:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/he_features/omnetpp.ini \
  -c DynamicPuncturing -r 0 \
  --result-dir=examples/ieee80211ax/he_features/results/manual/DynamicPuncturing
```

Status: `NOT RUN` during this rewrite. The retained
`results/packet-statistics/20260724T175025Z/PreamblePuncturing/cmdenv.stdout`
reached 1 s and `End.`, but the original process exit status and exact command
were not retained. Suite regeneration, also `NOT RUN` here:

```sh
python3 examples/ieee80211/analysis/analyze_pcap.py \
  --suite examples/ieee80211/analysis/suites/ax.json \
  --generate --subdir he_features --run 0
```

## Scalar and vector analysis

Inputs:
`results/scalar-vector/20260725T120411Z/{configuration}/*.{sca,vec}`.
Figure provenance:
[puncturing-frequency-allocation.png.json](../analysis/figures/puncturing/puncturing-frequency-allocation.png.json).

```sh
opp_scavetool query -l \
  -f 'name =~ "hePuncturedSubchannelMask:vector" OR name =~ "heRuToneOffset:vector" OR name =~ "heRuToneSize:vector" OR name =~ "heStaId:vector" OR name =~ "packetReceived:vector(packetBytes)"' \
  examples/ieee80211ax/he_features/results/scalar-vector/20260725T120411Z/*/*.sca \
  examples/ieee80211ax/he_features/results/scalar-vector/20260725T120411Z/*/*.vec
```

| Metric | Window/aggregation | Clean | unpunctured interference | punctured interference | dynamic | Interpretation |
|---|---|---:|---:|---:|---:|---|
| aggregate goodput | per-run application bytes, then mean ± 95% t-CI over 5 runs | 16.000 ± 0.000 Mbps | 63.931 ± 0.033 | 63.902 ± 0.043 | 63.921 ± 0.033 | outcome; interference rows overlap |
| puncture mask | event vector | — | empty/0 | configured nonzero | values 0 and 2 | mechanism telemetry |

The plot provenance lists all input hashes. Interpret tone offset, tone size,
STA ID, and mask only at aligned timestamps. Five runs support variability of
goodput; vector samples within a run are not repetitions.

## PCAP statistics

Capture point: `Lan80211AxHeFeatures.ap.wlan[0]`

Capture:
`results/packet-statistics/20260724T175025Z/PreamblePuncturing/PreamblePuncturing-#0Lan80211AxHeFeatures.ap.wlan[0].pcap`

Scope: legacy PCAP AP observations, simulation timestamps, TShark 4.6.4;
FCS/checksum settings are not retained.

```sh
tshark -n -r \
  'examples/ieee80211ax/he_features/results/packet-statistics/20260724T175025Z/PreamblePuncturing/PreamblePuncturing-#0Lan80211AxHeFeatures.ap.wlan[0].pcap' \
  -Y 'frame.number <= 8' -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.ta -e wlan.ra \
  -e wlan.fc.type_subtype -e radiotap.he.data_1.data_mcs_known \
  -e radiotap.he.data_3.data_mcs \
  -e radiotap.he.data_1.data_bw_ru_allocation_known \
  -e radiotap.he.data_5.data_bw_ru_allocation
```

The generated block below is exhaustive packet-population evidence and is
preserved verbatim; it is subordinate to the mechanism vectors.

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
| **PASS** | BccBaseline produced protocol-visible wireless observations | 2264 AP/global transmission observations |
| **PASS** | PreamblePuncturing produced protocol-visible wireless observations | 2264 AP/global transmission observations |
| **INCONCLUSIVE** | Puncturing mask transitions and RU allocations do not overlap punctured subchannels | Subtype counts cannot establish the puncturing mask; result vectors remain authoritative |

### Configuration: `BccBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2264**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24c219" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC] | 354 | 15.64% | 1066.0 B | 0.0 B | 619.1 us | 0.0 us | 5050 MHz | - | 20.0 dBm | 13.28% | 21.92% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 350 | 15.46% | 55.0 B | 0.0 B | 38.3 us | 0.0 us | 5050 MHz | - | 20.0 dBm | 0.81% | 1.34% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 69 | 3.05% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5050 MHz | - | 20.0 dBm | 0.12% | 0.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 69 | 3.05% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5050 MHz | -67.0 dBm | - | 0.13% | 0.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#184baa" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, BCC] | 350 | 15.46% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5045 MHz | -67.0 dBm | - | 2.30% | 3.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0842a6" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, BCC] | 700 | 30.92% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5053 MHz, 5057 MHz | -67.0 dBm | - | 8.04% | 13.27% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.18% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5050 MHz | -67.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.35% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5050 MHz | -67.0 dBm | 20.0 dBm | 0.01% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 10 | 0.44% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5050 MHz | -67.0 dBm | 20.0 dBm | 0.04% | 0.07% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#33cc52" /></svg> | Control: Subtype 0 [HE-MU, HE, GI 3.2 us] | 350 | 15.46% | 3210.0 B | 0.0 B | 3547.8 us | 0.0 us | 5050 MHz | - | 20.0 dBm | 75.26% | 124.17% |

### Configuration: `PreamblePuncturing`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2264**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#27a52f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 80 MHz, GI 3.2 us, LDPC] | 354 | 15.64% | 1066.0 B | 0.0 B | 175.2 us | 0.0 us | 5200 MHz | - | 20.0 dBm | 4.32% | 6.20% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 350 | 15.46% | 55.0 B | 0.0 B | 38.3 us | 0.0 us | 5200 MHz | - | 20.0 dBm | 0.93% | 1.34% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 69 | 3.05% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5200 MHz | - | 20.0 dBm | 0.13% | 0.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 69 | 3.05% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5200 MHz | -67.0 dBm | - | 0.15% | 0.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 1050 | 46.38% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5165 MHz, 5176 MHz, 5206 MHz | -67.0 dBm | - | 7.92% | 11.37% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 12 | 0.53% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5200 MHz | -67.0 dBm | 20.0 dBm | 0.02% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 10 | 0.44% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5200 MHz | -67.0 dBm | 20.0 dBm | 0.05% | 0.07% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#33cc52" /></svg> | Control: Subtype 0 [HE-MU, HE, GI 3.2 us] | 350 | 15.46% | 3210.0 B | 0.0 B | 3547.8 us | 0.0 us | 5200 MHz | - | 20.0 dBm | 86.48% | 124.17% |

<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## Frame exchange analysis

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.200196 s | AP → STA 1 | QoS Data, HE-SU | MCS known=1; MCS=1; BW/RU known=1; code=2 | first warm-up downlink |
| 2 | 0.200240 s | STA 1 → AP | Ack | no HE data fields | acknowledges data |
| 3 | 0.200292 s | AP → STA 1 | Action | management exchange | block-ack setup |
| 4 | 0.200337 s | STA 1 → AP | Ack | no HE data fields | acknowledges action |

This is a representative retained exchange, not a puncturing proof. The
capture authoritatively supplies known HE fields for frame 1, but no exported
mask field ties it to a disabled 20 MHz region. The model vectors remain the
decisive allocation evidence.

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| dynamic mask changes | `PASS` | start 0.35 s, end 0.7 s, mask `"0100"` | recorded masks 0 and 2 | not decoded | delivery continues |
| RUs avoid punctured region | `PASS` | puncture-aware scheduler selected | aligned mask/tone vectors | `INCONCLUSIVE` | not a delivery proof |
| puncturing improves goodput | `INCONCLUSIVE` | matched interference topology | mechanism active | HE traffic visible | 95% CIs overlap |

The scalar/vector and packet sessions are separate. They support adjacent
findings but not event-level packet-to-vector causality. The evidence
demonstrates INET's mask transition and allocation observability, while making
no broad performance claim.
Evidence basis: mask and known packet fields are **direct observations**,
goodput and its intervals are **derived measurements**, and any uncorrelated
mask-to-frame link is an **inference**.

## Limitations and inconclusive claims

- The retained radiotap export does not expose the puncturing mask.
- `CleanChannelBaseline` has a different offered load and is confounded.
- No co-recorded log ties one scheduler decision to one captured PPDU.
- A minimal resolving run would co-record mask/tone vectors, scheduler
  decision ID, and AP PCAP for one static and one dynamic treatment.

## Further experiments

- Shift the dynamic interval while holding jammer timing fixed; predict mask
  transitions move but jammer frames do not.
- Try each valid secondary 20 MHz mask and assert no aligned RU overlap.
- Add a matched-load clean 80 MHz control and five paired seeds before making
  a capacity comparison.

## Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | packet artifacts cannot directly expose mask-to-PPDU correlation |
| Intended behavior | make one scheduler decision, mask, RU geometry, and emitted PPDU joinable |
| Smallest change surface | shared AX feature plugin/result correlation first; production telemetry only if that is insufficient |
| Observability | stable decision/PPDU identifier in mask and RU records |
| Validation | static and dynamic control/treatment, one run first then five paired seeds; vectors + PCAP |
| Compatibility and risks | preserve typed-PHY fail-closed decoding and legacy suite output |
| Architecture and sealing | required before any `src/inet` change; no production edit is authorized here |
| Next handoff | HE scheduler/analysis owner and independent Wi-Fi regression reviewer |

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| scalar/vector | `results/scalar-vector/20260725T120411Z` | four comparison configs, 0--4 | figure JSON and named vectors | hashes in JSON; separate from PCAP |
| PCAP/results/log | `results/packet-statistics/20260724T175025Z` | `BccBaseline`, `PreamblePuncturing`, run 0 | shared analyzer; TShark 4.6.4 | AP and STA captures retained |
