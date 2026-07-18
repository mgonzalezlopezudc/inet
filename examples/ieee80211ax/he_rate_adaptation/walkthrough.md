# 802.11ax HE Rate Adaptation Simulation

This example illustrates the High Efficiency (HE) Rate Adaptation mechanisms in the IEEE 802.11ax (Wi-Fi 6) standard. It showcases the difference between a static SNR-based MCS selection fallback and the dynamic, feedback-driven **HE Minstrel** rate adaptation algorithm in a multi-user downlink scheduling scenario.

## Background: HE Rate Selection & Minstrel Rate Control

Selecting the optimal Modulation and Coding Scheme (MCS) is critical in wireless networks. A rate too high causes packet corruption, while a rate too low wastes channel airtime. In 802.11ax, this choice is further integrated with the downlink scheduler:

1. **Static SNR-Based Fallback (SNR Mapping)**:
   - When no dynamic rate control is active, the HCF Downlink Scheduler uses a static mapping.
   - It estimates the path loss to the destination, calculates the expected SNR, and matches it against a configured table of thresholds (`heMcsSnrThresholds` parameter, default: `"4 7 10 13 16 19 21 24 27 30 33 36"` dB for MCS 0..11).

2. **HE Minstrel Rate Control (`HeMinstrelRateControl`)**:
   - Inspired by the classic Minstrel algorithm for legacy 802.11, this is a feedback-driven rate controller.
   - It maintains an **Exponentially Weighted Moving Average (EWMA)** of frame transmission success probabilities for each peer station and each possible MCS/NSS combination.
   - It periodically schedules **probe frames** (controlled by `lookaroundRatio`) to test other rates, dynamically adapting to actual link conditions (such as fading, shadowing, and collision levels) based on received ACKs and Block ACKs.

---

## Network Topology

The network [HeRateAdaptationNetwork.ned](HeRateAdaptationNetwork.ned) consists of:
- **`ap`**: An Access Point located at `(320, 210)`.
- **`host[0..3]`**: Four wireless stations placed at varying distances from the AP:
  - `host[0]` at 70m (`(250, 210)`) -> closest, high SNR.
  - `host[1]` at 130m (`(200, 160)`).
  - `host[2]` at 177m (`(150, 260)`).
  - `host[3]` at 230m (`(90, 210)`) -> furthest, low SNR.
- **`server`**: A wired server connected to the AP.
- **Traffic**: Downlink UDP traffic is sent from the `server` to each of the four hosts via the AP (900B packets sent every 0.35ms). The common warm-up is `0.2–0.25s`, normal traffic starts at `0.3s`, and rate analysis starts at `0.5s` to allow controller settling.

```
  [host[3]]       [host[2]]       [host[1]]       [host[0]]      [ap] <== (wired) ==> [server]
    230m            177m            130m            70m
```

---

## Configurations in `omnetpp.ini`

The [omnetpp.ini](omnetpp.ini) file defines three scenarios:

### 1. `FixedMcs` (Baseline)
- The AP's HCF Downlink Scheduler does not use a dynamic rate control module.
- Instead, it falls back to the static path-loss SNR mapping to choose the transmission MCS for each station.

### 2. `HeMinstrel`
- Dynamic HE Minstrel rate control is enabled on the AP:
  - `**.ap.wlan[*].mac.hcf.rateControl.typename = "HeMinstrelRateControl"`
  - `**.ap.wlan[*].mac.hcf.rateControl.minMcs = 0`
  - `**.ap.wlan[*].mac.hcf.rateControl.maxMcs = 11`
  - `**.ap.wlan[*].mac.hcf.dlScheduler.heRateControlModule = "^.rateControl"`
- **Result**: The Downlink Scheduler queries the `HeMinstrelRateControl` module to select the optimal MCS/NSS dynamically for each peer, updating its selection based on ACK success rates.

### 3. `HeMinstrelMobile`

- Extends `HeMinstrel` and changes only `host[3]` to `LinearMobility` at
  `40 m/s`.
- This is the didactically useful adaptation case: the link budget changes
  during the run, so inspect the selected-rate vector over simulation time
  rather than comparing only its mean or the final packet count.

---

## Running the Simulation

From the INET project root, use the project launcher.

### Running with Qtenv (GUI)
```sh
bin/inet -u Qtenv -c HeMinstrel examples/ieee80211ax/he_rate_adaptation/omnetpp.ini
```

### Running with Cmdenv (Command Line)
```sh
# Run FixedMcs Baseline
bin/inet -u Cmdenv -c FixedMcs examples/ieee80211ax/he_rate_adaptation/omnetpp.ini

# Run HeMinstrel Config
bin/inet -u Cmdenv -c HeMinstrel examples/ieee80211ax/he_rate_adaptation/omnetpp.ini

# Run Minstrel with a moving edge station
bin/inet -u Cmdenv -c HeMinstrelMobile examples/ieee80211ax/he_rate_adaptation/omnetpp.ini
```

---

## Verifying Results

After running the simulations, use `opp_scavetool` to analyze the received packets at the hosts and the selected transmission bitrates of the AP.

```sh
# Query the total packets received at the UDP applications on host[0..3]
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.host*app*"' examples/ieee80211ax/he_rate_adaptation/results/*.sca

# Query the selected datarate statistics for transmissions by the AP HCF
opp_scavetool query -l -f 'name =~ "datarateSelected:vector"' examples/ieee80211ax/he_rate_adaptation/results/*.vec
```

### Vector summary

The five-run `HeMinstrelMobile` campaign selects MCS 0 through 11, with
`9.372 ± 3.830 Mbps` goodput and a `0.980 ± 0.010` transmission-success
fraction. The selected-MCS and transmission-outcome vectors together are the
evidence for useful adaptation; a changing MCS by itself could merely show
probing or instability.

---

## PCAP Tshark Packet Exchange Analysis

To record PCAP traces and inspect them with TShark, run the simulation with PCAP recording and checksum computation enabled:

```sh
bin/inet -u Cmdenv -c HeMinstrel examples/ieee80211ax/he_rate_adaptation/omnetpp.ini --result-dir=examples/ieee80211ax/he_rate_adaptation/results --**.numPcapRecorders=1 --**.checksumMode=\"computed\" --**.fcsMode=\"computed\"
```

Use TShark to print the timeline of packet exchanges:

```sh
tshark -n -r examples/ieee80211ax/he_rate_adaptation/results/HeMinstrel-#0HeRateAdaptationNetwork.ap.wlan[0].pcap -c 20
```

The decoded output timeline shows:
1. **Downlink UDP Packets**: The AP transmits UDP data packets (e.g. frames 1, 7, 13) to client hosts at distance-adapted MCS rates.
2. **Block Ack Negotiation**: Block ACK negotiation Action frames (e.g. frames 3, 5, 9, 11, 15) are exchanged between the AP and the client hosts to establish session block acknowledgments.
3. **Minstrel Dynamic Adaptation (HeMinstrelMobile)**: Under mobility (in `HeMinstrelMobile`), you can observe the AP dynamically reducing the transmission MCS and datarates for `host[3]` as it moves further away and its SNR decreases, maintaining high packet delivery success rates.

---

## Interpretation of Results

1. **Observed adaptation**:
   - The selected-MCS vector spans MCS 0 through 11 while the transmission-outcome vector remains highly successful (`0.980 ± 0.010`). This pairing is the evidence for adaptation; selected rates alone would not establish useful delivery.

2. **Why the moving edge station is the teaching case**:
   - A stationary strong link would let both static and adaptive selection stay
     near one rate. Moving `host[3]` at `40 m/s` makes the link budget change
     enough during a two-second run to traverse MCS 0 through 11.
   - Traffic begins at `0.3 s`, but analysis starts at `0.5 s` so controller
     initialization and probing are not mistaken for steady adaptation.
   - The `0.980 ± 0.010` success fraction shows the controller changes rates
     without simply trading goodput for widespread loss. Rate adaptation is an
     implementation policy available to earlier Wi-Fi too; the HE-specific
     benefit demonstrated here is selecting within the larger 802.11ax MCS,
     NSS, RU, and PPDU-format envelope.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
## 802.11 Packet Type Statistics
![802.11 Packet Type Statistics](packet_statistics.png)

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260718T132413Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %**: The sum of this frame type's estimated airtimes divided by the simulation time limit. Concurrent transmissions from multiple capture points are counted separately, so this value can exceed 100%; it is not the union of busy channel time.

### Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | FixedMcs produced protocol-visible wireless observations | 2012 AP/global transmission observations |
| **PASS** | HeMinstrel produced protocol-visible wireless observations | 1990 AP/global transmission observations |
| **PASS** | HeMinstrelMobile produced protocol-visible wireless observations | 3807 AP/global transmission observations |
| **INCONCLUSIVE** | Selected MCS/NSS, EWMA outcome and retries | The packet-type table is exchange evidence only; use the recorded feature vectors/results |

### Configuration: `FixedMcs`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2012**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 11 | 0.55% | 2966.0 B | 0.0 B | 3280.9 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 2.26% | 1.80% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 57 | 2.83% | 966.0 B | 0.0 B | 1064.4 us | 14.7 us | 5010 MHz | - | 13.0 dBm | 3.80% | 3.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 1345 | 66.85% | 966.0 B | 0.0 B | 1092.8 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 92.12% | 73.49% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d28a04" /></svg> | Control: Trigger [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 11 | 0.55% | 46.0 B | 0.0 B | 39.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.03% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c88037" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 99 | 4.92% | 24.0 B | 0.0 B | 37.6 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.23% | 0.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#11289c" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 94 | 4.67% | 32.0 B | 0.0 B | 38.1 us | 0.0 us | 5010 MHz | -74.9 dBm | - | 0.22% | 0.18% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0639bc" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 21 | 1.04% | 32.0 B | 0.0 B | 116.3 us | 0.0 us | 5005 MHz, 5015 MHz | -73.8 dBm | - | 0.15% | 0.12% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2098f3" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 348 | 17.30% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5010 MHz | -80.9 dBm | - | 1.12% | 0.89% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3598e3" /></svg> | Control: Ack [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 6 | 0.30% | 14.0 B | 0.0 B | 36.9 us | 0.0 us | 5010 MHz | -74.7 dBm | 13.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c71b0f" /></svg> | Management: Action [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 20 | 0.99% | 37.0 B | 0.0 B | 38.4 us | 0.0 us | 5010 MHz | -74.7 dBm | 13.0 dBm | 0.05% | 0.04% |

### Configuration: `HeMinstrel`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1990**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 6 | 0.30% | 2966.0 B | 0.0 B | 3280.9 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 1.24% | 0.98% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 70 | 3.52% | 966.0 B | 0.0 B | 1064.0 us | 14.4 us | 5010 MHz | - | 13.0 dBm | 4.70% | 3.72% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 1338 | 67.24% | 966.0 B | 0.0 B | 1092.8 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 92.33% | 73.11% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d28a04" /></svg> | Control: Trigger [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 6 | 0.30% | 46.0 B | 0.0 B | 39.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c88037" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 95 | 4.77% | 24.0 B | 0.0 B | 37.6 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.23% | 0.18% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#11289c" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 92 | 4.62% | 32.0 B | 0.0 B | 38.1 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.22% | 0.18% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0639bc" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 11 | 0.55% | 32.0 B | 0.0 B | 116.3 us | 0.0 us | 5005 MHz, 5015 MHz | -73.6 dBm | - | 0.08% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2098f3" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 341 | 17.14% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5010 MHz | -80.9 dBm | - | 1.10% | 0.87% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#5a9ae2" /></svg> | Control: Ack [HE-SU, HE-MCS 10, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.05% | 14.0 B | 0.0 B | 37.0 us | 0.0 us | 5010 MHz | -81.0 dBm | - | 0.00% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3598e3" /></svg> | Control: Ack [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 6 | 0.30% | 14.0 B | 0.0 B | 36.9 us | 0.0 us | 5010 MHz | -74.7 dBm | 13.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2a87e5" /></svg> | Control: Ack [HE-SU, HE-MCS 5, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.05% | 14.0 B | 0.0 B | 37.9 us | 0.0 us | 5010 MHz | -81.0 dBm | - | 0.00% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e7182d" /></svg> | Management: Action [HE-SU, HE-MCS 10, 20 MHz, GI 3.2 us, BCC] | 1 | 0.05% | 37.0 B | 0.0 B | 38.7 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.00% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c71b0f" /></svg> | Management: Action [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 21 | 1.06% | 37.0 B | 0.0 B | 38.4 us | 0.0 us | 5010 MHz | -79.9 dBm | 13.0 dBm | 0.05% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5170f" /></svg> | Management: Action [HE-SU, HE-MCS 5, 20 MHz, GI 3.2 us, BCC] | 1 | 0.05% | 37.0 B | 0.0 B | 41.1 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.00% | 0.00% |

### Configuration: `HeMinstrelMobile`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3807**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 707 | 18.57% | 2966.0 B | 0.0 B | 3280.9 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 71.69% | 115.98% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 57 | 1.50% | 966.0 B | 0.0 B | 1063.8 us | 14.2 us | 5010 MHz | - | 13.0 dBm | 1.87% | 3.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 593 | 15.58% | 966.0 B | 0.0 B | 1092.8 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 20.03% | 32.40% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d28a04" /></svg> | Control: Trigger [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 707 | 18.57% | 46.0 B | 0.0 B | 39.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.85% | 1.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c88037" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 52 | 1.37% | 24.0 B | 0.0 B | 37.6 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.06% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#11289c" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 46 | 1.21% | 32.0 B | 0.0 B | 38.1 us | 0.0 us | 5010 MHz | -76.2 dBm | - | 0.05% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0639bc" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 1413 | 37.12% | 32.0 B | 0.0 B | 116.3 us | 0.0 us | 5005 MHz, 5015 MHz | -73.0 dBm | - | 5.08% | 8.22% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2098f3" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 197 | 5.17% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5010 MHz | -81.9 dBm | - | 0.31% | 0.51% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3598e3" /></svg> | Control: Ack [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 6 | 0.16% | 14.0 B | 0.0 B | 36.9 us | 0.0 us | 5010 MHz | -74.7 dBm | 13.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#67aae9" /></svg> | Control: Ack [HE-SU, HE-MCS 8, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.03% | 14.0 B | 0.0 B | 37.3 us | 0.0 us | 5010 MHz | -82.0 dBm | - | 0.00% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#41aaec" /></svg> | Control: Ack [HE-SU, HE-MCS 9, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.03% | 14.0 B | 0.0 B | 37.1 us | 0.0 us | 5010 MHz | -81.0 dBm | - | 0.00% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e7182d" /></svg> | Management: Action [HE-SU, HE-MCS 10, 20 MHz, GI 3.2 us, BCC] | 2 | 0.05% | 37.0 B | 0.0 B | 38.7 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.00% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c71b0f" /></svg> | Management: Action [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 23 | 0.60% | 37.0 B | 0.0 B | 38.4 us | 0.0 us | 5010 MHz | -80.4 dBm | 13.0 dBm | 0.03% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d11016" /></svg> | Management: Action [HE-SU, HE-MCS 8, 20 MHz, GI 3.2 us, BCC] | 1 | 0.03% | 37.0 B | 0.0 B | 39.4 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.00% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f4371a" /></svg> | Management: Action [HE-SU, HE-MCS 9, 20 MHz, GI 3.2 us, BCC] | 1 | 0.03% | 37.0 B | 0.0 B | 39.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.00% | 0.00% |

### Analysis of Packet Distribution
IEEE 802.11 constrains negotiated HE modes but does not mandate a Minstrel algorithm. These packet counts therefore cannot establish adaptation, and a control/data ratio is not reliable evidence of retransmission or probing. Use the aligned selected-MCS/NSS, EWMA probability, transmission-outcome, and retry vectors documented above. INET's HE Minstrel remains a simplified implementation without scheduler-context or localized-fading adaptation.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->
