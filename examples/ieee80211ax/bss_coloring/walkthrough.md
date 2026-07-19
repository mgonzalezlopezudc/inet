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
- Wired servers generate downlink UDP traffic to the client hosts (1000B payloads with intervals uniformly distributed between `1ms` and `1.4ms`). All conditions use a `0.2–0.25s` warm-up trigger and start normal traffic at `0.3s`.
- The medium limit cache is set to `100ms` so the saturated aggregate transmissions are not rejected by the generic `10ms` limit.

The `200 m` AP separation and `30 m` AP-to-client distances create the intended
spatial-reuse geometry: the other AP should be detectable by ordinary CCA but
much weaker than the wanted AP at each receiver. MCS 0 and `20 mW` power leave
link margin for concurrent transmission. The randomized high-rate streams keep
both BSSs backlogged while avoiding an artificial advantage from scheduling all
four application packets at exactly the same simulation times.

---

## Configurations in `omnetpp.ini`

The [omnetpp.ini](omnetpp.ini) file defines several configurations:

1. **`BssColoringDisabled`**:
   - Spatial reuse is disabled: `enableSpatialReuse = false`.
   - The nodes defer to the other BSS because the signal (-80 dBm) is above the sensitivity threshold.
2. **`BssColoringEnabled`**:
   - Spatial reuse is enabled: `enableSpatialReuse = true` with `obssPdThreshold = -79dBm`.
   - Inter-BSS frames received below the threshold are ignored, enabling concurrent transmissions.
3. **`ObssPdConservative` / `ObssPdAggressive`**:
   - Sweeps the OBSS/PD threshold from the standard minimum (`-82dBm`) to maximum (`-62dBm`).
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
| **BssColoringDisabled** | 6.540 ± 0.338 Mbps | 0.965 ± 0.016 | 0.444 ± 0.178% |
| **BssColoringEnabled** | 8.446 ± 0.808 Mbps | 0.922 ± 0.154 | 15.416 ± 3.034% |
| **ObssPdConservative** | 6.540 ± 0.338 Mbps | 0.965 ± 0.016 | 0.444 ± 0.178% |
| **ObssPdAggressive** | 8.446 ± 0.808 Mbps | 0.922 ± 0.154 | 15.416 ± 3.034% |
| **BssColoringCollision** | 6.540 ± 0.338 Mbps | 0.965 ± 0.016 | 0.444 ± 0.178% |

Values are means ± 95% Student-t confidence intervals over five seeded runs,
measured from `0.3–0.95s`. Reason-coded receiver vectors confirm that the
enabled treatments classify different-color PPDUs as inter-BSS and reset CCA
when they are below OBSS/PD. Raising the threshold just above the observed
cross-BSS power increases aggregate goodput by about 29% and concurrent AP
airtime by roughly 35 times, while mean Jain fairness remains above 0.92.

The `-82dBm` conservative treatment follows the disabled trajectory because
the approximately `-80dBm` OBSS transmissions remain above its threshold. The
`-79dBm` enabled and `-62dBm` aggressive treatments follow the same higher-reuse
trajectory because the observed OBSS power is already below both thresholds;
raising the threshold farther cannot create another binary CCA opportunity in
this fixed geometry. Giving both BSSs the same color also reproduces the
disabled trajectory, showing that the gain depends on correct BSS
classification rather than merely enabling the receiver option.

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

The fresh run-0 PCAPng captures contain 1416 frames at `ap1` and 1434 frames at
`ap2`. TShark shows the same warm-up/data/action/ACK exchange pattern at both
observation points. The decoded output timeline shows:
1. **Downlink UDP Packets**: `ap1` sends UDP data frames to its stations (e.g. frame 1, 15).
2. **Action Frame Handshake**: Stations establish block acknowledgment session configurations with their AP (e.g. frames 3, 5, 7, 11).
3. **Evidence boundary**: MAC PCAPs show the exchanges but not the receiver's CCA decision. The result vectors directly record local/received color, intra/inter-BSS classification, eligibility, ignore decision, OBSS/PD threshold, reason code, and coupled transmit-power limit. The campaign refuses to plot an enabled condition unless eligible inter-BSS decisions are present and the threshold/power relation is valid.

The `-82dBm`, `-79dBm`, and `-62dBm` settings span the conservative boundary,
the useful transition for this geometry, and the standard maximum. A broader
spatial-reuse study must inspect concurrency, goodput, PER, and fairness
together—raising the threshold is not beneficial if it merely converts
deferral into collisions.

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

Capture session `20260719T095439Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %**: The sum of this frame type's estimated airtimes divided by the simulation time limit. Concurrent transmissions from multiple capture points are counted separately, so this value can exceed 100%; it is not the union of busy channel time.

### Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | BssColoringCollision produced protocol-visible wireless observations | 2679 AP/global transmission observations |
| **PASS** | BssColoringDisabled produced protocol-visible wireless observations | 2679 AP/global transmission observations |
| **PASS** | BssColoringEnabled produced protocol-visible wireless observations | 2850 AP/global transmission observations |
| **PASS** | ObssPdAggressive produced protocol-visible wireless observations | 2850 AP/global transmission observations |
| **PASS** | ObssPdConservative produced protocol-visible wireless observations | 2679 AP/global transmission observations |
| **PASS** | TwoNav produced protocol-visible wireless observations | 2322 AP/global transmission observations |
| **PASS** | The bounded scenario exposes a coloring/OBSS-PD decision difference | At least two frame-distribution signatures differ |

### Configuration: `BssColoringCollision`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2679**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 466 | 17.39% | 2194.0 B | 0.0 B | 2436.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 56.01% | 113.53% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 316 | 11.80% | 1068.6 B | 173.3 B | 1170.5 us | 190.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 18.25% | 36.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 227 | 8.47% | 1068.0 B | 166.9 B | 1204.4 us | 182.6 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 13.49% | 27.34% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fcac22" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 466 | 17.39% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 1.98% | 4.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#a96c3d" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 10 | 0.37% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.03% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#102ea8" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 10 | 0.37% | 104.0 B | 58.8 B | 149.8 us | 64.3 us | 5050 MHz | -72.5 dBm | - | 0.07% | 0.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1037ad" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 932 | 34.79% | 32.0 B | 0.0 B | 206.7 us | 0.0 us | 5043 MHz, 5047 MHz | -72.0 dBm | - | 9.50% | 19.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#61a1ef" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 238 | 8.88% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.5 dBm | 13.0 dBm | 0.60% | 1.22% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f0372d" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 14 | 0.52% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.05% | 0.11% |

### Configuration: `BssColoringDisabled`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2679**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 466 | 17.39% | 2194.0 B | 0.0 B | 2436.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 56.01% | 113.53% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 316 | 11.80% | 1068.6 B | 173.3 B | 1170.5 us | 190.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 18.25% | 36.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 227 | 8.47% | 1068.0 B | 166.9 B | 1204.4 us | 182.6 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 13.49% | 27.34% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fcac22" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 466 | 17.39% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 1.98% | 4.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#a96c3d" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 10 | 0.37% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.03% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#102ea8" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 10 | 0.37% | 104.0 B | 58.8 B | 149.8 us | 64.3 us | 5050 MHz | -72.5 dBm | - | 0.07% | 0.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1037ad" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 932 | 34.79% | 32.0 B | 0.0 B | 206.7 us | 0.0 us | 5043 MHz, 5047 MHz | -72.0 dBm | - | 9.50% | 19.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#61a1ef" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 238 | 8.88% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.5 dBm | 13.0 dBm | 0.60% | 1.22% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f0372d" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 14 | 0.52% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.05% | 0.11% |

### Configuration: `BssColoringEnabled`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2850**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 333 | 11.68% | 2200.4 B | 117.3 B | 2443.3 us | 128.3 us | 5050 MHz | - | 13.0 dBm | 44.11% | 81.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 348 | 12.21% | 1069.1 B | 198.4 B | 1171.4 us | 217.4 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 22.10% | 40.76% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 267 | 9.37% | 1064.3 B | 130.5 B | 1200.4 us | 142.7 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 17.37% | 32.05% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fcac22" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 340 | 11.93% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 1.59% | 2.94% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#a96c3d" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 11 | 0.39% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.04% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#102ea8" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 11 | 0.39% | 130.2 B | 46.3 B | 178.4 us | 50.6 us | 5050 MHz | -70.2 dBm | - | 0.11% | 0.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0639bc" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 2 | 0.07% | 32.0 B | 0.0 B | 116.3 us | 0.0 us | 5045 MHz | -72.0 dBm | - | 0.01% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1037ad" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 1232 | 43.23% | 32.0 B | 0.0 B | 206.7 us | 0.0 us | 5043 MHz, 5047 MHz, 5053 MHz | -71.4 dBm | - | 13.80% | 25.46% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#61a1ef" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 292 | 10.25% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.5 dBm | 13.0 dBm | 0.81% | 1.50% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f0372d" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 14 | 0.49% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.06% | 0.11% |

### Configuration: `ObssPdAggressive`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2850**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 333 | 11.68% | 2200.4 B | 117.3 B | 2443.3 us | 128.3 us | 5050 MHz | - | 13.0 dBm | 44.11% | 81.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 348 | 12.21% | 1069.1 B | 198.4 B | 1171.4 us | 217.4 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 22.10% | 40.76% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 267 | 9.37% | 1064.3 B | 130.5 B | 1200.4 us | 142.7 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 17.37% | 32.05% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fcac22" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 340 | 11.93% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 1.59% | 2.94% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#a96c3d" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 11 | 0.39% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.04% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#102ea8" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 11 | 0.39% | 130.2 B | 46.3 B | 178.4 us | 50.6 us | 5050 MHz | -70.2 dBm | - | 0.11% | 0.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0639bc" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 2 | 0.07% | 32.0 B | 0.0 B | 116.3 us | 0.0 us | 5045 MHz | -72.0 dBm | - | 0.01% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1037ad" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 1232 | 43.23% | 32.0 B | 0.0 B | 206.7 us | 0.0 us | 5043 MHz, 5047 MHz, 5053 MHz | -71.4 dBm | - | 13.80% | 25.46% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#61a1ef" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 292 | 10.25% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.5 dBm | 13.0 dBm | 0.81% | 1.50% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f0372d" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 14 | 0.49% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.06% | 0.11% |

### Configuration: `ObssPdConservative`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2679**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 466 | 17.39% | 2194.0 B | 0.0 B | 2436.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 56.01% | 113.53% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 316 | 11.80% | 1068.6 B | 173.3 B | 1170.5 us | 190.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 18.25% | 36.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 227 | 8.47% | 1068.0 B | 166.9 B | 1204.4 us | 182.6 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 13.49% | 27.34% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fcac22" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 466 | 17.39% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 1.98% | 4.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#a96c3d" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 10 | 0.37% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.03% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#102ea8" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 10 | 0.37% | 104.0 B | 58.8 B | 149.8 us | 64.3 us | 5050 MHz | -72.5 dBm | - | 0.07% | 0.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1037ad" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 932 | 34.79% | 32.0 B | 0.0 B | 206.7 us | 0.0 us | 5043 MHz, 5047 MHz | -72.0 dBm | - | 9.50% | 19.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#61a1ef" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 238 | 8.88% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.5 dBm | 13.0 dBm | 0.60% | 1.22% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f0372d" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 14 | 0.52% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.05% | 0.11% |

### Configuration: `TwoNav`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2322**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24db3c" /></svg> | Data: QoS Data [HE-MU, HE, GI 3.2 us, LDPC] | 206 | 8.87% | 2194.0 B | 0.0 B | 2436.3 us | 0.0 us | 5050 MHz | - | 13.0 dBm | 36.84% | 50.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31b125" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 240 | 10.34% | 1068.5 B | 171.1 B | 1170.5 us | 187.4 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 20.62% | 28.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28dc31" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 296 | 12.75% | 1064.8 B | 296.8 B | 1200.9 us | 324.7 us | 5050 MHz | -74.3 dBm | 13.0 dBm | 26.09% | 35.55% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fcac22" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 368 | 15.85% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 2.33% | 3.18% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#a96c3d" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 10 | 0.43% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.05% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#102ea8" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 10 | 0.43% | 152.0 B | 0.0 B | 202.3 us | 0.0 us | 5050 MHz | -72.5 dBm | - | 0.15% | 0.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1037ad" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 824 | 35.49% | 32.0 B | 0.0 B | 206.7 us | 0.0 us | 5043 MHz, 5047 MHz | -72.0 dBm | - | 12.50% | 17.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#61a1ef" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 352 | 15.16% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -74.0 dBm | 13.0 dBm | 1.33% | 1.81% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f0372d" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, BCC] | 16 | 0.69% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -73.8 dBm | 13.0 dBm | 0.09% | 0.12% |

### Analysis of Packet Distribution
**PASS: BSS-coloring separation.** At least two frame-distribution signatures differ. IEEE Std 802.11-2024 Clause 26.10 permits eligible inter-BSS reuse after OBSS/PD classification; it does not guarantee a throughput improvement, and a more permissive threshold can increase interference. The differing distribution is only a screening signal; the separate five-seed result campaign validates direct OBSS classification, threshold, CCA, power-limit, and reuse-decision telemetry. The current model reports the standards-defined threshold/power coupling but does not dynamically adapt OBSS/PD or apply that limit to later transmissions.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->
