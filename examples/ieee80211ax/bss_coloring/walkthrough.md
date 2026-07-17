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
| **BssColoringDisabled** | 6.649 ± 0.165 Mbps | 0.900 ± 0.081 | 0.532 ± 0.258% |
| **BssColoringEnabled** | 6.649 ± 0.165 Mbps | 0.900 ± 0.081 | 0.532 ± 0.258% |
| **ObssPdConservative** | 6.649 ± 0.165 Mbps | 0.900 ± 0.081 | 0.532 ± 0.258% |
| **ObssPdAggressive** | 6.649 ± 0.165 Mbps | 0.900 ± 0.081 | 0.532 ± 0.258% |
| **BssColoringCollision** | 6.649 ± 0.165 Mbps | 0.900 ± 0.081 | 0.532 ± 0.258% |

Values are means ± 95% Student-t confidence intervals over five seeded runs,
measured from `0.3–0.95s`. The identical results mean this scalar-medium
experiment demonstrates color assignment, threshold configuration, and dual
NAV state, but does not establish a throughput advantage. The intended
802.11ax advantage is conditional: ignoring a weak OBSS is useful only when it
changes the CCA decision and the added interference still permits decoding.

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
3. **Evidence boundary**: The native MAC captures do not expose the full
   OBSS/PD decision or distinguish every medium observation. Together with the
   identical `.sca/.vec` metrics for all five manifest conditions, this run
   does not support claiming a spatial-reuse throughput benefit.

The threshold sweep is still instructive. `-78 dBm` is conservative and should
admit fewer inter-BSS opportunities; `-52 dBm` is aggressive and risks harmful
interference; `-62 dBm` is the middle treatment. A good spatial-reuse study
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

## 802.11 Packet Type Statistics
This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from the Access Point's wireless interface (`ap.wlan[0]`), which captures all uplink, downlink, and management traffic in the BSS without duplication.

Two airtime occupancy percentages are provided:
- **Air Time %**: The percentage of the total transmission airtime of all packets occupied by this frame type.
- **Air Time (Sim Time) %**: The percentage of the total simulation time occupied by the transmission of this frame type (defined as the sum of physical airtimes of this frame type w.r.t. the total simulation time limit).

### Configuration: `BssColoringCollision`
Total over-the-air packets captured (Global BSS/AP): **166**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Control: Subtype 0 | 44 | 26.51% | 28262.4 B | 17298.2 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 96.52% | 41.54% |
| Control: Block Ack Request (BAR) | 36 | 21.69% | 24.0 B | 0.0 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.23% | 0.10% |
| Control: Block Ack (BA) | 36 | 21.69% | 38.7 B | 27.5 B | 5050 MHz | -72.5 dBm | - | 0.28% | 0.12% |
| Control: Ack | 20 | 12.05% | 14.0 B | 0.0 B | 5050 MHz | -72.2 dBm | 13.0 dBm | 0.11% | 0.05% |
| Data: QoS Data | 16 | 9.64% | 1066.0 B | 0.0 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 2.63% | 1.13% |
| Management: Action | 14 | 8.43% | 37.0 B | 0.0 B | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.23% | 0.10% |

### Configuration: `BssColoringDisabled`
Total over-the-air packets captured (Global BSS/AP): **166**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Control: Subtype 0 | 44 | 26.51% | 28262.4 B | 17298.2 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 96.52% | 41.54% |
| Control: Block Ack Request (BAR) | 36 | 21.69% | 24.0 B | 0.0 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.23% | 0.10% |
| Control: Block Ack (BA) | 36 | 21.69% | 38.7 B | 27.5 B | 5050 MHz | -72.5 dBm | - | 0.28% | 0.12% |
| Control: Ack | 20 | 12.05% | 14.0 B | 0.0 B | 5050 MHz | -72.2 dBm | 13.0 dBm | 0.11% | 0.05% |
| Data: QoS Data | 16 | 9.64% | 1066.0 B | 0.0 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 2.63% | 1.13% |
| Management: Action | 14 | 8.43% | 37.0 B | 0.0 B | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.23% | 0.10% |

### Configuration: `BssColoringEnabled`
Total over-the-air packets captured (Global BSS/AP): **166**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Control: Subtype 0 | 44 | 26.51% | 28262.4 B | 17298.2 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 96.52% | 41.54% |
| Control: Block Ack Request (BAR) | 36 | 21.69% | 24.0 B | 0.0 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.23% | 0.10% |
| Control: Block Ack (BA) | 36 | 21.69% | 38.7 B | 27.5 B | 5050 MHz | -72.5 dBm | - | 0.28% | 0.12% |
| Control: Ack | 20 | 12.05% | 14.0 B | 0.0 B | 5050 MHz | -72.2 dBm | 13.0 dBm | 0.11% | 0.05% |
| Data: QoS Data | 16 | 9.64% | 1066.0 B | 0.0 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 2.63% | 1.13% |
| Management: Action | 14 | 8.43% | 37.0 B | 0.0 B | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.23% | 0.10% |

### Configuration: `ObssPdAggressive`
Total over-the-air packets captured (Global BSS/AP): **166**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Control: Subtype 0 | 44 | 26.51% | 28262.4 B | 17298.2 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 96.52% | 41.54% |
| Control: Block Ack Request (BAR) | 36 | 21.69% | 24.0 B | 0.0 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.23% | 0.10% |
| Control: Block Ack (BA) | 36 | 21.69% | 38.7 B | 27.5 B | 5050 MHz | -72.5 dBm | - | 0.28% | 0.12% |
| Control: Ack | 20 | 12.05% | 14.0 B | 0.0 B | 5050 MHz | -72.2 dBm | 13.0 dBm | 0.11% | 0.05% |
| Data: QoS Data | 16 | 9.64% | 1066.0 B | 0.0 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 2.63% | 1.13% |
| Management: Action | 14 | 8.43% | 37.0 B | 0.0 B | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.23% | 0.10% |

### Configuration: `ObssPdConservative`
Total over-the-air packets captured (Global BSS/AP): **166**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Control: Subtype 0 | 44 | 26.51% | 28262.4 B | 17298.2 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 96.52% | 41.54% |
| Control: Block Ack Request (BAR) | 36 | 21.69% | 24.0 B | 0.0 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.23% | 0.10% |
| Control: Block Ack (BA) | 36 | 21.69% | 38.7 B | 27.5 B | 5050 MHz | -72.5 dBm | - | 0.28% | 0.12% |
| Control: Ack | 20 | 12.05% | 14.0 B | 0.0 B | 5050 MHz | -72.2 dBm | 13.0 dBm | 0.11% | 0.05% |
| Data: QoS Data | 16 | 9.64% | 1066.0 B | 0.0 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 2.63% | 1.13% |
| Management: Action | 14 | 8.43% | 37.0 B | 0.0 B | 5050 MHz | -74.0 dBm | 13.0 dBm | 0.23% | 0.10% |

### Configuration: `TwoNav`
Total over-the-air packets captured (Global BSS/AP): **208**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Control: Subtype 0 | 56 | 26.92% | 20940.3 B | 14136.2 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 95.43% | 39.20% |
| Control: Block Ack Request (BAR) | 46 | 22.12% | 24.0 B | 0.0 B | 5050 MHz | -80.0 dBm | 13.0 dBm | 0.31% | 0.13% |
| Control: Block Ack (BA) | 46 | 22.12% | 32.0 B | 0.0 B | 5050 MHz | -72.5 dBm | - | 0.34% | 0.14% |
| Control: Ack | 26 | 12.50% | 14.0 B | 0.0 B | 5050 MHz | -73.1 dBm | 13.0 dBm | 0.16% | 0.06% |
| Data: QoS Data | 19 | 9.13% | 1158.4 B | 178.5 B | 5050 MHz | -75.4 dBm | 13.0 dBm | 3.50% | 1.44% |
| Management: Action | 15 | 7.21% | 37.0 B | 0.0 B | 5050 MHz | -73.1 dBm | 13.0 dBm | 0.25% | 0.10% |

### Analysis of Packet Distribution
BSS Coloring simulations show packet exchanges across multiple overlapping BSSs (OBSS). In addition to standard **QoS Data** and **Block Ack (BA)** frames, the statistics reflect management traffic like **Beacons** from multiple APs. When BSS coloring is disabled, collisions and backoffs occur, altering the proportion of retransmitted data frames. Enabling BSS coloring reduces mutual interference, allowing smoother channel access and higher successful data frame delivery rates.
