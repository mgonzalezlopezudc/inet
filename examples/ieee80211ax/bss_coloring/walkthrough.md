# Walkthrough - HE BSS Coloring & Spatial Reuse Simulation

This walkthrough explains how 802.11ax turns BSS identity into a spatial-reuse
decision. Earlier stations defer to any sufficiently strong same-channel
transmission. An HE station can identify an inter-BSS PPDU and apply a more
permissive OBSS/PD threshold, gaining airtime when the other BSS is far enough
away that simultaneous transmissions remain decodable.

## Background: BSS Coloring & Spatial Reuse

In legacy 802.11 standards, stations operating on the same channel share the channel capacity via CSMA/CA. When a station hears any transmission above its Clear Channel Assessment Carrier Sense (CCA-CS) threshold (e.g., -82 dBm for 20 MHz), it considers the medium busy and defers transmission. In dense deployments (such as apartment buildings or offices), overlapping BSSs (OBSS) operating on the same channel frequently suppress one another, causing severe throughput degradation even when a concurrent transmission would have been decodable.

802.11ax introduces **BSS Coloring** and **OBSS Packet Detect (OBSS/PD) Spatial Reuse**:
1. **BSS Color**: Every BSS is assigned a numerical identifier called a "color" (between 1 and 63), carried in the HE PHY preamble header.
2. **Frame Classification**: A receiver distinguishes between **Intra-BSS** frames (same color as its own BSS) and **Inter-BSS / OBSS** frames (different color).
3. **OBSS/PD Spatial Reuse**: If an incoming frame is Inter-BSS and its received power is below a configured **OBSS/PD threshold** (e.g., -62 dBm), the receiver can choose to ignore the frame and treat the physical medium as IDLE, enabling concurrent transmission.
4. **Dual NAV (Two NAV)**: In legacy 802.11, there is only one Network Allocation Vector (NAV) timer for virtual carrier sensing. 802.11ax introduces two separate NAV timers:
   - **Intra-BSS NAV**: Updated by virtual carrier sense fields in frames originating from the station's own BSS.
   - **Basic NAV**: Updated by virtual carrier sense fields in frames originating from an overlapping BSS (OBSS).
   - Separating these timers prevents an OBSS frame's virtual carrier sense reservation from blocking transmissions within the station's local BSS, protecting local transmissions while still allowing spatial reuse.

---

## Network Topology and Configuration

The network [BssColoringNetwork.ned](BssColoringNetwork.ned) consists of two overlapping BSSs:
- **BSS 1**: `ap1` at `(200, 250)` and associated `sta1[0..1]` at `(170, 240/260)`. Color 1 is set on the AP and both STAs.
- **BSS 2**: `ap2` at `(400, 250)` and associated `sta2[0..1]` at `(430, 240/260)`. Color 2 is set on the AP and both STAs.
- Wired servers generate downlink UDP traffic to the client hosts (1000B payloads sent every 2ms). All conditions use a `0.2–0.25s` warm-up trigger and start normal traffic at `0.3s`.
- The medium limit cache is set to `100ms` so the saturated aggregate transmissions are not rejected by the generic `10ms` limit.

The `200 m` AP separation and `30 m` AP-to-client distances create the intended
spatial-reuse geometry: the other AP should be detectable by ordinary CCA but
much weaker than the wanted AP at each receiver. MCS 0 and `20 mW` power leave
link margin for concurrent transmission; the `2 ms` streams keep both BSSs
backlogged so an airtime opportunity can become measurable goodput.

---

## Configurations in `omnetpp.ini`

The [omnetpp.ini](omnetpp.ini) file defines several configurations:

1. **`BssColoringDisabled`**:
   - Spatial reuse is disabled: `enableSpatialReuse = false`.
   - The nodes defer to the other BSS because the signal (-80 dBm) is above the sensitivity threshold.
2. **`BssColoringEnabled`**:
   - Spatial reuse is enabled: `enableSpatialReuse = true` with `obssPdThreshold = -62dBm`.
   - Inter-BSS frames received below the threshold are ignored, enabling concurrent transmissions.
3. **`ObssPdConservative` / `ObssPdAggressive`**:
   - Sweeps the OBSS/PD threshold (-78 dBm and -52 dBm) to study the trade-off between concurrency and collision risk.
4. **`BssColoringCollision`**:
   - All radios in both BSSs use BSS Color 1. Spatial reuse fails because the other BSS is classified as same-color traffic.
5. **`TwoNav`**:
   - Enables separate NAV timers: `heTwoNav = true`.

---

## Running the Simulation

Execute the configurations using Cmdenv:
```sh
bin/inet -u Cmdenv -c BssColoringDisabled examples/ieee80211ax/bss_coloring/omnetpp.ini
bin/inet -u Cmdenv -c BssColoringEnabled examples/ieee80211ax/bss_coloring/omnetpp.ini
bin/inet -u Cmdenv -c TwoNav examples/ieee80211ax/bss_coloring/omnetpp.ini
```

---

## Verifying Results

Query the packet delivery counts at the client stations:
```sh
opp_scavetool query -l -f 'name =~ "packetReceived:vector(packetBytes)" and module =~ "*.sta*app*"' examples/ieee80211ax/bss_coloring/results/*.vec
```

### Quantitative Summary:

| Configuration | Aggregate goodput | Jain fairness | Concurrent AP airtime |
|---|---:|---:|---:|
| **BssColoringDisabled** | 7.084 ± 0.493 Mbps | 0.937 ± 0.018 | 0.614 ± 0.262% |
| **BssColoringEnabled** | 8.121 ± 0.157 Mbps | 0.537 ± 0.020 | 1.158 ± 0.463% |
| **ObssPdConservative** | 8.121 ± 0.157 Mbps | 0.537 ± 0.020 | 1.158 ± 0.463% |
| **ObssPdAggressive** | 8.121 ± 0.157 Mbps | 0.537 ± 0.020 | 1.158 ± 0.463% |
| **BssColoringCollision** | 7.084 ± 0.493 Mbps | 0.937 ± 0.018 | 0.614 ± 0.262% |

Values are means ± 95% Student-t confidence intervals over five seeded runs,
measured from `0.3–0.95s`. The results show a clear throughput and airtime
separation among configurations when DL MU-OFDMA scheduling is functioning.
Reason-coded receiver vectors confirm that the enabled treatments classify different-color PPDUs as inter-BSS and reset CCA when they are below OBSS/PD. The same-color collision treatment remains intra-BSS. Enabling spatial reuse increases concurrent AP airtime (to ~1.16%) and
aggregate goodput (to 8.12 Mbps). However, concurrent transmissions
introduce mutual interference at the receivers, leading to packet losses and lowering the
Jain's fairness metric (down to ~0.53) compared to Disabled (~0.94) where the APs strictly
defer to one another. When BSS colors collide (BssColoringCollision), the inter-BSS traffic
is classified as same-color, preventing spatial reuse and yielding results similar to Disabled.

The three enabled thresholds produce identical run trajectories here. This is not a standards violation: the observed cross-BSS PPDUs are below even the conservative `-78 dBm` threshold, so `-78`, `-62`, and `-52 dBm` make the same binary CCA decision. It is a scenario limitation for studying the threshold trade-off; a distance/power sweep spanning those thresholds is required to separate the treatments.

---

## PCAP Tshark Packet Exchange Analysis

To record PCAP traces and inspect them with TShark, run the simulation with PCAP recording and checksum computation enabled:

```sh
mkdir -p examples/ieee80211ax/bss_coloring/results/pcap
bin/inet -u Cmdenv -f examples/ieee80211ax/bss_coloring/omnetpp.ini -c BssColoringEnabled -r 0 --result-dir=examples/ieee80211ax/bss_coloring/results/pcap --**.numPcapRecorders=1 --**.checksumMode=\"computed\" --**.fcsMode=\"computed\" --**.pcapRecorder[*].moduleNamePatterns=\"wlan[0]\" --**.pcapRecorder[*].dumpProtocols=\"ieee80211mac\" --**.pcapRecorder[*].fileFormat=\"pcapng\" --**.pcapRecorder[*].timePrecision=9 --**.pcapRecorder[*].alwaysFlush=true
```

Use TShark to print the timeline of packet exchanges at the first Access Point (`ap1`):

```sh
tshark -n -r 'examples/ieee80211ax/bss_coloring/results/pcap/BssColoringEnabled-#0BssColoringNetwork.ap1.wlan[0].pcap' -c 20
```

The run-0 captures are nonempty PCAPng files with 83 frames at each
AP. TShark shows the same warm-up/data/action/ACK exchange pattern at both
observation points. The decoded output timeline shows:
1. **Downlink UDP Packets**: `ap1` sends UDP data frames to its stations (e.g. frame 1, 15).
2. **Action Frame Handshake**: Stations establish block acknowledgment session configurations with their AP (e.g. frames 3, 5, 7, 11).
3. **Evidence boundary**: MAC PCAPs show the exchanges but not the receiver's CCA decision. The result vectors directly record local/received color, intra/inter-BSS classification, eligibility, ignore decision, OBSS/PD threshold, reason code, and coupled transmit-power limit. The campaign refuses to plot an enabled condition unless eligible inter-BSS decisions are present and the threshold/power relation is valid.

In a topology whose received powers cross the thresholds, `-78 dBm` should
admit fewer inter-BSS opportunities while `-52 dBm` is more permissive and risks harmful
interference; `-62 dBm` is the middle treatment. In this topology all three make the same decisions. A broader spatial-reuse study
must inspect concurrency, goodput, PER, and fairness together—raising the
threshold is not beneficial if it merely converts deferral into collisions.

---

## Two NAV (Dual NAV) Verification

To verify the Dual NAV (`TwoNav`) configuration, query the NAV transition vectors in the result files:
- **`nav` (Basic NAV)**: Records transitions of the basic NAV timer (source `navChanged`).
- **`intraBssNavChanged`**: Records transitions of the intra-BSS NAV timer (source `intraBssNavChanged`).

Query the vector metrics in scavetool to see how the client stations update their separate NAV timers:
1. In `BssColoringEnabled` (without TwoNav), every inter-BSS virtual carrier reservation updates the single main `nav:vector`, forcing the station to defer.
2. In `TwoNav`, inter-BSS frame reservations update only the Basic NAV (monitored via `nav:vector`), while local intra-BSS reservations update the Intra-BSS NAV (monitored via `intraBssNavChanged:vector`). This separation allows the MAC layer to ignore Basic NAV updates during eligible spatial-reuse transmission opportunities. (Note: Since there are no client-to-client transmissions in this downlink-heavy scenario, the `intraBssNavChanged` signal remains un-triggered/idle).

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
| **PASS** | BssColoringCollision produced protocol-visible wireless observations | 2792 AP/global transmission observations |
| **PASS** | BssColoringDisabled produced protocol-visible wireless observations | 2792 AP/global transmission observations |
| **PASS** | BssColoringEnabled produced protocol-visible wireless observations | 2483 AP/global transmission observations |
| **PASS** | ObssPdAggressive produced protocol-visible wireless observations | 2483 AP/global transmission observations |
| **PASS** | ObssPdConservative produced protocol-visible wireless observations | 2483 AP/global transmission observations |
| **PASS** | TwoNav produced protocol-visible wireless observations | 2692 AP/global transmission observations |
| **PASS** | The bounded scenario exposes a coloring/OBSS-PD decision difference | At least two frame-distribution signatures differ |

### Configuration: `BssColoringCollision`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2792**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 504 | 18.05% | 2194.0 B | 0.0 B | 2436.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 58.59% | 122.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 276 | 9.89% | 1066.0 B | 0.0 B | 1167.8 us | 7.3 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 15.38% | 32.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 234 | 8.38% | 1066.0 B | 0.0 B | 1202.2 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 13.42% | 28.13% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fcac22" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 502 | 17.98% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 2.07% | 4.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#a96c3d" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 12 | 0.43% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.04% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#102ea8" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 12 | 0.43% | 32.0 B | 0.0 B | 71.0 us | 0.0 us | 5050 MHz | -72.5 dBm | - | 0.04% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0639bc" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 16 | 0.57% | 32.0 B | 0.0 B | 116.3 us | 0.0 us | 5045 MHz, 5055 MHz | -72.2 dBm | - | 0.09% | 0.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1037ad" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 988 | 35.39% | 32.0 B | 0.0 B | 206.7 us | 0.0 us | 5043 MHz, 5047 MHz | -72.0 dBm | - | 9.74% | 20.42% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#61a1ef" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 234 | 8.38% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.5 dBm | 13.0 dBm | 0.57% | 1.20% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f0372d" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 14 | 0.50% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.05% | 0.11% |

### Configuration: `BssColoringDisabled`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2792**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 504 | 18.05% | 2194.0 B | 0.0 B | 2436.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 58.59% | 122.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 276 | 9.89% | 1066.0 B | 0.0 B | 1167.8 us | 7.3 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 15.38% | 32.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 234 | 8.38% | 1066.0 B | 0.0 B | 1202.2 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 13.42% | 28.13% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fcac22" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 502 | 17.98% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 2.07% | 4.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#a96c3d" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 12 | 0.43% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.04% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#102ea8" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 12 | 0.43% | 32.0 B | 0.0 B | 71.0 us | 0.0 us | 5050 MHz | -72.5 dBm | - | 0.04% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0639bc" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 16 | 0.57% | 32.0 B | 0.0 B | 116.3 us | 0.0 us | 5045 MHz, 5055 MHz | -72.2 dBm | - | 0.09% | 0.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1037ad" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 988 | 35.39% | 32.0 B | 0.0 B | 206.7 us | 0.0 us | 5043 MHz, 5047 MHz | -72.0 dBm | - | 9.74% | 20.42% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#61a1ef" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 234 | 8.38% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.5 dBm | 13.0 dBm | 0.57% | 1.20% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f0372d" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 14 | 0.50% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.05% | 0.11% |

### Configuration: `BssColoringEnabled`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2483**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 262 | 10.55% | 2194.0 B | 0.0 B | 2436.3 us | 0.0 us | 5050 MHz | - | 13.0 dBm | 46.95% | 63.83% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 84 | 3.38% | 1066.0 B | 0.0 B | 1181.7 us | 17.8 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 7.30% | 9.93% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 324 | 13.05% | 1066.0 B | 0.0 B | 1202.2 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 28.65% | 38.95% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fcac22" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 515 | 20.74% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 3.27% | 4.45% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#a96c3d" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 34 | 1.37% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.16% | 0.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#102ea8" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 34 | 1.37% | 32.0 B | 0.0 B | 71.0 us | 0.0 us | 5050 MHz | -72.5 dBm | - | 0.18% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0639bc" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 458 | 18.45% | 32.0 B | 0.0 B | 116.3 us | 0.0 us | 5045 MHz, 5055 MHz | -72.1 dBm | - | 3.92% | 5.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1037ad" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 580 | 23.36% | 32.0 B | 0.0 B | 206.7 us | 0.0 us | 5043 MHz, 5047 MHz | -71.9 dBm | - | 8.82% | 11.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#61a1ef" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 178 | 7.17% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.5 dBm | 13.0 dBm | 0.67% | 0.91% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f0372d" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 14 | 0.56% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.08% | 0.11% |

### Configuration: `ObssPdAggressive`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2483**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 262 | 10.55% | 2194.0 B | 0.0 B | 2436.3 us | 0.0 us | 5050 MHz | - | 13.0 dBm | 46.95% | 63.83% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 84 | 3.38% | 1066.0 B | 0.0 B | 1181.7 us | 17.8 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 7.30% | 9.93% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 324 | 13.05% | 1066.0 B | 0.0 B | 1202.2 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 28.65% | 38.95% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fcac22" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 515 | 20.74% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 3.27% | 4.45% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#a96c3d" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 34 | 1.37% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.16% | 0.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#102ea8" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 34 | 1.37% | 32.0 B | 0.0 B | 71.0 us | 0.0 us | 5050 MHz | -72.5 dBm | - | 0.18% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0639bc" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 458 | 18.45% | 32.0 B | 0.0 B | 116.3 us | 0.0 us | 5045 MHz, 5055 MHz | -72.1 dBm | - | 3.92% | 5.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1037ad" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 580 | 23.36% | 32.0 B | 0.0 B | 206.7 us | 0.0 us | 5043 MHz, 5047 MHz | -71.9 dBm | - | 8.82% | 11.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#61a1ef" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 178 | 7.17% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.5 dBm | 13.0 dBm | 0.67% | 0.91% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f0372d" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 14 | 0.56% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.08% | 0.11% |

### Configuration: `ObssPdConservative`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2483**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 262 | 10.55% | 2194.0 B | 0.0 B | 2436.3 us | 0.0 us | 5050 MHz | - | 13.0 dBm | 46.95% | 63.83% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 84 | 3.38% | 1066.0 B | 0.0 B | 1181.7 us | 17.8 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 7.30% | 9.93% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 324 | 13.05% | 1066.0 B | 0.0 B | 1202.2 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 28.65% | 38.95% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fcac22" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 515 | 20.74% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 3.27% | 4.45% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#a96c3d" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 34 | 1.37% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.16% | 0.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#102ea8" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 34 | 1.37% | 32.0 B | 0.0 B | 71.0 us | 0.0 us | 5050 MHz | -72.5 dBm | - | 0.18% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0639bc" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 458 | 18.45% | 32.0 B | 0.0 B | 116.3 us | 0.0 us | 5045 MHz, 5055 MHz | -72.1 dBm | - | 3.92% | 5.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1037ad" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 580 | 23.36% | 32.0 B | 0.0 B | 206.7 us | 0.0 us | 5043 MHz, 5047 MHz | -71.9 dBm | - | 8.82% | 11.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#61a1ef" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 178 | 7.17% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.5 dBm | 13.0 dBm | 0.67% | 0.91% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f0372d" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 14 | 0.56% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.08% | 0.11% |

### Configuration: `TwoNav`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2692**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 298 | 11.07% | 2194.0 B | 0.0 B | 2436.3 us | 0.0 us | 5050 MHz | - | 13.0 dBm | 51.64% | 72.60% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 52 | 1.93% | 1066.0 B | 0.0 B | 1177.3 us | 16.6 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 4.35% | 6.12% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 274 | 10.18% | 1067.7 B | 2.0 B | 1204.1 us | 2.2 us | 5050 MHz | -74.5 dBm | 13.0 dBm | 23.47% | 32.99% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fcac22" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 575 | 21.36% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 3.53% | 4.96% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#a96c3d" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 14 | 0.52% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.06% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#102ea8" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 14 | 0.52% | 32.0 B | 0.0 B | 71.0 us | 0.0 us | 5050 MHz | -72.5 dBm | - | 0.07% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0639bc" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 260 | 9.66% | 32.0 B | 0.0 B | 116.3 us | 0.0 us | 5045 MHz, 5055 MHz | -72.2 dBm | - | 2.15% | 3.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1037ad" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 932 | 34.62% | 32.0 B | 0.0 B | 206.7 us | 0.0 us | 5043 MHz, 5047 MHz | -72.0 dBm | - | 13.70% | 19.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#61a1ef" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 257 | 9.55% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -75.4 dBm | 13.0 dBm | 0.94% | 1.32% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f0372d" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 16 | 0.59% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -73.8 dBm | 13.0 dBm | 0.09% | 0.12% |

### Analysis of Packet Distribution
**PASS: BSS-coloring separation.** At least two frame-distribution signatures differ. IEEE Std 802.11-2024 Clause 26.10 permits eligible inter-BSS reuse after OBSS/PD classification; it does not guarantee a throughput improvement, and a more permissive threshold can increase interference. The differing distribution is only a screening signal; the separate five-seed result campaign validates direct OBSS classification, threshold, CCA, power-limit, and reuse-decision telemetry. The current model reports the standards-defined threshold/power coupling but does not dynamically adapt OBSS/PD or apply that limit to later transmissions.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->
