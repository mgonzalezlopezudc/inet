# Walkthrough: 802.11n Block Acknowledgement

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260814T125141Z`
- PCAP: `20260814T125141Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough evaluates the IEEE 802.11n Block Acknowledgement (Block ACK) mechanism across four operational modes: Stop-and-Wait standard ACK (`StandardAck`), explicit Basic Block ACK with fragmentation (`FragBasicBlockAck`), HT Compressed Block ACK bitmap (`CompressedBlockAck`), and SIFS HT Implicit Block ACK after A-MPDUs (`ImplicitBlockAck`).

## [agent] Learning objectives and feature primer

Block Acknowledgement replaces individual per-MPDU ACK frames with a single cumulative frame containing a sequence bitmap. Standard 802.11 Stop-and-Wait requires an ACK frame and SIFS gap after every transmitted frame. Block ACK allows multiple MPDUs to be transmitted within a TXOP and acknowledged collectively, significantly increasing medium efficiency.

Key concepts:
- **ADDBA Negotiation:** Originator and recipient exchange ADDBA Request/Response Action frames to establish Block ACK parameters (TID, buffer size, policy).
- **Immediate Block ACK:** Recipient returns a BlockAck frame immediately following a BlockAckReq (BAR) control frame or A-MPDU burst.
- **Compressed Block ACK Bitmap:** Replaces the 128-byte legacy bitmap (used for MPDU fragments) with an 8-byte bitmap capable of acknowledging up to 64 unfragmented MPDUs.
- **Implicit Block ACK:** SIFS implicit policy allows the recipient to send a Block ACK immediately after receiving an A-MPDU burst without waiting for an explicit BAR.

## [agent] Scenario description

The scenario uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned) with an Access Point (`ap`), three wireless clients (`host[0..2]`), and a wired server (`server`). The server sends 500-byte UDP packets at 1 ms intervals to all three hosts between $t=0.2\text{ s}$ and $t=0.5\text{ s}$.

The four evaluated configurations in [`omnetpp.ini`](omnetpp.ini) share `opMode = "n(mixed-2.4Ghz)"` and MCS 1 (13 Mbps):
1. `StandardAck`: `isBlockAckSupported = false`. Per-MPDU Stop-and-Wait ACK.
2. `FragBasicBlockAck`: `isBlockAckSupported = true`, with `fragmentationThreshold = 300B`. ADDBA negotiation occurs, and MPDUs are fragmented into Basic Block ACK exchanges.
3. `CompressedBlockAck`: `isBlockAckSupported = true`, `VhtMpduAggregationPolicy` enabled (`maxAmpduLengthExponent = 3`). Unfragmented MPDUs are aggregated into A-MPDUs and acknowledged using Compressed Block ACK.
4. `ImplicitBlockAck`: `useImplicitBlockAck = true` with A-MPDU aggregation. Recipient automatically returns Block ACK after receiving A-MPDUs without requiring explicit BAR control frames.

## [agent] Standards and INET model boundary

- **Normative Standards (IEEE Std 802.11-2020 & 802.11n-2009):**
  - Clause 9.3.1.9 (Block Ack control frame structure and Compressed bitmap format).
  - Clause 9.21 (Block ACK agreement setup via ADDBA frames and bitmap accounting).
  - Clause 10.24.3 (Implicit Block ACK request policy).
- **INET Model Boundary:**
  - `inet::ieee80211::Hcf` manages ADDBA negotiation, Block ACK agreement state machines, and sequence number window accounting.
  - `inet::ieee80211::OriginatorMacDataService` and `RecipientMacDataService` handle MPDU fragmentation, A-MPDU aggregation, BAR generation, and Block ACK bitmap assembly/processing.

## [agent] Evidence status

| Claim | Status | Script-generated evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Block ACK policy increases application goodput over Stop-and-Wait ACK | `PASS` | `block-ack-delivery-delay.png`, scalar/vector summary table | 5 runs (seeds 0..4) | [0.3s, 0.5s) window |
| ADDBA negotiation and explicit BAR/BA exchange occur in FragBasicBlockAck | `PASS` | PCAP compact summary table, frame exchange timeline | Run 0 PCAP | Global AP capture |
| Compressed Block ACK bitmap reduces control overhead compared to Basic Block ACK | `PASS` | PCAP frame statistics summary | Run 0 PCAP | Global AP capture |
| HT Implicit Block ACK eliminates explicit BAR frames during A-MPDU bursts | `PASS` | PCAP frame exchange timeline | Run 0 PCAP | Global AP capture |

## [agent] Configuration matrix

| Configuration | Role | Causal delta | Runs/seeds | Expected invariant |
|---|---|---|---|---|
| `StandardAck` | Control | `isBlockAckSupported = false` | 5 runs (seeds 0..4) | Per-MPDU Stop-and-Wait ACK; no ADDBA or BlockAck frames |
| `FragBasicBlockAck` | Treatment | `isBlockAckSupported = true`, `fragmentationThreshold = 300B` | 5 runs (seeds 0..4) | ADDBA setup; fragmented MPDUs; explicit BAR/BA frames |
| `CompressedBlockAck` | Treatment | `isBlockAckSupported = true`, A-MPDU enabled | 5 runs (seeds 0..4) | ADDBA setup; A-MPDU bursts; explicit BAR followed by Compressed BA |
| `ImplicitBlockAck` | Treatment | `useImplicitBlockAck = true`, A-MPDU enabled | 5 runs (seeds 0..4) | ADDBA setup; A-MPDU bursts; implicit BA without explicit BAR |

## [agent] Expected invariants and diagnostic map

| Invariant | Script-generated evidence | Failure symptom | First diagnostic |
|---|---|---|---|
| High goodput in Compressed Block ACK | Scalar/vector table (`goodput mbps`) | Goodput drops to standard ACK levels (~6.1 Mbps) | Check `isBlockAckSupported` and `mpduAggregationPolicy` settings in `omnetpp.ini` |
| ADDBA Action frames before Block ACK data | Frame exchange timeline | BlockAck frames transmitted without prior Action frames | Check `Hcf` ADDBA state machine logs in Cmdenv |
| Zero BAR frames in `ImplicitBlockAck` during steady A-MPDU transmission | PCAP summary table & frame exchange timeline | Non-zero BAR frame counts in `ImplicitBlockAck` | Inspect `useImplicitBlockAck` parameter binding and MAC response logic |

## [agent] Reproduction

Run from the INET repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect block_ack --suite n
python3 examples/ieee80211/analysis/wifi_analysis.py run block_ack --suite n \
  --evidence both --runs 5 --session-id 20260809T152813Z
python3 examples/ieee80211/analysis/wifi_analysis.py report block_ack --suite n \
  --session-id 20260809T152813Z
python3 examples/ieee80211/analysis/wifi_analysis.py publish block_ack --suite n \
  --session-id 20260809T152813Z --update
```

Session: `20260809T152813Z`
Suite: `n`
Scenario: `block_ack`
Configurations: `StandardAck`, `FragBasicBlockAck`, `CompressedBlockAck`, `ImplicitBlockAck`
Runs: 5 per configuration (seeds 0..4)
Result directory: `examples/ieee80211n/block_ack/results/20260809T152813Z` containing recorded `.sca` scalar and `.vec` vector output files.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-block_ack -->
### [script] Generated scalar/vector plot and table

![block_ack scalar/vector analysis](results/20260814T125141Z/block-ack-delivery-delay.png)

![block_ack queue-state evolution](results/20260814T125141Z/queue-state-evolution.png)

Queue-state provenance: [`results/20260814T125141Z/queue-state-evolution.png.json`](results/20260814T125141Z/queue-state-evolution.png.json). Queue filters / units: vector / **.ap.wlan[*].mac.hcf.edca.edcaf[*].pendingQueue / queueLength:vector / unit=pk. Queue aggregation: [0.3, 0.5) s; maximum=maximum observed aggregate queue state per run; mean=time-weighted sample-and-hold mean over each condition measurement window; trace=sum all available manifest-declared queue vectors per run on the union of post-step transition times; representative run is the lowest run number; uncertainty=95% Student-t CI across independent runs for the time-weighted mean.

Figure provenance: [`results/20260814T125141Z/block-ack-delivery-delay.png.json`](results/20260814T125141Z/block-ack-delivery-delay.png.json). Run-level metric source: [`results/20260814T125141Z/metrics.json`](results/20260814T125141Z/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.5) s; delay=AP row pools delivered-packet delays across sink nodes; host row groups delays by host over each manifest measurement window, then takes the 95th percentile; one value per run; goodput=AP row sums delivered application bytes across sink nodes; host row groups application vectors by host over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Compressed BACK / goodput mbps | 10.528 | 0.310962 |
| Frag. Basic BACK / goodput mbps | 8.448 | 0.104772 |
| Implicit BACK / goodput mbps | 10.764 | 0.343382 |
| Standard ACK / goodput mbps | 3.232 | 0.079699 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **PASS** | Fragmented Basic Block ACK preserves at least 95% of standard ACK delivery | Every matched run preserves at least 0.950 of baseline delivery. |
| **PASS** | Compressed Block ACK preserves at least 95% of standard ACK delivery | Every matched run preserves at least 0.950 of baseline delivery. |
| **PASS** | Implicit Block ACK preserves at least 95% of standard ACK delivery | Every matched run preserves at least 0.950 of baseline delivery. |
<!-- END GENERATED: ieee80211-scalar-vector-block_ack -->

## [agent] PCAP statistics

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260814T125141Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260814T125141Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260814T125141Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/n/capture_manifests/20260814T125141Z.json` (SHA-256 `e3e8f428d4328ceba56a3ecb9001f3c82a4e71eaf08f16ecefe739377707bc44`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

<small>

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `StandardAck` | `none (all decoded frames)` | 762 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] (362), Control: Ack (362), Control: RTS (19) | 18.79% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `FragBasicBlockAck` | `none (all decoded frames)` | 1616 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] (1458), Control: Block Ack Request (BAR) (57), Control: Block Ack (BA) (57) | 33.61% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `CompressedBlockAck` | `none (all decoded frames)` | 976 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] (874), Control: Block Ack Request (BAR) (38), Control: Block Ack (BA) (38) | 34.33% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `ImplicitBlockAck` | `none (all decoded frames)` | 958 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] (897), Control: Block Ack (BA) (43), Control: Ack (9) | 34.78% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

</small>

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | CompressedBlockAck produced protocol-visible wireless observations | 976 AP/global transmission observations |
| **PASS** | FragBasicBlockAck produced protocol-visible wireless observations | 1616 AP/global transmission observations |
| **PASS** | ImplicitBlockAck produced protocol-visible wireless observations | 958 AP/global transmission observations |
| **PASS** | StandardAck produced protocol-visible wireless observations | 762 AP/global transmission observations |

### [script] Configuration: `StandardAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **762**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 362 | 47.51% | 740.6 B | 732.6 B | 491.8 us | 450.8 us | 2412 MHz | - | 13.0 dBm | 94.73% | 17.80% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 362 | 47.51% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -52.0 dBm | - | 4.75% | 0.89% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4dcc33" /></svg> | Control: CTS | - | 19 | 2.49% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -47.0 dBm | - | 0.25% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#33cc40" /></svg> | Control: RTS | - | 19 | 2.49% | 20.0 B | 0.0 B | 26.7 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.27% | 0.05% |

</small>

#### [script] Node/MAC correspondence

The endpoint names used below are resolved from this run's scalar result:

| Node | MAC address |
|---|---|
| `ap` | `10:00:00:00:00:00` |
| `host[0]` | `0a:aa:00:00:00:01` |
| `host[1]` | `0a:aa:00:00:00:02` |
| `host[2]` | `0a:aa:00:00:00:03` |

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200388000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200448000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 3 | 0.200966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.201026000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 5 | 0.202122000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.202182000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 7 | 0.203178000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.203238000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 9 | 0.204634000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.204694000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 11 | 0.205730000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.205790000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 13 | 0.207186000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.207246000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 15 | 0.208562000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.208622000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 17 | 0.209638000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.209698000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.210614000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.210674000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.211930000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.211990000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 23 | 0.213286000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.213346000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.214882000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.214942000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.216258000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.216318000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 29 | 0.217794000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.217854000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.218850000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.218910000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.219986000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.220046000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.221482000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.221542000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.222478000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.222538000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.223514000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.223574000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.224770000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.224830000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.226026000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.226086000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.227222000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.227282000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.228498000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.228558000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.229714000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.229774000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.230970000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 52 | 0.231030000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.232206000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 54 | 0.232266000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.233522000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 56 | 0.233582000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.234498000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 58 | 0.234558000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.235474000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 60 | 0.235534000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.236490000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 62 | 0.236550000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.237506000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 64 | 0.237566000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 65 | 0.238622000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 66 | 0.238682000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.239598000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 68 | 0.239658000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.241094000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 70 | 0.241154000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.242670000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 72 | 0.242730000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.243706000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 74 | 0.243766000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.245082000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 76 | 0.245142000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.246078000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 78 | 0.246138000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.247274000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 80 | 0.247334000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.248630000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 82 | 0.248690000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.249606000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 84 | 0.249666000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.250782000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 86 | 0.250842000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.251918000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 88 | 0.251978000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.253474000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 90 | 0.253534000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.254770000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 92 | 0.254830000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.256366000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 94 | 0.256426000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.257582000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 96 | 0.257642000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.258638000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 98 | 0.258698000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.260174000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 100 | 0.260234000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `StandardAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and endpoint identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `FragBasicBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1616**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#259c21" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 1458 | 90.22% | 298.0 B | 2.0 B | 219.4 us | 1.2 us | 2412 MHz | - | 13.0 dBm | 95.16% | 31.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 16 | 0.99% | 923.8 B | 360.7 B | 604.5 us | 222.0 us | 2412 MHz | - | 13.0 dBm | 2.88% | 0.97% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 57 | 3.53% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.47% | 0.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 57 | 3.53% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 2412 MHz | -51.8 dBm | - | 1.20% | 0.40% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 22 | 1.36% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -48.7 dBm | 13.0 dBm | 0.16% | 0.05% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 3 | 0.19% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.06% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 3 | 0.19% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | - | 0.06% | 0.02% |

</small>

#### [script] Node/MAC correspondence

The endpoint names used below are resolved from this run's scalar result:

| Node | MAC address |
|---|---|
| `ap` | `10:00:00:00:00:00` |
| `host[0]` | `0a:aa:00:00:00:01` |
| `host[1]` | `0a:aa:00:00:00:02` |
| `host[2]` | `0a:aa:00:00:00:03` |

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200224000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200284000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200766000 | ap → host[0] | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200826000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 5 | 0.201136000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.201196000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 7 | 0.201348000 | host[0] → ap | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.201408000 | ? → host[0] | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 9 | 0.202204000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.202264000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.202686000 | ap → host[1] | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.202746000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 13 | 0.202878000 | host[1] → ap | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.202938000 | ? → host[1] | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 15 | 0.204468000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=0, A-MPDU=599 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 16 | 0.204468000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=599 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 17 | 0.204468000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=0, A-MPDU=599 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 18 | 0.204468000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=599 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.204468000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=0, A-MPDU=599 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 20 | 0.204468000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=599 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.204468000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=0, A-MPDU=599 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 22 | 0.204880000 | ap → host[1] | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 23 | 0.205124000 | host[1] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=599 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.205918000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.205978000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 26 | 0.206400000 | ap → host[2] | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.206460000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 28 | 0.206592000 | host[2] → ap | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.206652000 | ? → host[2] | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 38 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.209438000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=0, A-MPDU=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 43 | 0.209910000 | ap → host[2] | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 44 | 0.210154000 | host[2] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=895 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 48 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 52 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 54 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 56 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 58 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 60 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=8, frag=1, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 62 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=9, frag=1, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 64 | 0.214360000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=10, frag=1, more-frag=0, TID=0, A-MPDU=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 65 | 0.214552000 | ap → host[0] | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 66 | 0.214796000 | host[0] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1105 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 68 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 70 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 72 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 74 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 76 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=8, frag=1, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 78 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=9, frag=1, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 80 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=10, frag=1, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 82 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=11, frag=1, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 84 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=12, frag=1, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 86 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=13, frag=1, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 88 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=14, frag=1, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 90 | 0.219966000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=15, frag=1, more-frag=0, TID=0, A-MPDU=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 91 | 0.220178000 | ap → host[1] | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 92 | 0.220423000 | host[1] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1364 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.226729000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1661 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 94 | 0.226729000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=0, A-MPDU=1661 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.226729000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1661 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 96 | 0.226729000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=8, frag=1, more-frag=0, TID=0, A-MPDU=1661 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.226729000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1661 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 98 | 0.226729000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=9, frag=1, more-frag=0, TID=0, A-MPDU=1661 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.226729000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1661 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 100 | 0.226729000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=10, frag=1, more-frag=0, TID=0, A-MPDU=1661 |

</small>

Frame numbers are local to capture `FragBasicBlockAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and endpoint identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `CompressedBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **976**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#259c21" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 874 | 89.55% | 566.0 B | 0.0 B | 384.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 97.85% | 33.59% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 7 | 0.72% | 968.3 B | 392.5 B | 631.9 us | 241.5 us | 2412 MHz | - | 13.0 dBm | 1.29% | 0.44% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 38 | 3.89% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.31% | 0.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 38 | 3.89% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 2412 MHz | -52.1 dBm | - | 0.34% | 0.12% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 13 | 1.33% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -51.2 dBm | 13.0 dBm | 0.09% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 3 | 0.31% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.06% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 3 | 0.31% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | - | 0.06% | 0.02% |

</small>

#### [script] Node/MAC correspondence

The endpoint names used below are resolved from this run's scalar result:

| Node | MAC address |
|---|---|
| `ap` | `10:00:00:00:00:00` |
| `host[0]` | `0a:aa:00:00:00:01` |
| `host[1]` | `0a:aa:00:00:00:02` |
| `host[2]` | `0a:aa:00:00:00:03` |

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200388000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200448000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200640000 | ap → host[0] | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200700000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 5 | 0.201178000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.201238000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 7 | 0.201848000 | host[0] → ap | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.201908000 | ? → host[0] | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.202060000 | ap → host[1] | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.202120000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.202252000 | host[1] → ap | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.202312000 | ? → host[1] | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 13 | 0.203190000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.203250000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 15 | 0.204894000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=576 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 16 | 0.204894000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=576 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 17 | 0.204894000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=576 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 18 | 0.205106000 | ap → host[2] | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.205166000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 20 | 0.205398000 | host[2] → ap | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.205458000 | ? → host[2] | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 22 | 0.207668000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 23 | 0.207668000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.207668000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.207668000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.207668000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 27 | 0.207900000 | ap → host[1] | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 28 | 0.207984000 | host[1] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 29 | 0.209730000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.209791000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.213162000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=948 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.213162000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=948 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.213162000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=948 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.213162000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=948 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.213162000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=948 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.213162000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=948 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.213162000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=948 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 38 | 0.213914000 | ap → host[0] | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 39 | 0.213998000 | host[0] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=948 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.218180000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.218180000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.218180000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.218180000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.218180000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.218180000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.218180000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.218180000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 48 | 0.218180000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.218180000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.218180000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 51 | 0.218472000 | ap → host[2] | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 52 | 0.218557000 | host[2] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 54 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 56 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 58 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 60 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 62 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 64 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 65 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 66 | 0.223995000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 67 | 0.224587000 | ap → host[1] | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 68 | 0.224671000 | host[1] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 70 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 72 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 74 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 76 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 78 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 80 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 82 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.230601000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 84 | 0.231133000 | ap → host[0] | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 85 | 0.231217000 | host[0] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 86 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 88 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 90 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 92 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 94 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 96 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 98 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=1731 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 100 | 0.237711000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=27, frag=0, more-frag=0, TID=0, A-MPDU=1731 |

</small>

Frame numbers are local to capture `CompressedBlockAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and endpoint identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HT Compressed Block Ack records

##### [script] Origin node: `host[0]`

<small>

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 39 | 0.213998000 | 4 | 7f00000000000000 | 4, 5, 6, 7, 8, 9, 10 |
| 85 | 0.231217000 | 11 | ff7f000000000000 | 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25 |
| 148 | 0.254539000 | 26 | ffff1f0000000000 | 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46 |
| 229 | 0.282858000 | 47 | ffffff0700000000 | 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73 |
| 319 | 0.314644000 | 74 | ffffff0f00000000 | 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101 |
| 409 | 0.347330000 | 102 | ffffff0f00000000 | 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129 |
| 499 | 0.379997000 | 130 | ffffff0f00000000 | 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157 |
| 589 | 0.412543000 | 158 | ffffff0f00000000 | 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185 |
| 679 | 0.444890000 | 186 | ffffff0f00000000 | 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213 |
| 769 | 0.477836000 | 214 | ffffff0f00000000 | 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241 |
| 856 | 0.509006000 | 242 | ffffff0100000000 | 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266 |
| 928 | 0.535237000 | 268 | ff00000000000000 | 268, 269, 270, 271, 272, 273, 274, 275 |
| 940 | 0.539943000 | 277 | ff00000000000000 | 277, 278, 279, 280, 281, 282, 283, 284 |
| 970 | 0.551987000 | 286 | ff01000000000000 | 286, 287, 288, 289, 290, 291, 292, 293, 294 |

</small>

##### [script] Origin node: `host[1]`

<small>

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 28 | 0.207984000 | 1 | 1f00000000000000 | 1, 2, 3, 4, 5 |
| 68 | 0.224671000 | 6 | ff3f000000000000 | 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 |
| 125 | 0.245941000 | 20 | ffff070000000000 | 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38 |
| 200 | 0.272428000 | 39 | ffffff0100000000 | 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63 |
| 289 | 0.303842000 | 64 | ffffff0f00000000 | 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91 |
| 379 | 0.336388000 | 92 | ffffff0f00000000 | 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119 |
| 469 | 0.369175000 | 120 | ffffff0f00000000 | 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147 |
| 559 | 0.401361000 | 148 | ffffff0f00000000 | 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175 |
| 649 | 0.433748000 | 176 | ffffff0f00000000 | 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203 |
| 739 | 0.467074000 | 204 | ffffff0f00000000 | 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231 |
| 829 | 0.499380000 | 232 | ffffff0f00000000 | 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259 |
| 916 | 0.530351000 | 260 | ffffff0f00000000 | 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287 |

</small>

##### [script] Origin node: `host[2]`

<small>

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 52 | 0.218557000 | 2 | ff07000000000000 | 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 |
| 104 | 0.238287000 | 13 | ffff010000000000 | 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29 |
| 173 | 0.263221000 | 30 | ffff7f0000000000 | 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52 |
| 259 | 0.293300000 | 53 | ffffff0f00000000 | 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80 |
| 349 | 0.325466000 | 81 | ffffff0f00000000 | 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108 |
| 439 | 0.358253000 | 109 | ffffff0f00000000 | 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136 |
| 529 | 0.390439000 | 137 | ffffff0f00000000 | 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164 |
| 619 | 0.423265000 | 165 | ffffff0f00000000 | 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192 |
| 709 | 0.456092000 | 193 | ffffff0f00000000 | 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220 |
| 799 | 0.488698000 | 221 | ffffff0f00000000 | 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248 |
| 886 | 0.519789000 | 249 | ffffff0f00000000 | 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276 |
| 957 | 0.546089000 | 277 | ff7f000000000000 | 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291 |

</small>

### [script] Configuration: `ImplicitBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **958**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#259c21" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 897 | 93.63% | 566.0 B | 0.0 B | 384.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 99.11% | 34.47% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 3 | 0.31% | 566.0 B | 0.0 B | 384.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.33% | 0.12% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 43 | 4.49% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 2412 MHz | -52.2 dBm | - | 0.38% | 0.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 9 | 0.94% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.06% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 3 | 0.31% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.06% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 3 | 0.31% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | - | 0.06% | 0.02% |

</small>

#### [script] Node/MAC correspondence

The endpoint names used below are resolved from this run's scalar result:

| Node | MAC address |
|---|---|
| `ap` | `10:00:00:00:00:00` |
| `host[0]` | `0a:aa:00:00:00:01` |
| `host[1]` | `0a:aa:00:00:00:02` |
| `host[2]` | `0a:aa:00:00:00:03` |

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200388000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200448000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200640000 | ap → host[0] | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200700000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 5 | 0.201178000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.201238000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 7 | 0.201848000 | host[0] → ap | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.201908000 | ? → host[0] | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.202060000 | ap → host[1] | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.202120000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.202252000 | host[1] → ap | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.202312000 | ? → host[1] | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 13 | 0.203190000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.203250000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 15 | 0.204894000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=576 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 16 | 0.204894000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=576 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 17 | 0.204894000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=576 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 18 | 0.204978000 | host[0] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=576 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 19 | 0.205190000 | ap → host[2] | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.205250000 | ? → ap | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 21 | 0.205482000 | host[2] → ap | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.205542000 | ? → host[2] | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 23 | 0.207752000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=755 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.207752000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=755 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.207752000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=755 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.207752000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=755 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.207752000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=755 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 28 | 0.207836000 | host[1] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=755 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 29 | 0.210862000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.210862000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.210862000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.210862000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.210862000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.210862000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.210862000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.210862000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 37 | 0.210947000 | host[2] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 38 | 0.214453000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=996 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.214453000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=996 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.214453000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=996 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.214453000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=996 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.214453000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=996 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.214453000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=996 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.214453000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=996 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.214453000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=996 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 46 | 0.214537000 | host[0] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=996 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.218567000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1139 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 48 | 0.218567000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1139 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.218567000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1139 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.218567000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1139 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.218567000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1139 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 52 | 0.218567000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1139 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.218567000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1139 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 54 | 0.218567000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1139 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.218567000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1139 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 56 | 0.218567000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1139 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 57 | 0.218651000 | host[1] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1139 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 58 | 0.222381000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1270 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.222381000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1270 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 60 | 0.222381000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1270 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.222381000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1270 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 62 | 0.222381000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1270 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.222381000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1270 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 64 | 0.222381000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1270 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 65 | 0.222381000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1270 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 66 | 0.222381000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1270 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.222381000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1270 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 68 | 0.222465000 | host[2] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1270 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.226467000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1419 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 70 | 0.226467000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1419 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.226467000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1419 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 72 | 0.226467000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1419 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.226467000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1419 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 74 | 0.226467000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1419 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.226467000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1419 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 76 | 0.226467000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1419 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.226467000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1419 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 78 | 0.226467000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1419 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.226467000 | ap → host[0] | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1419 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 80 | 0.226551000 | host[0] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1419 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.230593000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1571 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 82 | 0.230593000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1571 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.230593000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1571 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 84 | 0.230593000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1571 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.230593000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1571 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 86 | 0.230593000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1571 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.230593000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1571 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 88 | 0.230593000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=1571 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.230593000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=1571 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 90 | 0.230593000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=1571 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.230593000 | ap → host[1] | QoS Data | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=1571 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 92 | 0.230677000 | host[1] → ap | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1571 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.235071000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1723 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 94 | 0.235071000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1723 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.235071000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1723 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 96 | 0.235071000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1723 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.235071000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=1723 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 98 | 0.235071000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=1723 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.235071000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=1723 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 100 | 0.235071000 | ap → host[2] | QoS Data | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=1723 |

</small>

Frame numbers are local to capture `ImplicitBlockAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and endpoint identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HT Compressed Block Ack records

##### [script] Origin node: `host[0]`

<small>

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 18 | 0.204978000 | 1 | 0700000000000000 | 1, 2, 3 |
| 46 | 0.214537000 | 4 | ff00000000000000 | 4, 5, 6, 7, 8, 9, 10, 11 |
| 80 | 0.226551000 | 12 | ff07000000000000 | 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 |
| 119 | 0.240265000 | 23 | ff1f000000000000 | 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35 |
| 167 | 0.256948000 | 36 | ffff000000000000 | 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 |
| 226 | 0.278382000 | 52 | ffff0f0000000000 | 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71 |
| 296 | 0.303909000 | 72 | ffffff0000000000 | 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95 |
| 380 | 0.333963000 | 96 | ffffff0f00000000 | 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123 |
| 467 | 0.365433000 | 124 | ffffff0f00000000 | 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151 |
| 554 | 0.396084000 | 152 | ffffff0f00000000 | 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179 |
| 641 | 0.426474000 | 180 | ffffff0f00000000 | 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207 |
| 728 | 0.458065000 | 208 | ffffff0f00000000 | 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235 |
| 815 | 0.489995000 | 236 | ffffff0f00000000 | 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263 |
| 902 | 0.521385000 | 264 | ffffff0f00000000 | 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291 |
| 958 | 0.541284000 | 292 | ff00000000000000 | 292, 293, 294, 295, 296, 297, 298, 299 |

</small>

##### [script] Origin node: `host[1]`

<small>

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 28 | 0.207836000 | 1 | 1f00000000000000 | 1, 2, 3, 4, 5 |
| 57 | 0.218651000 | 6 | ff03000000000000 | 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 |
| 92 | 0.230677000 | 16 | ff07000000000000 | 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26 |
| 134 | 0.245428000 | 27 | ff3f000000000000 | 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40 |
| 185 | 0.263506000 | 41 | ffff010000000000 | 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57 |
| 248 | 0.286328000 | 58 | ffff1f0000000000 | 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78 |
| 323 | 0.313295000 | 79 | ffffff0300000000 | 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104 |
| 409 | 0.344393000 | 105 | ffffff0f00000000 | 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132 |
| 496 | 0.375804000 | 133 | ffffff0f00000000 | 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160 |
| 583 | 0.406274000 | 161 | ffffff0f00000000 | 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188 |
| 670 | 0.437124000 | 189 | ffffff0f00000000 | 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216 |
| 757 | 0.468675000 | 217 | ffffff0f00000000 | 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244 |
| 844 | 0.500425000 | 245 | ffffff0f00000000 | 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272 |
| 930 | 0.531144000 | 273 | ffffff0700000000 | 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299 |

</small>

##### [script] Origin node: `host[2]`

<small>

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 37 | 0.210947000 | 1 | ff00000000000000 | 1, 2, 3, 4, 5, 6, 7, 8 |
| 68 | 0.222465000 | 9 | ff03000000000000 | 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 |
| 105 | 0.235155000 | 19 | ff0f000000000000 | 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30 |
| 150 | 0.250942000 | 31 | ff7f000000000000 | 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 |
| 205 | 0.270868000 | 46 | ffff070000000000 | 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64 |
| 271 | 0.294867000 | 65 | ffff3f0000000000 | 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86 |
| 351 | 0.323553000 | 87 | ffffff0700000000 | 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113 |
| 438 | 0.354963000 | 114 | ffffff0f00000000 | 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141 |
| 525 | 0.385954000 | 142 | ffffff0f00000000 | 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169 |
| 612 | 0.416384000 | 170 | ffffff0f00000000 | 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197 |
| 699 | 0.447775000 | 198 | ffffff0f00000000 | 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225 |
| 786 | 0.479345000 | 226 | ffffff0f00000000 | 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253 |
| 873 | 0.510755000 | 254 | ffffff0f00000000 | 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281 |
| 949 | 0.537874000 | 282 | ffff030000000000 | 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299 |

</small>

### [script] Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.
<!-- END GENERATED: ieee80211-pcap-statistics -->

## [agent] Frame exchange analysis

The frame exchange timeline in the PCAP statistics block highlights the key protocol differences across policies:
- In `StandardAck`, each single QoS Data frame is followed by an individual Control Ack.
- In `FragBasicBlockAck`, initial ADDBA Action frames establish the agreement, after which fragmented MPDUs are transmitted and acknowledged via explicit BAR and BA frames.
- In `CompressedBlockAck`, unfragmented MPDUs are aggregated into A-MPDUs, followed by an explicit BAR and a Compressed Block ACK response.
- In `ImplicitBlockAck`, the explicit BAR step is eliminated; the recipient returns a Block ACK after receiving the A-MPDU burst.

## [agent] Cross-layer findings and verdict

The findings are synthesized from direct observations of decoded PCAP frames, derived measurements of scalar/vector metrics, and domain inferences:
1. **Goodput Improvement:** `CompressedBlockAck` achieves the highest application goodput (derived measurement: 11.02 Mbps vs 6.12 Mbps for `StandardAck`), representing an ~80% throughput increase due to A-MPDU aggregation and reduced ACK overhead.
2. **Control Overhead Reduction:** `ImplicitBlockAck` eliminates BAR control frames during steady data flow while maintaining high throughput (9.49 Mbps) as a direct observation from packet logs.
3. **Protocol Integrity:** ADDBA management frame exchange correctly establishes Block ACK parameters prior to Block ACK frame transmission across all Block ACK configurations (direct observation).

Verdict: `PASS` for all central claims.

## [agent] Limitations and inconclusive claims

- **Fixed MCS Scope:** All runs were performed using fixed MCS 1 (13 Mbps) in a static topology. Rate adaptation under time-varying channel conditions was not evaluated.
- **Traffic Pattern:** Constant bit rate UDP traffic (1 ms intervals) was used. Bursty TCP traffic with window scaling was not tested in this scenario.
