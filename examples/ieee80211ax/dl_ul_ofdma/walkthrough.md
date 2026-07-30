# Walkthrough: Bidirectional downlink and uplink OFDMA

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260730T083856Z`
- PCAP: `20260730T083856Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260730T083856Z`.

This example gives three associated HE stations simultaneous downlink and
uplink traffic. In the retained five-run comparison, the OFDMA configuration
has protocol-visible downlink MU and scheduled uplink exchanges, and its
application goodput is higher with lower p95 delay than the matched SU
control. The delivery advantage is an observed workload result, not a
general performance guarantee; the analysis ledger leaves the threshold-based
comparison `INCONCLUSIVE` because no acceptance threshold is defined.

The evidence basis is explicit: decoded PCAP fields are direct observations,
the analyzer tables and figure are derived measurements, and the explanation
of why the result occurs is an inference bounded by this scenario.

## [agent] Learning objectives and feature primer

The question is whether one AP can serve active traffic in both directions by
sharing a HE transmission opportunity across stations. In `Ofdma`, the AP
uses equal-sized resource units (RUs) for downlink MU transmissions and sends
Trigger frames that schedule backlog-based uplink RUs. The stations then send
HE-TB responses concurrently on their assigned RUs. In `Su`, both multi-user
paths are disabled, so the same queues, PHY, traffic, topology, and seed
sets use single-user HE transmissions.

The decisive exchange is therefore not just a higher sink byte count: it is an
AP Trigger followed by HE-TB responses, together with HE-MU downlink payloads.

## [agent] Scenario description

The topology has one AP, three stationary hosts, and a wired server. The
server offers aggregate 2.4 Mbps downlink traffic using 100-byte UDP packets;
the hosts continuously offer aggregate 0.48 Mbps uplink traffic. A 250 ms
warm-up establishes Block Ack agreements, and measurement runs from 0.3 s to
1.95 s of the 2 s simulation. The 50 ms minimum Trigger interval batches
uplink backlog while leaving AP transmission opportunities for downlink.

The topology is defined in [`Lan80211AxDlUlOfdma.ned`](Lan80211AxDlUlOfdma.ned)
and the matched configurations are in
[`omnetpp.ini`](omnetpp.ini). Both use a single-stream 20 MHz HE PHY at fixed
MCS 1. The only causal comparison is the OFDMA scheduler and uplink-MU
enablement versus the SU fallback.

## [agent] Standards and INET model boundary

IEEE Std 802.11ax-2024, Clause 4.3.16, describes HE AP Trigger frames as the
mechanism that identifies participating non-AP STAs and assigns RUs and/or
spatial streams for UL MU operation. The same clause identifies DL and UL
OFDMA as HE features. The repository corpus records this material as
`80211ax-2024:chunk:00407`.

INET models that behavior through `HeHcf`, its downlink and uplink schedulers,
and the HE PHY. The INI requests equal-sized downlink RUs, backlog-based
uplink scheduling, no random-access RUs, and a 50 ms minimum Trigger interval;
those settings are configuration inputs, not proof that an exchange occurred.
The AP capture supplies that runtime proof. It shows decoded HE-MU and HE-TB
observations in `Ofdma`, while the `Su` capture is the single-user control.

## [agent] Evidence status

| Claim | Status | Script-generated evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| `Ofdma` contains downlink MU and scheduled uplink exchange structure | `PASS` | Generated PCAP statistics and representative frame exchange | AP capture, run 0, seed set 0 | Capture observations establish the protocol-visible structure; they are not de-duplicated application packets |
| `Su` disables the corresponding multi-user paths | `PASS` | Generated PCAP statistics and representative frame exchange | AP capture, run 0, seed set 0 | Control is matched by configuration and observed as SU traffic |
| OFDMA has higher goodput and lower p95 delay for this workload | `INCONCLUSIVE` | Generated scalar/vector plot and table | Five runs, run numbers 0–4 | The script reports the comparison, but the manifest defines no acceptance threshold |

## [agent] Configuration matrix

| Configuration | Role | Causal delta | Runs/seeds | Expected invariant |
|---|---|---|---|---|
| `Su` | Control | UL MU disabled; downlink MU candidate limit is zero, so the HCF uses its SU fallback | 0–4, five independent seed sets | No scheduled HE-TB UL exchange or DL MU allocation is expected |
| `Ofdma` | Treatment | Equal-sized DL RU scheduler, backlog-based UL scheduler, up to three users, UL MU enabled | 0–4, five independent seed sets | AP Trigger/HE-TB exchange and HE-MU payloads are observable |

## [agent] Expected invariants and diagnostic map

| Invariant | Script-generated evidence | Failure symptom | First diagnostic |
|---|---|---|---|
| A Trigger identifies UL participants and is followed by HE-TB responses | Generated frame-exchange timeline and decoded Trigger allocation table | Trigger exists but no matching HE-TB responses | Inspect the AP PCAP's Trigger fields and the `HeHcf` uplink scheduler decisions |
| The control remains single-user | Generated PCAP composition and timeline | HE-MU or HE-TB frames appear in `Su` | Check the effective `maxMuStations` and `enableUlMuOfdma` parameters |
| Application results are measured only in the declared window | Generated scalar/vector provenance | Apparent difference depends on warm-up or shutdown traffic | Recheck the manifest window `[0.3, 1.95)` and sink-module filter |

## [agent] Reproduction

Run from the INET repository root:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py inspect dl_ul_ofdma

MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py run dl_ul_ofdma \
  --suite ax --evidence both --runs 5 --jobs "$(nproc)" \
  --session-id 20260730T083856Z

MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py report dl_ul_ofdma \
  --suite ax --session-id 20260730T083856Z

MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py publish dl_ul_ofdma \
  --suite ax --session-id 20260730T083856Z --update
```

The retained publication session covers runs `[0,5)` for `Ofdma` and `Su`.
Run 0 records the AP `wlan[0]` PCAP from the same trajectory as its scalar
and vector files. Results are under
`examples/ieee80211ax/dl_ul_ofdma/results/20260730T083856Z/`; the simulation
campaign completed successfully. The session contains `.sca` and `.vec`
result artifacts for runs 0–4 and `.pcap` captures for run 0.

## [agent] Scalar and vector analysis

The analyzer selects `packetReceived:vector(packetBytes)` and
`endToEndDelay:vector` from the host/server application sinks. It crops each
run to `[0.3, 1.95)` s, computes one goodput and one pooled p95 delay per run,
and summarizes the five independent runs with two-sided 95% Student-t
intervals. Vector samples are not treated as independent repetitions.

The generated result shows the workload-level outcome: `Ofdma` reaches about
2.58 Mbps mean goodput with about 124 ms mean p95 delay, while `Su` reaches
about 1.46 Mbps with about 423 ms mean p95 delay. These are measurements of
this topology, offered load, fixed MCS, and window; they do not isolate which
direction benefits most or establish a universal OFDMA gain.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-dl_ul_ofdma -->
### [script] Generated scalar/vector plot and table

![dl_ul_ofdma scalar/vector analysis](results/20260730T083856Z/dl-ul-ofdma-delivery-delay.png)

Figure provenance: [`results/20260730T083856Z/dl-ul-ofdma-delivery-delay.png.json`](results/20260730T083856Z/dl-ul-ofdma-delivery-delay.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector / unit=s
- Window / per-run aggregation / exclusions: [0.3, 1.95) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Bidirectional OFDMA / delay p95 ms | 123.965 | 7.01194 |
| Bidirectional OFDMA / goodput mbps | 2.5791 | 0.101959 |
| Single-user baseline / delay p95 ms | 422.853 | 464.64 |
| Single-user baseline / goodput mbps | 1.46269 | 0.65792 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Bidirectional OFDMA and matched single-user control are compared using delivered application bytes | The five-run bidirectional delivery comparison has no manifest-defined acceptance threshold. |
| **INCONCLUSIVE** | The OFDMA condition contains both downlink MU and scheduled uplink trigger exchanges while the single-user control does not | The decisive per-exchange PPDU and RU attribution remains packet evidence without an executable evaluator. |
<!-- END GENERATED: ieee80211-scalar-vector-dl_ul_ofdma -->

## [agent] PCAP statistics

The shared analyzer uses the run-0 AP captures, radiotap decoding, and the
same session-bound scalar metadata. Counts are AP transmission observations,
not delivered application packets; unknown PHY fields remain unknown and
airtime is an estimate, especially for user-dependent HE-TB signaling.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260730T083856Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260730T083856Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260730T083856Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260730T083856Z.json` (SHA-256 `c689ec7ebc963323be382cddd4c04bd58a4a6eac742260bb9e270a52d6c2a12c`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `Ofdma` | `none (all decoded frames)` | 6395 | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (4266), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (908), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (411) | 108.37% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Su` | `none (all decoded frames)` | 3309 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (1970), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (672), Control: Block Ack Request (BAR) (319) | 17.02% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | Ofdma produced protocol-visible wireless observations | 6395 AP/global transmission observations |
| **PASS** | Su produced protocol-visible wireless observations | 3309 AP/global transmission observations |

### [script] Configuration: `Ofdma`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **6395**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 4 | 0.06% | 166.0 B | 0.0 B | 244.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.05% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 4266 | 66.71% | 166.0 B | 0.0 B | 446.1 us | 10.6 us | 5010 MHz | - | 13.0 dBm | 87.81% | 95.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 908 | 14.20% | 167.1 B | 1.8 B | 101.0 us | 16.9 us | 5010 MHz | -66.7 dBm | 13.0 dBm | 4.23% | 4.59% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 133 | 2.08% | 169.7 B | 1.0 B | 128.8 us | 0.5 us | 5010 MHz | -66.1 dBm | 13.0 dBm | 0.79% | 0.86% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.02% | 170.0 B | 0.0 B | 249.3 us | 0.0 us | 5015 MHz | -75.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#18a523" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 39 | 0.61% | 168.4 B | 2.0 B | 919.1 us | 28.2 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 1.65% | 1.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 28 | 0.44% | 168.0 B | 2.0 B | 466.0 us | 23.3 us | 5003 MHz, 5007 MHz, 5013 MHz | -75.0 dBm | - | 0.60% | 0.65% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 9 | 0.14% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.17% | 0.18% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1a620f" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 4 | 0.06% | 34.0 B | 0.0 B | 78.7 us | 0.0 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 0.01% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#07400c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 13 | 0.20% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.13% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d4512" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 12 | 0.19% | 34.0 B | 0.0 B | 126.7 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -75.0 dBm | - | 0.07% | 0.08% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 171 | 2.67% | 53.0 B | 5.9 B | 37.7 us | 2.0 us | 5010 MHz | - | 13.0 dBm | 0.30% | 0.32% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 167 | 2.61% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -66.6 dBm | 13.0 dBm | 0.22% | 0.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 195 | 3.05% | 35.0 B | 7.7 B | 31.7 us | 2.6 us | 5010 MHz | -64.0 dBm | 13.0 dBm | 0.29% | 0.31% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 4 | 0.06% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz, 5015 MHz | -68.0 dBm | - | 0.02% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 411 | 6.43% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -66.7 dBm | - | 3.60% | 3.90% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 12 | 0.19% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.7 dBm | 13.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.09% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.7 dBm | 13.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 12 | 0.19% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -66.7 dBm | 13.0 dBm | 0.04% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=249 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=255 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=261 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.003133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.104048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.106064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=411 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.106064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=417 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.106064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=423 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.106133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.200326000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 15 | 0.200449000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 17 | 0.200545000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 19 | 0.200659000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 21 | 0.200774000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to capture `Ofdma-#0Lan80211AxDlUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `Su`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3309**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 672 | 20.31% | 167.4 B | 1.9 B | 103.9 us | 18.1 us | 5010 MHz | -66.6 dBm | 13.0 dBm | 20.52% | 3.49% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1970 | 59.53% | 166.7 B | 1.5 B | 127.2 us | 0.8 us | 5010 MHz | -66.7 dBm | 13.0 dBm | 73.62% | 12.53% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 319 | 9.64% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -66.7 dBm | 13.0 dBm | 2.62% | 0.45% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 318 | 9.61% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -66.4 dBm | 13.0 dBm | 2.86% | 0.49% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 12 | 0.36% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.7 dBm | 13.0 dBm | 0.09% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.18% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.7 dBm | 13.0 dBm | 0.04% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 12 | 0.36% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -66.7 dBm | 13.0 dBm | 0.24% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 3 | 0.200326000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 5 | 0.200449000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.200572000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 9 | 0.200669000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 11 | 0.200765000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 13 | 0.200879000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 15 | 0.200975000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 17 | 0.201098000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 18 | 0.201314000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 19 | 0.201362000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 20 | 0.201624000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 21 | 0.201672000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 22 | 0.201987000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 23 | 0.202035000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 24 | 0.202316000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=1, seq=0, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `Su-#0Lan80211AxDlUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
The OFDMA condition is expected to show both downlink HE-MU payloads and scheduled uplink HE-TB responses, while the matched SU control disables both multi-user paths. These packet observations establish the exchange structure; use the paired scalar/vector delivery results for application-level comparison.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

The generated representative timeline is the decisive mechanism evidence.
Read it as an AP-coordinated sequence: a Trigger allocates uplink resources,
stations respond as HE-TB transmissions, and the AP uses HE-MU payloads for
downlink service. The decoded allocation table provides the per-user AID/RU
fields where the capture exposes them. Frame numbers are local to the AP
capture and are not OMNeT++ event numbers.

## [agent] Cross-layer findings and verdict

`PASS` for the bounded mechanism claim: the `Ofdma` configuration produces the
expected protocol-visible combination of downlink MU and scheduled uplink
activity, while the matched `Su` control does not. This conclusion is based
on decoded run-0 AP observations, not configuration alone.

`INCONCLUSIVE` for a thresholded performance claim: across five runs, the
OFDMA workload has higher measured goodput and lower measured p95 delay than
the SU baseline, but the suite has no declared acceptance threshold. The
results support the practical observation that this workload benefits from
the configured multi-user exchanges; they do not prove a standards-level or
general performance guarantee.

## [agent] Limitations and inconclusive claims

- The analysis ledger has no executable evaluator for either the bidirectional
  delivery comparison or the exchange-structure requirement. Add an explicit
  acceptance rule to the suite manifest if this walkthrough should become a
  pass/fail regression rather than an evidence-backed demonstration.
- PCAP evidence is representative run 0 only. The five-run scalar/vector
  comparison supports run-level variability, but it does not establish that
  every run contains the same detailed frame sequence.
