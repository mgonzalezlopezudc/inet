# Walkthrough - HE Channel-Width Comparison

This walkthrough guides you through the contiguous HE channel width simulation example in the INET Framework, analyzing how channel bandwidth affects transmission capacity, subchannel scheduling, and client latency.

## Background: HE Channel Bandwidths and Tones

In IEEE 802.11ax (Wi-Fi 6), the OFDM subcarrier spacing is reduced to 1/4 of the legacy spacing (78.125 kHz instead of 312.5 kHz), resulting in 4 times more subcarriers (tones) for the same bandwidth. A wider channel can carry significantly more tones, which can be allocated to a single user (HE SU) or partitioned into smaller Resource Units (RUs) for multiple concurrent users (HE MU-OFDMA).

The full-bandwidth tone allocations for the supported channel widths are:
- **20 MHz**: 242-tone RU
- **40 MHz**: 484-tone RU
- **80 MHz**: 996-tone RU
- **160 MHz**: 2x996-tone RU (contiguous 160 MHz representation, not non-contiguous 80+80 MHz)

### The noise-integration trade-off
While a wider channel increases the transmission rate, it also integrates more physical noise across the receiver band. Doubling the receiver bandwidth increases the thermal noise floor by 3 dB:
- 40 MHz has 3 dB more noise than 20 MHz.
- 80 MHz has 6 dB more noise than 20 MHz.
- 160 MHz has 9 dB more noise than 20 MHz.
Therefore, wider channels do not automatically translate to increased range or better performance in low-SNR environments unless transmit power or receiver sensitivity is adjusted.

---

## Network Topology and Configuration

The simulation runs in the `HeChannelWidthsNetwork` topology consisting of:
- **`ap`**: Access Point at `(300, 200)`.
- **`sta[0..3]`**: Four stationary client hosts arranged around the AP at close range (approx. 50-80 meters).
- **`server`**: A wired server connected to the AP via a 100G Ethernet link.
- **Traffic**: Downlink UDP traffic is sent from the server to each of the four client hosts (1000B payloads sent every 0.25ms in the saturated phase, with a single-packet trigger at t = 0.2s for ADDBA warmup).

The variables under test are the channel width and its matching physical bitrate:
- **`Width20MHz`**: 20 MHz channel, 14.625 Mbps bitrate.
- **`Width40MHz`**: 40 MHz channel, 29.25 Mbps bitrate.
- **`Width80MHz`**: 80 MHz channel, 61.25 Mbps bitrate.
- **`Width160MHz`**: 160 MHz channel, 122.5 Mbps bitrate.

---

## Running the Simulation

Run the four configurations using Cmdenv:
```sh
bin/inet -u Cmdenv -c Width20MHz examples/ieee80211ax/he_channel_widths/omnetpp.ini
bin/inet -u Cmdenv -c Width40MHz examples/ieee80211ax/he_channel_widths/omnetpp.ini
bin/inet -u Cmdenv -c Width80MHz examples/ieee80211ax/he_channel_widths/omnetpp.ini
bin/inet -u Cmdenv -c Width160MHz examples/ieee80211ax/he_channel_widths/omnetpp.ini
```

---

## Verifying and Interpreting Results

Compare the total received packets and the mean end-to-end delays at the client applications:
```sh
# Query total received packets at the clients
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.host*app*"' examples/ieee80211ax/he_channel_widths/results/*.sca

# Query end-to-end delay histograms at the clients
opp_scavetool query -l -f 'name =~ "endToEndDelay:histogram"' examples/ieee80211ax/he_channel_widths/results/*.sca
```

### Quantitative Summary:
For each configuration, the server generates a single trigger packet per host during an idle warmup phase (`t = 0.2s` to `0.25s`) to establish Block Ack agreements, and then generates saturated traffic from `t = 0.3s` to `0.45s`. The five-seed analysis measures `0.3–0.43s`.

| Configuration | Aggregate goodput | p95 delay |
|---|---:|---:|
| **`Width20MHz`** | 23.94 Mbps | 100.88 ms |
| **`Width40MHz`** | 35.31 Mbps | 89.39 ms |
| **`Width80MHz`** | 66.15 Mbps | 48.64 ms |
| **`Width160MHz`** | 111.90 Mbps | 15.81 ms |

---

## PCAP Tshark Packet Exchange Analysis

To record PCAP traces and inspect them with TShark, run the simulation with PCAP recording and checksum computation enabled:

```sh
bin/inet -u Cmdenv -c Width20MHz examples/ieee80211ax/he_channel_widths/omnetpp.ini --result-dir=examples/ieee80211ax/he_channel_widths/results --**.numPcapRecorders=1 --**.checksumMode=\"computed\" --**.fcsMode=\"computed\"
```

Use TShark to print the timeline of packet exchanges:

```sh
tshark -n -r examples/ieee80211ax/he_channel_widths/results/Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap -c 20
```

The decoded output timeline shows:
1. **ADDBA Negotiation**: Before data transfer, the AP and client hosts negotiate block acknowledgment using 802.11 Action frames (e.g. frames 3, 5).
2. **Downlink UDP Packets**: The AP transmits UDP data frames to each client host (e.g. frames 1, 7, 11, 15) which are acknowledged by the client hosts via Block Ack frames.

---

## Analysis and Insights:

1. **Bandwidth vs. Wire-Time Delay**:
   - As the channel width increases from 20 MHz to 160 MHz, aggregate goodput rises from `23.94` to `111.90 Mbps` and p95 delay falls from `100.88` to `15.81 ms`. With the warm-up trigger setup, all four stations have nonempty arrival vectors in every run.
   - This occurs because a wider channel supports a higher physical bit rate, reducing the physical transmission duration (airtime) of the frame.

2. **Why these parameters make bandwidth visible**:
   - The `0.25 ms` per-flow interval keeps every width backlogged. If the load
     were below 20 MHz capacity, all four configurations would merely deliver
     the offered load and bandwidth would appear irrelevant.
   - The 1000-byte payload is large enough that PHY capacity matters more than
     per-packet MAC overhead. The 100 Gbit/s wired link makes its serialization
     delay negligible, so the measured trend comes from the wireless channel.
   - The stations are deliberately close to the AP. This is a capacity test,
     not a coverage test: at the cell edge, the 3 dB noise increase per width
     doubling can outweigh the extra tones.

## 802.11 Packet Type Statistics
![802.11 Packet Type Statistics](packet_statistics.png)

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from the Access Point's wireless interface (`ap.wlan[0]`), which captures all uplink, downlink, and management traffic in the BSS without duplication.

Two airtime occupancy percentages are provided:
- **Air Time %**: The percentage of the total transmission airtime of all packets occupied by this frame type.
- **Air Time (Sim Time) %**: The percentage of the total simulation time occupied by the transmission of this frame type (defined as the sum of physical airtimes of this frame type w.r.t. the total simulation time limit).

### Configuration: `Width160MHz`
Total over-the-air packets captured (Global BSS/AP): **815**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Control: Block Ack (BA) | 526 | 64.54% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5180 MHz, 5200 MHz, 5220 MHz, 5260 MHz, 5280 MHz, 5300 MHz | -71.7 dBm | - | 8.64% | 3.58% |
| Data: QoS Data | 137 | 16.81% | 16467.9 B | 3319.2 B | 1199.5 us | 216.8 us | 5240 MHz | - | 15.0 dBm | 87.99% | 36.52% |
| Control: Trigger | 132 | 16.20% | 63.9 B | 1.6 B | 41.3 us | 0.5 us | 5240 MHz | - | 15.0 dBm | 2.92% | 1.21% |
| Control: Ack | 12 | 1.47% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5240 MHz | -72.0 dBm | 15.0 dBm | 0.16% | 0.07% |
| Management: Action | 8 | 0.98% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5240 MHz | -72.0 dBm | 15.0 dBm | 0.30% | 0.12% |

### Configuration: `Width20MHz`
Total over-the-air packets captured (Global BSS/AP): **693**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Control: Block Ack (BA) | 444 | 64.07% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5043 MHz, 5047 MHz, 5053 MHz, 5057 MHz | -71.0 dBm | - | 4.49% | 3.03% |
| Data: QoS Data | 117 | 16.88% | 4217.3 B | 665.8 B | 2430.9 us | 364.2 us | 5050 MHz | - | 15.0 dBm | 93.71% | 63.20% |
| Control: Trigger | 112 | 16.16% | 64.0 B | 0.0 B | 41.3 us | 0.0 us | 5050 MHz | - | 15.0 dBm | 1.53% | 1.03% |
| Control: Ack | 12 | 1.73% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5050 MHz | -71.0 dBm | 15.0 dBm | 0.10% | 0.07% |
| Management: Action | 8 | 1.15% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5050 MHz | -71.0 dBm | 15.0 dBm | 0.18% | 0.12% |

### Configuration: `Width40MHz`
Total over-the-air packets captured (Global BSS/AP): **1023**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Control: Block Ack (BA) | 664 | 64.91% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5085 MHz, 5096 MHz, 5104 MHz, 5115 MHz | -71.0 dBm | - | 8.05% | 4.53% |
| Data: QoS Data | 172 | 16.81% | 4262.3 B | 553.1 B | 1308.0 us | 153.6 us | 5100 MHz | - | 15.0 dBm | 88.89% | 49.99% |
| Control: Trigger | 167 | 16.32% | 64.0 B | 0.0 B | 41.3 us | 0.0 us | 5100 MHz | - | 15.0 dBm | 2.73% | 1.53% |
| Control: Ack | 12 | 1.17% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5100 MHz | -71.0 dBm | 15.0 dBm | 0.12% | 0.07% |
| Management: Action | 8 | 0.78% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5100 MHz | -71.0 dBm | 15.0 dBm | 0.22% | 0.12% |

### Configuration: `Width80MHz`
Total over-the-air packets captured (Global BSS/AP): **959**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Control: Block Ack (BA) | 622 | 64.86% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5170 MHz, 5180 MHz, 5189 MHz, 5211 MHz, 5220 MHz, 5230 MHz | -71.5 dBm | - | 8.58% | 4.24% |
| Data: QoS Data | 161 | 16.79% | 8370.5 B | 1402.5 B | 1217.3 us | 183.2 us | 5200 MHz | - | 15.0 dBm | 88.14% | 43.55% |
| Control: Trigger | 156 | 16.27% | 63.9 B | 1.4 B | 41.3 us | 0.5 us | 5200 MHz | - | 15.0 dBm | 2.90% | 1.43% |
| Control: Ack | 12 | 1.25% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5200 MHz | -71.0 dBm | 15.0 dBm | 0.13% | 0.07% |
| Management: Action | 8 | 0.83% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5200 MHz | -71.0 dBm | 15.0 dBm | 0.25% | 0.12% |

### Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.

### Model Limitations
- **Preamble Puncturing and Interference**: While preamble puncturing is modeled by selecting non-punctured subchannels for transmissions, the channel model and radio medium do not dynamically apply localized interference/noise on a per-subchannel basis. Interference is modeled over the entire bandwidth.
