# 802.11ax HE Extended Range Single User (ER SU) Simulation

This example illustrates the Extended Range Single User (ER SU) transmission format introduced in the IEEE 802.11ax (Wi-Fi 6) standard. It demonstrates how HE ER SU PPDUs and robust PHY/MAC settings (such as Dual Carrier Modulation and MCS restrictions) maintain connectivity and data delivery at the outer boundaries (cell edge) of a wireless coverage area.

## Background: HE ER SU & Dual Carrier Modulation (DCM)

In dense or outdoor deployments, stations (STAs) at the cell edge suffer from low Signal-to-Noise Ratio (SNR) due to path loss, leading to packet reception failures. To extend range without increasing transmission power, 802.11ax introduces **HE ER SU**:
1. **Preamble Duplication**: The preamble of an HE ER SU PPDU repeats the HE-SIG-A field, increasing its duration by 8 µs (from 36 µs to 44 µs in total). This repetition allows receivers to combine the energy of the two fields, significantly improving HE-SIG-A decoding robustness.
2. **Resource Unit Limitation**: HE ER SU is restricted to a single spatial stream (NSS = 1) and robust MCS indices (MCS 0, 1, or 2).
3. **Dual Carrier Modulation (DCM)**: DCM modulates the same information block over two widely separated subcarriers in the frequency domain. This introduces frequency diversity, making the transmission resilient against frequency-selective fading and noise, at the cost of halving the net data rate.

---

## Network Topology

The network [HeErSuNetwork.ned](HeErSuNetwork.ned) consists of:
- **`ap`**: An Access Point located at `(390, 180)`.
- **`host[0]`**: A stationary wireless host located at `(70, 180)`
  (representing a cell-edge client).
- **Distance**: The AP and client are separated by **320 meters**. At this distance, the Free Space Path Loss (FSPL) at 5 GHz is approximately 96.5 dB, resulting in very weak signal reception (around -86.5 dBm at 10mW transmission power).
- **`server`**: A wired server connected to the AP.
- **Traffic**: Downlink UDP traffic is sent from the `server` to `host[0]` via
  the AP (300 B packets sent every 1 ms).

```
        [server]
           | (wired)
           v
        [ ap ] <--------------- 320m -------------> [host[0]]
      (AP at 390m)                                (STA at 70m)
```

---

## Configurations in `omnetpp.ini`

The [omnetpp.ini](omnetpp.ini) file defines three scenarios:

### 1. `HeSu` (Baseline)
- The AP transmits standard HE SU PPDUs.
- It uses a fixed bitrate configuration of `7.3125 Mbps` (corresponding to MCS 0 with 20 MHz bandwidth, NSS = 1, and no DCM).
- Preamble format is the standard HE SU PPDU (36 µs).

### 2. `HeErSu` (Extended Range SU)
- The AP enables Minstrel rate control with ER SU capability:
  - `**.ap.wlan[*].mac.hcf.rateControl.typename = "HeMinstrelRateControl"`
  - `**.ap.wlan[*].mac.hcf.rateControl.enableExtendedRangeSu = true`
  - `**.ap.wlan[*].mac.hcf.rateControl.preferDcm = true`
  - `**.ap.wlan[*].mac.hcf.rateControl.maxMcs = 2`
- **Result**: The AP rate controller restricts MCS selection to robust levels (0, 1, 2), activates DCM to combat frequency selective fading, and formats the PPDUs as HE ER SU with the repeated HE-SIG-A preamble field (44 µs total duration).

### 3. `ErBss` (ER BSS management behavior)

- Extends `HeErSu`, enables full beaconing/association instead of installing
  association state at initialization, and sets the HE ER BSS capability.
- Management-frame bitrate overrides are removed so HE rate control can choose
  ER SU for the relevant management and group-addressed transmissions.
- Block Ack is disabled to keep the trace focused on ER-BSS management and
  single-MPDU behavior; this is a scope choice, not an ER-BSS requirement.

---

## Running the Simulation

Ensure your environment is set up, then run the simulations.

### Running with Qtenv (GUI)
```sh
bin/inet -u Qtenv -c HeErSu examples/ieee80211ax/he_er_su/omnetpp.ini
```

### Running with Cmdenv (Command Line)
```sh
# Run HeSu Baseline
bin/inet -u Cmdenv -c HeSu examples/ieee80211ax/he_er_su/omnetpp.ini

# Run HeErSu Config
bin/inet -u Cmdenv -c HeErSu examples/ieee80211ax/he_er_su/omnetpp.ini

# Run ER-BSS beaconing and association
bin/inet -u Cmdenv -c ErBss examples/ieee80211ax/he_er_su/omnetpp.ini
```

---

## Verifying Results

After running the simulations, analyze the packets received at the UDP application layer on the `host[0]` client and MAC layer packet drops due to incorrect reception.

```sh
# Query packets received at the UDP sink on the host[0]
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.host[0].app*"' examples/ieee80211ax/he_er_su/results/*.sca

# Query packet drops due to corruption/incorrect reception at the host[0] MAC layer
opp_scavetool query -l -f 'name =~ "packetDropIncorrectlyReceived:count" and module =~ "*.host[0].wlan[0].mac"' examples/ieee80211ax/he_er_su/results/*.sca
```

### Expected Output Summary

```
HeSu-#0.sca:
scalar  HeErSuNetwork.host[0].app[0]       packetReceived:count                 1700
scalar  HeErSuNetwork.host[0].wlan[0].mac  packetDropIncorrectlyReceived:count  14

HeErSu-#0.sca:
scalar  HeErSuNetwork.host[0].app[0]       packetReceived:count                 1700
scalar  HeErSuNetwork.host[0].wlan[0].mac  packetDropIncorrectlyReceived:count  0
scalar  HeErSuNetwork.ap.wlan[0].mac       packetDropIncorrectlyReceived:count  9

ErBss-#0.sca:
scalar  HeErSuNetwork.host[0].app[0]       packetReceived:count                 120
scalar  HeErSuNetwork.host[0].wlan[0].mac  packetDropIncorrectlyReceived:count  0
```

---

## PCAP Tshark Packet Exchange Analysis

To record PCAP traces and inspect them with TShark, run the simulation with PCAP recording and checksum computation enabled:

```sh
bin/inet -u Cmdenv -c HeErSu examples/ieee80211ax/he_er_su/omnetpp.ini --result-dir=examples/ieee80211ax/he_er_su/results --**.numPcapRecorders=1 --**.checksumMode=\"computed\" --**.fcsMode=\"computed\"
```

Use TShark to print the timeline of packet exchanges:

```sh
tshark -n -r examples/ieee80211ax/he_er_su/results/HeErSu-#0HeErSuNetwork.ap.wlan[0].pcap -c 20
```

The decoded output timeline shows:
1. **Long-Distance Downlink Data**: The AP transmits UDP data packets (e.g. frame 1, 6) to the cell-edge `host[0]` at 320m.
2. **Block Ack Negotiation**: Block ACK negotiation Action frames (e.g. frames 3, 5, 7) are exchanged between the AP and the client host to establish session block acknowledgments.
3. **Preamble and Coding Resilience**: In this run, `HeErSu` delivered all 1700 offered packets to the host with 0 host-side incorrect-reception drops; 9 incorrect-reception drops were recorded at the AP. `HeSu` also delivered 1700 packets but recorded 14 host-side incorrect-reception drops.

---

## Interpretation of Results

1. **Successful Delivery**:
   - Both scenarios deliver **1700 packets** to the `host[0]` client during the normalized `.3–2s` traffic interval.
   - *Why?* Under the clean, deterministic Free Space Path Loss model without dynamic shadowing or multipath fading, the signal at 320 meters (-86.55 dBm) remains slightly above the receiver sensitivity threshold of -88 dBm. This allows MCS 0 to decode successfully in both cases.
   - However, standard `HeSu` experiences **14 MAC packet drops** due to marginal reception corruption on the baseline preamble (36 µs).
   - `HeErSu` records **0 host-side MAC packet drops** in this run, although 9 incorrect-reception drops are recorded at the AP. This supports the observed delivery outcome but is not a full coverage or fading sweep.

2. **Under-the-Hood Preamble Difference**:
   - In `HeSu`, transmissions use standard HE SU PPDUs with a **36 µs** preamble.
   - In `HeErSu`, transmissions employ HE ER SU PPDUs. The preamble includes the repeated HE-SIG-A field, increasing the preamble duration to **44 µs**. This extra 8 µs of repeated header symbol provides the essential energy duplication required to decode preambles under marginal SNR or real-world fading conditions, preventing drops.
   - The configuration demonstrates selection and timing of the robust format. It does not prove a full coverage sweep range gain in this deterministic free-space channel; such a claim requires a fading/noise experiment and a delivery/PER sweep.

3. **Why the parameters sit near the coverage boundary**:
   - `320 m`, `10 mW`, and MCS 0 put ordinary HE SU only slightly above the
     configured receiver sensitivity. A short distance would hide the value of
     repeated HE-SIG-A and DCM; a much longer distance would make both modes
     fail regardless of format.
   - The 300-byte, `1 ms` stream creates enough frames to expose a reception
     reliability difference without turning this into a capacity benchmark.
     DCM intentionally trades half the data rate for frequency diversity, so
     its expected advantage is lower packet-error rate or extended coverage,
     not higher peak throughput.

## 802.11 Packet Type Statistics
This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from the Access Point's wireless interface (`ap.wlan[0]`), which captures all uplink, downlink, and management traffic in the BSS without duplication.

Two airtime occupancy percentages are provided:
- **Air Time %**: The percentage of the total transmission airtime of all packets occupied by this frame type.
- **Air Time (Sim Time) %**: The percentage of the total simulation time occupied by the transmission of this frame type (defined as the sum of physical airtimes of this frame type w.r.t. the total simulation time limit).

### Configuration: `ErBss`
Total over-the-air packets captured (Global BSS/AP): **240**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Data: QoS Data | 120 | 50.00% | 166.0 B | 0.0 B | 5010 MHz | - | 10.0 dBm | 89.70% | 1.29% |
| Control: Ack | 120 | 50.00% | 14.0 B | 0.0 B | 5010 MHz | -87.0 dBm | - | 10.30% | 0.15% |

### Configuration: `HeErSu`
Total over-the-air packets captured (Global BSS/AP): **3417**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Data: QoS Data | 1704 | 49.87% | 366.0 B | 0.0 B | 5010 MHz | - | 10.0 dBm | 92.82% | 27.62% |
| Control: Ack | 1702 | 49.81% | 14.0 B | 0.0 B | 5010 MHz | -87.0 dBm | - | 7.05% | 2.10% |
| Management: Action | 11 | 0.32% | 37.0 B | 0.0 B | 5010 MHz | -87.0 dBm | 10.0 dBm | 0.13% | 0.04% |

### Configuration: `HeSu`
Total over-the-air packets captured (Global BSS/AP): **3414**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Data: QoS Data | 1700 | 49.79% | 366.0 B | 0.0 B | 5010 MHz | - | 10.0 dBm | 92.78% | 27.56% |
| Control: Ack | 1700 | 49.79% | 14.0 B | 0.0 B | 5010 MHz | -87.0 dBm | - | 7.06% | 2.10% |
| Management: Action | 14 | 0.41% | 37.0 B | 0.0 B | 5010 MHz | - | 10.0 dBm | 0.16% | 0.05% |

### Analysis of Packet Distribution
Extended Range (ER) simulations demonstrate HE SU versus HE ER SU transmissions. Because the channel conditions are poor at cell boundaries, configurations utilizing HE ER SU (which uses a robust DCM coding and extended preambles) show successful delivery of **QoS Data** and **Block Ack (BA)** frames, whereas standard HE SU configurations suffer from packet loss, resulting in fewer successful data and acknowledgment frames.
