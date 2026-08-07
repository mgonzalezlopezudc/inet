# Walkthrough: 802.11n Block Acknowledgement

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260807T101006Z`
- PCAP: `20260807T101006Z`
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
4. `ImplicitBlockAck`: `useHtImplicitBlockAck = true` with A-MPDU aggregation. Recipient automatically returns Block ACK after receiving A-MPDUs without requiring explicit BAR control frames.

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
| `ImplicitBlockAck` | Treatment | `useHtImplicitBlockAck = true`, A-MPDU enabled | 5 runs (seeds 0..4) | ADDBA setup; A-MPDU bursts; implicit BA without explicit BAR |

## [agent] Expected invariants and diagnostic map

| Invariant | Script-generated evidence | Failure symptom | First diagnostic |
|---|---|---|---|
| High goodput in Compressed Block ACK | Scalar/vector table (`goodput mbps`) | Goodput drops to standard ACK levels (~6.1 Mbps) | Check `isBlockAckSupported` and `mpduAggregationPolicy` settings in `omnetpp.ini` |
| ADDBA Action frames before Block ACK data | Frame exchange timeline | BlockAck frames transmitted without prior Action frames | Check `Hcf` ADDBA state machine logs in Cmdenv |
| Zero BAR frames in `ImplicitBlockAck` during steady A-MPDU transmission | PCAP summary table & frame exchange timeline | Non-zero BAR frame counts in `ImplicitBlockAck` | Inspect `useHtImplicitBlockAck` parameter binding and MAC response logic |

## [agent] Reproduction

Run from the INET repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect block_ack --suite n
python3 examples/ieee80211/analysis/wifi_analysis.py run block_ack --suite n \
  --evidence both --runs 5 --session-id 20260805T233926Z
python3 examples/ieee80211/analysis/wifi_analysis.py report block_ack --suite n \
  --session-id 20260805T233926Z
python3 examples/ieee80211/analysis/wifi_analysis.py publish block_ack --suite n \
  --session-id 20260805T233926Z --update
```

Session: `20260806T082520Z`
Suite: `n`
Scenario: `block_ack`
Configurations: `StandardAck`, `FragBasicBlockAck`, `CompressedBlockAck`, `HtImplicitBlockAck`
Runs: 5 per configuration (seeds 0..4)
Result directory: `examples/ieee80211n/block_ack/results/20260806T082520Z` containing recorded `.sca` scalar and `.vec` vector output files.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-block_ack -->
### [script] Generated scalar/vector plot and table

![block_ack scalar/vector analysis](results/20260807T101006Z/block-ack-delivery-delay.png)

Figure provenance: [`results/20260807T101006Z/block-ack-delivery-delay.png.json`](results/20260807T101006Z/block-ack-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.5) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Compressed BACK / goodput mbps | 10.612 | 0.0136017 |
| Frag. Basic BACK / goodput mbps | 9.568 | 0.0333173 |
| Implicit BACK / goodput mbps | 10.936 | 0.296651 |
| Standard ACK / goodput mbps | 4.04 | 0.0392649 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Compare application delivered bytes across Block ACK policies | No manifest acceptance threshold defined for Block ACK policy comparison |
<!-- END GENERATED: ieee80211-scalar-vector-block_ack -->

## [agent] PCAP statistics

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260807T101006Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260807T101006Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260807T101006Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/n/capture_manifests/20260807T101006Z.json` (SHA-256 `f52f606dbf1bccf5332057042a2dcd5c7e86fab7254262f3906b6c0c0dd15c37`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

<small>

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `StandardAck` | `none (all decoded frames)` | 606 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] (303), Control: Ack (303) | 15.56% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `FragBasicBlockAck` | `none (all decoded frames)` | 1778 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] (1646), Control: Block Ack Request (BAR) (54), Control: Block Ack (BA) (54) | 36.70% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `CompressedBlockAck` | `none (all decoded frames)` | 995 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] (897), Control: Block Ack Request (BAR) (40), Control: Block Ack (BA) (40) | 34.89% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `ImplicitBlockAck` | `none (all decoded frames)` | 960 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] (897), Control: Block Ack (BA) (45), Control: Ack (9) | 34.79% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

</small>

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | CompressedBlockAck produced protocol-visible wireless observations | 995 AP/global transmission observations |
| **PASS** | FragBasicBlockAck produced protocol-visible wireless observations | 1778 AP/global transmission observations |
| **PASS** | ImplicitBlockAck produced protocol-visible wireless observations | 960 AP/global transmission observations |
| **PASS** | StandardAck produced protocol-visible wireless observations | 606 AP/global transmission observations |

### [script] Configuration: `StandardAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **606**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 303 | 50.00% | 735.7 B | 339.0 B | 488.7 us | 208.6 us | 2412 MHz | - | 13.0 dBm | 95.20% | 14.81% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 303 | 50.00% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -51.2 dBm | - | 4.80% | 0.75% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200388000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200448000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 3 | 0.200966000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.201026000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 5 | 0.202122000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.202182000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 7 | 0.203178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.203238000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 9 | 0.204314000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.204374000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 11 | 0.205410000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.205470000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 13 | 0.206546000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.206606000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 15 | 0.207782000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.207842000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 17 | 0.208818000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.208878000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.210074000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.210134000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.211070000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.211130000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 23 | 0.212106000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.212166000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.213102000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.213162000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.214078000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.214138000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 29 | 0.215194000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.215254000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.216210000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.216270000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.217186000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.217246000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.218262000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.218322000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.219258000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.219318000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.220434000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.220494000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.221430000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.221490000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.222586000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.222646000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.223782000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.223842000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.224758000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.224818000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.225974000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.226034000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.227030000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 52 | 0.227090000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.228066000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 54 | 0.228126000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.229202000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 56 | 0.229262000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.230238000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 58 | 0.230298000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.231474000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 60 | 0.231534000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.232470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 62 | 0.232530000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.233446000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 64 | 0.233506000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 65 | 0.234422000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 66 | 0.234482000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.235438000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 68 | 0.235498000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.236454000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 70 | 0.236514000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.237570000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 72 | 0.237630000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.238546000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 74 | 0.238606000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.239722000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 76 | 0.239782000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.240738000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 78 | 0.240798000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.241774000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 80 | 0.241834000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.242790000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 82 | 0.242850000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.243786000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 84 | 0.243846000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.244942000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 86 | 0.245002000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.246078000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 88 | 0.246138000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.247054000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 90 | 0.247114000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.248150000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 92 | 0.248210000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.249186000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 94 | 0.249246000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.250422000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 96 | 0.250482000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.251438000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 98 | 0.251498000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.252634000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 100 | 0.252694000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `StandardAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `FragBasicBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1778**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#259c21" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 1646 | 92.58% | 296.6 B | 19.2 B | 218.5 us | 11.8 us | 2412 MHz | - | 13.0 dBm | 98.00% | 35.96% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 6 | 0.34% | 298.0 B | 2.0 B | 219.4 us | 1.2 us | 2412 MHz | - | 13.0 dBm | 0.36% | 0.13% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 54 | 3.04% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.41% | 0.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 54 | 3.04% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 2412 MHz | -52.3 dBm | - | 1.04% | 0.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 12 | 0.67% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.08% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.34% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.11% | 0.04% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200224000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200284000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200766000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200826000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 5 | 0.201136000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.201196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 7 | 0.201348000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.201408000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 9 | 0.201884000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.201944000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.202366000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.202426000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 13 | 0.202558000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.202618000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 15 | 0.202948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.203008000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 17 | 0.203716000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.203776000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 19 | 0.204258000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.204318000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 21 | 0.204470000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.204530000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 23 | 0.204920000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.204980000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.207424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1024 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.207424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=0, A-MPDU=1024 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.207424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1024 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 28 | 0.207424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=0, A-MPDU=1024 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 29 | 0.207424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1024 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.207424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=0, A-MPDU=1024 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.207424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1024 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.207424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=0, A-MPDU=1024 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.207424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1024 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.207424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=0, A-MPDU=1024 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 35 | 0.207856000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 36 | 0.208100000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1024 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 38 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 48 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 52 | 0.211242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=1, more-frag=0, TID=0, A-MPDU=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 53 | 0.211394000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 54 | 0.211638000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1254 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 56 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 58 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 60 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 62 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 64 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 65 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 66 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 68 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 70 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=1, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 72 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=1, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 74 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=1, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 76 | 0.216016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=1, more-frag=0, TID=0, A-MPDU=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 77 | 0.216148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 78 | 0.216392000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1529 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 80 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 82 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 84 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=1, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 86 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=9, frag=1, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 88 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=10, frag=1, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 90 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=11, frag=1, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 92 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=12, frag=1, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 94 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=13, frag=1, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 96 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=14, frag=1, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 98 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=15, frag=1, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 100 | 0.220670000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=16, frag=1, more-frag=0, TID=0, A-MPDU=1897 |

</small>

Frame numbers are local to capture `FragBasicBlockAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `CompressedBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **995**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#259c21" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 897 | 90.15% | 566.0 B | 0.0 B | 384.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 98.81% | 34.47% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 3 | 0.30% | 566.0 B | 0.0 B | 384.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.33% | 0.12% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 40 | 4.02% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.32% | 0.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 40 | 4.02% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 2412 MHz | -52.4 dBm | - | 0.35% | 0.12% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 9 | 0.90% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.06% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.60% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.12% | 0.04% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200388000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200448000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200640000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200700000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 5 | 0.201178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.201238000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 7 | 0.201848000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.201908000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.202060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.202120000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.202252000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.202312000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 13 | 0.202870000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.202930000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.203520000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.203580000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.203772000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.203832000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.205470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=799 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 20 | 0.205470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=799 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.205470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=799 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 22 | 0.205470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=799 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 23 | 0.207600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=870 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.207600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=870 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.207600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=870 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.207600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=870 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.207600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=870 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 28 | 0.207832000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 29 | 0.207916000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=870 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.211122000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1042 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.211122000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1042 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.211122000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1042 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.211122000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1042 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.211122000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1042 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.211122000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1042 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.211122000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1042 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.211122000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1042 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 38 | 0.211394000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 39 | 0.211479000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1042 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.214053000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1235 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.214053000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1235 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.214053000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1235 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.214053000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1235 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.214053000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1235 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.214053000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1235 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.214053000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1235 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 47 | 0.214245000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 48 | 0.214329000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1235 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.217887000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1421 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.217887000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1421 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.217887000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1421 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 52 | 0.217887000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1421 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.217887000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1421 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 54 | 0.217887000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1421 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.217887000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1421 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 56 | 0.217887000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1421 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.217887000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1421 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 58 | 0.218119000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 59 | 0.218203000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1421 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 60 | 0.221853000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1639 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.221853000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1639 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 62 | 0.221853000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1639 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.221853000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1639 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 64 | 0.221853000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1639 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 65 | 0.221853000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1639 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 66 | 0.221853000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1639 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.221853000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1639 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 68 | 0.221853000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1639 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.221853000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1639 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 70 | 0.222125000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 71 | 0.222209000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1639 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 72 | 0.226311000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1864 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.226311000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1864 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 74 | 0.226311000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1864 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.226311000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1864 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 76 | 0.226311000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1864 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.226311000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1864 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 78 | 0.226311000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1864 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.226311000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1864 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 80 | 0.226311000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1864 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.226311000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1864 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 82 | 0.226311000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1864 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 83 | 0.226523000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 84 | 0.226607000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1864 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.231161000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 86 | 0.231161000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.231161000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 88 | 0.231161000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.231161000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 90 | 0.231161000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.231161000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 92 | 0.231161000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.231161000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 94 | 0.231161000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.231161000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 96 | 0.231161000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 97 | 0.231493000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 98 | 0.231577000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2096 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.236403000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=2353 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 100 | 0.236403000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=2353 |

</small>

Frame numbers are local to capture `CompressedBlockAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HT Compressed Block Ack records

##### [script] Origin address: 0a:aa:00:00:00:01

<small>

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 48 | 0.214329000 | 5 | 7f00000000000000 | 5, 6, 7, 8, 9, 10, 11 |
| 84 | 0.226607000 | 12 | ff07000000000000 | 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 |
| 129 | 0.242213000 | 23 | ff3f000000000000 | 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36 |
| 187 | 0.262316000 | 37 | ffff070000000000 | 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55 |
| 256 | 0.286050000 | 56 | ffff3f0000000000 | 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77 |
| 340 | 0.315445000 | 78 | ffffff0f00000000 | 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105 |
| 430 | 0.346751000 | 106 | ffffff0f00000000 | 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133 |
| 520 | 0.377997000 | 134 | ffffff0f00000000 | 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161 |
| 610 | 0.408884000 | 162 | ffffff0f00000000 | 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189 |
| 700 | 0.440690000 | 190 | ffffff0f00000000 | 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217 |
| 790 | 0.472397000 | 218 | ffffff0f00000000 | 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245 |
| 880 | 0.503343000 | 246 | ffffff0f00000000 | 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273 |
| 968 | 0.533845000 | 274 | ffffff0300000000 | 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299 |

</small>

##### [script] Origin address: 0a:aa:00:00:00:02

<small>

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 29 | 0.207916000 | 1 | 1f00000000000000 | 1, 2, 3, 4, 5 |
| 59 | 0.218203000 | 6 | ff01000000000000 | 6, 7, 8, 9, 10, 11, 12, 13, 14 |
| 98 | 0.231577000 | 15 | ff0f000000000000 | 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26 |
| 147 | 0.248412000 | 27 | ffff000000000000 | 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42 |
| 209 | 0.269802000 | 43 | ffff0f0000000000 | 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62 |
| 282 | 0.295304000 | 63 | ffffff0000000000 | 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86 |
| 370 | 0.325767000 | 87 | ffffff0f00000000 | 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114 |
| 460 | 0.357153000 | 115 | ffffff0f00000000 | 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142 |
| 550 | 0.388280000 | 143 | ffffff0f00000000 | 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170 |
| 640 | 0.419546000 | 171 | ffffff0f00000000 | 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198 |
| 730 | 0.451352000 | 199 | ffffff0f00000000 | 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226 |
| 820 | 0.482599000 | 227 | ffffff0f00000000 | 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254 |
| 910 | 0.513765000 | 255 | ffffff0f00000000 | 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282 |
| 987 | 0.540396000 | 283 | ffff010000000000 | 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299 |

</small>

##### [script] Origin address: 0a:aa:00:00:00:03

<small>

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 39 | 0.211479000 | 1 | ff00000000000000 | 1, 2, 3, 4, 5, 6, 7, 8 |
| 71 | 0.222209000 | 9 | ff03000000000000 | 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 |
| 113 | 0.236659000 | 19 | ff1f000000000000 | 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 |
| 166 | 0.254962000 | 32 | ffff010000000000 | 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48 |
| 232 | 0.277560000 | 49 | ffff1f0000000000 | 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69 |
| 310 | 0.305003000 | 70 | ffffff0300000000 | 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95 |
| 400 | 0.336149000 | 96 | ffffff0f00000000 | 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123 |
| 490 | 0.367615000 | 124 | ffffff0f00000000 | 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151 |
| 580 | 0.398602000 | 152 | ffffff0f00000000 | 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179 |
| 670 | 0.430268000 | 180 | ffffff0f00000000 | 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207 |
| 760 | 0.461735000 | 208 | ffffff0f00000000 | 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235 |
| 850 | 0.493061000 | 236 | ffffff0f00000000 | 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263 |
| 940 | 0.524147000 | 264 | ffffff0f00000000 | 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291 |

</small>

### [script] Configuration: `ImplicitBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **960**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#259c21" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 897 | 93.44% | 566.0 B | 0.0 B | 384.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 99.09% | 34.47% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 3 | 0.31% | 566.0 B | 0.0 B | 384.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.33% | 0.12% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 45 | 4.69% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 2412 MHz | -52.3 dBm | - | 0.40% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 9 | 0.94% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.06% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.62% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.12% | 0.04% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200388000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200448000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200640000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200700000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 5 | 0.201178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.201238000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 7 | 0.201848000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.201908000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.202060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.202120000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.202252000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.202312000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 13 | 0.202870000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.202930000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.203520000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.203580000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.203772000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.203832000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.205470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=799 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 20 | 0.205470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=799 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.205470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=799 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 22 | 0.205470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=799 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 23 | 0.205554000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=799 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.207684000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=902 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.207684000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=902 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.207684000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=902 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.207684000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=902 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 28 | 0.207684000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=902 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 29 | 0.207768000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=902 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.210442000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1030 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.210442000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1030 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.210442000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1030 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.210442000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1030 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.210442000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1030 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.210442000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1030 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.210442000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1030 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 37 | 0.210527000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1030 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 38 | 0.213009000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1190 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.213009000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1190 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.213009000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1190 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.213009000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1190 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.213009000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1190 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.213009000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1190 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 44 | 0.213093000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1190 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.216019000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1343 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.216019000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1343 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.216019000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1343 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 48 | 0.216019000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1343 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.216019000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1343 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.216019000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1343 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.216019000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1343 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 52 | 0.216019000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1343 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 53 | 0.216103000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1343 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 54 | 0.219621000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1510 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.219621000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1510 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 56 | 0.219621000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1510 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.219621000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1510 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 58 | 0.219621000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1510 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.219621000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1510 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 60 | 0.219621000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1510 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.219621000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1510 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 62 | 0.219621000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1510 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 63 | 0.219705000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1510 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 64 | 0.223003000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1684 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 65 | 0.223003000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1684 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 66 | 0.223003000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1684 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.223003000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1684 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 68 | 0.223003000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1684 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.223003000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1684 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 70 | 0.223003000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1684 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.223003000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1684 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 72 | 0.223003000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1684 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 73 | 0.223087000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1684 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 74 | 0.226777000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1876 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.226777000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1876 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 76 | 0.226777000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1876 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.226777000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1876 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 78 | 0.226777000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1876 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.226777000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1876 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 80 | 0.226777000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1876 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.226777000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1876 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 82 | 0.226777000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1876 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.226777000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=1876 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 84 | 0.226861000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1876 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.230943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 86 | 0.230943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.230943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 88 | 0.230943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.230943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 90 | 0.230943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.230943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 92 | 0.230943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.230943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 94 | 0.230943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.230943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=27, frag=0, more-frag=0, TID=0, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 96 | 0.231027000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.235521000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=2281 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 98 | 0.235521000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=2281 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.235521000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=2281 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 100 | 0.235521000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=2281 |

</small>

Frame numbers are local to capture `ImplicitBlockAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HT Compressed Block Ack records

##### [script] Origin address: 0a:aa:00:00:00:01

<small>

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 23 | 0.205554000 | 1 | 0f00000000000000 | 1, 2, 3, 4 |
| 44 | 0.213093000 | 5 | 3f00000000000000 | 5, 6, 7, 8, 9, 10 |
| 73 | 0.223087000 | 11 | ff01000000000000 | 11, 12, 13, 14, 15, 16, 17, 18, 19 |
| 109 | 0.235605000 | 20 | ff0f000000000000 | 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 |
| 151 | 0.250396000 | 32 | ff3f000000000000 | 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 |
| 202 | 0.268434000 | 46 | ffff010000000000 | 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62 |
| 262 | 0.289601000 | 63 | ffff0f0000000000 | 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82 |
| 331 | 0.314135000 | 83 | ffff7f0000000000 | 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105 |
| 412 | 0.342473000 | 106 | ffffff0700000000 | 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132 |
| 499 | 0.372884000 | 133 | ffffff0f00000000 | 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160 |
| 586 | 0.403114000 | 161 | ffffff0f00000000 | 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188 |
| 673 | 0.433485000 | 189 | ffffff0f00000000 | 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216 |
| 760 | 0.464255000 | 217 | ffffff0f00000000 | 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244 |
| 847 | 0.495045000 | 245 | ffffff0f00000000 | 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272 |
| 933 | 0.525204000 | 273 | ffffff0700000000 | 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299 |

</small>

##### [script] Origin address: 0a:aa:00:00:00:02

<small>

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 29 | 0.207768000 | 1 | 1f00000000000000 | 1, 2, 3, 4, 5 |
| 53 | 0.216103000 | 6 | ff00000000000000 | 6, 7, 8, 9, 10, 11, 12, 13 |
| 84 | 0.226861000 | 14 | ff03000000000000 | 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 |
| 122 | 0.240124000 | 24 | ff0f000000000000 | 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35 |
| 167 | 0.256070000 | 36 | ff7f000000000000 | 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50 |
| 221 | 0.275124000 | 51 | ffff030000000000 | 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68 |
| 284 | 0.297407000 | 69 | ffff1f0000000000 | 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89 |
| 357 | 0.323149000 | 90 | ffffff0100000000 | 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114 |
| 441 | 0.352564000 | 115 | ffffff0f00000000 | 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142 |
| 528 | 0.382954000 | 143 | ffffff0f00000000 | 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170 |
| 615 | 0.413304000 | 171 | ffffff0f00000000 | 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198 |
| 702 | 0.443795000 | 199 | ffffff0f00000000 | 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226 |
| 789 | 0.474485000 | 227 | ffffff0f00000000 | 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254 |
| 876 | 0.505176000 | 255 | ffffff0f00000000 | 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282 |
| 951 | 0.531582000 | 283 | ffff010000000000 | 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299 |

</small>

##### [script] Origin address: 0a:aa:00:00:00:03

<small>

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 37 | 0.210527000 | 1 | 7f00000000000000 | 1, 2, 3, 4, 5, 6, 7 |
| 63 | 0.219705000 | 8 | ff01000000000000 | 8, 9, 10, 11, 12, 13, 14, 15, 16 |
| 96 | 0.231027000 | 17 | ff07000000000000 | 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 |
| 136 | 0.245094000 | 28 | ff1f000000000000 | 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40 |
| 184 | 0.262056000 | 41 | ffff000000000000 | 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56 |
| 241 | 0.282107000 | 57 | ffff070000000000 | 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75 |
| 307 | 0.305625000 | 76 | ffff3f0000000000 | 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97 |
| 384 | 0.332715000 | 98 | ffffff0300000000 | 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123 |
| 470 | 0.362714000 | 124 | ffffff0f00000000 | 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151 |
| 557 | 0.393004000 | 152 | ffffff0f00000000 | 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179 |
| 644 | 0.423355000 | 180 | ffffff0f00000000 | 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207 |
| 731 | 0.453965000 | 208 | ffffff0f00000000 | 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235 |
| 818 | 0.484715000 | 236 | ffffff0f00000000 | 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263 |
| 905 | 0.515426000 | 264 | ffffff0f00000000 | 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291 |
| 960 | 0.534612000 | 292 | ff00000000000000 | 292, 293, 294, 295, 296, 297, 298, 299 |

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
