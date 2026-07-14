# Walkthrough - HE Channel-Width Comparison

This walkthrough guides you through the contiguous HE channel width simulation example in the INET Framework, analyzing how channel bandwidth affects transmission capacity, subchannel scheduling, and client latency.

## Background: HE Channel Bandwidths and Tones

In IEEE 802.11ax (Wi-Fi 6), the OFDM subcarrier spacing is reduced to 1/4 of the legacy spacing (78.125 kHz instead of 312.5 kHz), resulting in 4 times more subcarriers (tones) for the same bandwidth. A wider channel can carry significantly more tones, which can be allocated to a single user (HE SU) or partitioned into smaller Resource Units (RUs) for multiple concurrent users (HE MU-OFDMA).

The full-bandwidth tone allocations for the supported channel widths are:
- **20 MHz**: 242-tone RU
- **40 MHz**: 484-tone RU
- **80 MHz**: 996-tone RU
- **160 MHz**: 2x996-tone RU (contiguous 160 MHz representation, not non-contiguous 80+80 MHz)

### The Noise Integration Penalty:
While a wider channel increases the transmission rate, it also integrates more physical noise across the receiver band. Doubling the receiver bandwidth increases the thermal noise floor by 3 dB:
- 40 MHz has 3 dB more noise than 20 MHz.
- 80 MHz has 6 dB more noise than 20 MHz (and a corresponding 10 dB noise integration penalty is typically offset by increasing transmitter power).
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
For each configuration, the server generates a single trigger packet per host during an idle warmup phase (`t = 0.2s` to `0.25s`) to establish Block Ack agreements, and then generates saturated traffic from `t = 0.3s` to `0.5s` (800 packets per flow, total 3200 packets). Under high saturation, the scheduler distributes RUs equitably across all configurations:

| Configuration | `host[0]` (Pkts / Delay) | `host[1]` (Pkts / Delay) | `host[2]` (Pkts / Delay) | `host[3]` (Pkts / Delay) |
|---|---|---|---|---|
| **`Width20MHz`** | 114 / 60.51 ms | 113 / 61.30 ms | 113 / 61.31 ms | 113 / 61.32 ms |
| **`Width40MHz`** | 167 / 52.28 ms | 166 / 52.81 ms | 166 / 52.81 ms | 166 / 52.82 ms |
| **`Width80MHz`** | 312 / 32.41 ms | 312 / 32.41 ms | 312 / 32.41 ms | 311 / 32.68 ms |
| **`Width160MHz`**| 528 / 9.92 ms  | 528 / 9.93 ms  | 528 / 9.93 ms  | 528 / 9.93 ms  |

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
   - As the channel width increases from 20 MHz to 160 MHz, the mean delay for all active stations under high traffic drops consistently (e.g., for `host[0]`: 60.51 ms at 20 MHz to 52.28 ms at 40 MHz, 32.41 ms at 80 MHz, and 9.92 ms at 160 MHz). With the warmup trigger setup, all four stations are successfully scheduled concurrently without any station being starved.
   - This occurs because a wider channel supports a higher physical bit rate, reducing the physical transmission duration (airtime) of the frame.

2. **Negligible Sequential Delay with 100G Ethernet**:
   - In earlier configurations using a 100M Ethernet link, the packets for the four stations were serialized one after another on the wire, introducing a staggered arrival/queueing delay at the AP MAC layer (`host[0] < host[1] < host[2] < host[3]`).
   - With the 100G Ethernet link, the transmission delay per packet is extremely small (around 16 ns for a 200B packet). This makes the Ethernet serialization delay negligible, so the packets arrive at the AP almost simultaneously, effectively eliminating this staggered queueing delay.
