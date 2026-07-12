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
- **`server`**: A wired server connected to the AP via a 100M Ethernet link.
- **Traffic**: Downlink UDP traffic is sent from the server to each of the four client hosts (200B payloads sent every 2ms).

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
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.sta*app*"' examples/ieee80211ax/he_channel_widths/results/*.sca

# Query mean end-to-end delay at the clients
opp_scavetool query -l -f 'name =~ "endToEndDelay:histogram" and field =~ "mean"' examples/ieee80211ax/he_channel_widths/results/*.sca
```

### Quantitative Summary:
For all configurations, the server generates 400 packets per flow (total 1600 packets). Every client receives **exactly 400 packets** (or 399 in some runs), showing 100% delivery. Since the offered load is well below the capacity of all widths, we compare the **mean end-to-end delay** to observe the performance gains:

| Configuration | `sta[0]` Delay | `sta[1]` Delay | `sta[2]` Delay | `sta[3]` Delay |
|---|---|---|---|---|
| **`Width20MHz`** | 0.273 ms | 0.541 ms | 0.809 ms | 1.077 ms |
| **`Width40MHz`** | 0.178 ms | 0.350 ms | 0.522 ms | 1.435 ms |
| **`Width80MHz`** | 0.113 ms | 0.222 ms | 0.330 ms | 0.438 ms |
| **`Width160MHz`**| 0.097 ms | 0.190 ms | 0.281 ms | 1.129 ms |

### Analysis and Insights:

1. **Bandwidth vs. Wire-Time Delay**:
   - As the channel width increases, the mean delay for the first station (`sta[0]`) drops significantly: **0.273 ms** (20 MHz) $\rightarrow$ **0.178 ms** (40 MHz) $\rightarrow$ **0.113 ms** (80 MHz) $\rightarrow$ **0.097 ms** (160 MHz).
   - This occurs because a wider channel supports a higher physical bit rate, reducing the physical transmission duration (airtime) of the frame.

2. **Sequential Delay and Ethernet Serialization**:
   - Within each run, the delays increase sequentially: `sta[0] < sta[1] < sta[2] < sta[3]`.
   - This is a didactically interesting artifact of **Ethernet serialization** on the server-to-AP link. The server generates packets for the four stations at the exact same simulation time. However, because the Ethernet link is serial, the packets must be transmitted one after another.
   - Packet 0 is placed on the wire first, while Packet 3 must wait for Packets 0, 1, and 2 to finish transmitting over the Ethernet cable. This creates a staggered queueing arrival time at the AP MAC layer, translating directly to sequential end-to-end delays at the client applications.
