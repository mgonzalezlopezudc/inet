# Walkthrough - HE MU-MIMO (Downlink and Uplink)

This walkthrough guides you through the HE Multi-User MIMO (MU-MIMO) simulation examples in the INET Framework.

## Background: HE MU-MIMO

Multi-User Multiple Input Multiple Output (MU-MIMO) is a key feature of 802.11ax. Unlike Orthogonal Frequency Division Multiple Access (OFDMA), which partitions the channel in the frequency domain (into Resource Units), MU-MIMO allows multiple users to transmit or receive concurrently over the **entire bandwidth** by exploiting the spatial dimensions of the channel (separating users by distinct spatial streams).
- **Downlink MU-MIMO (DL MU-MIMO)**: The Access Point (AP) functions as the beamformer. Before scheduling concurrent transmissions, the AP must perform **channel sounding**:
  1. The AP transmits a **Null Data Packet Announcement (NDPA)** followed by a **Null Data Packet (NDP)**.
  2. The target stations (beamformees) measure the physical channel characteristics and compute Channel State Information (CSI).
  3. The AP polls the stations for feedback (using a BFRP Trigger frame), and the stations reply with their CSI feedback reports.
  4. The AP schedules downlink concurrent streams using the collected CSI to steer energy to each station, preventing inter-user interference.
- **Uplink MU-MIMO (UL MU-MIMO)**: Multiple stations transmit concurrently to the AP on the same full-bandwidth RU.
  1. The AP transmits a **Basic Trigger frame** assigning specific spatial streams to each station.
  2. The stations transmit their HE TB PPDUs concurrently.
  3. The AP (equipped with multiple antennas) separates and decodes the concurrent spatial streams.

---

## Network Topology and Configuration

The simulations run in single-BSS topologies (`Lan80211AxDlOfdma` and `Lan80211AxUlOfdma`) where:
- **`ap`**: Equipped with 4 or 8 antennas.
- **`host[]`**: Associated client stations.
- **`server`**: A wired server.

The MU-MIMO configurations are defined across `downlink.ini` and `uplink.ini`:

### 1. Downlink MU-MIMO (`DlMuMimo` / `DlMuMimo80MHz` in `downlink.ini`)
- **Channel**: 20 MHz or 80 MHz contiguous channel.
- **AP**: Configured with 4 or 8 antennas:
  ```ini
  **.ap.wlan[*].radio.antenna.numAntennas = 4 (or 8)
  **.ap.wlan[*].mib.heDlMuMimoBeamformer = true
  **.ap.wlan[*].mib.heSoundingDimensions = 4 (or 8)
  **.ap.wlan[*].mac.hcf.enableDlMuMimo = true
  ```
- **STAs**: Declare as beamformees capable of feeding back CSI:
  ```ini
  **.host[*].wlan[*].mib.heDlMuMimoBeamformee = true
  **.host[*].wlan[*].mib.heFeedbackMode = 2  # MU-only feedback
  **.host[*].wlan[*].mib.heBeamformeeSts20Mhz = 4
  ```

### 2. Uplink MU-MIMO (`UlMuMimo` in `uplink.ini`)
- **AP**: Enables UL MU-MIMO scheduling:
  ```ini
  **.ap.wlan[*].mib.heFullBandwidthUlMuMimo = true
  **.ap.wlan[*].mac.hcf.enableUlMuMimo = true
  **.ap.wlan[*].radio.antenna.numAntennas = 4
  ```
- **STAs**: Declare support for UL MU-MIMO:
  ```ini
  **.host[*].wlan[*].mib.heFullBandwidthUlMuMimo = true
  ```

---

## Running the Simulation

Execute the configurations using Cmdenv:
```sh
# Run 20 MHz Downlink MU-MIMO (3 users)
bin/inet -u Cmdenv -c DlMuMimo examples/ieee80211ax/multi_user/mu_mimo/downlink.ini

# Run 80 MHz Downlink MU-MIMO (8 users)
bin/inet -u Cmdenv -c DlMuMimo80MHz examples/ieee80211ax/multi_user/mu_mimo/downlink.ini

# Run 20 MHz Uplink MU-MIMO (3 users)
bin/inet -u Cmdenv -c UlMuMimo examples/ieee80211ax/multi_user/mu_mimo/uplink.ini
```

---

## Verifying Results and Code Inspection

After simulation completion, query the packet counts:
```sh
# Query received packets at the clients for DL MU-MIMO
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.host*app*"' examples/ieee80211ax/multi_user/mu_mimo/results/DlMu*.sca

# Query received packets at the server for UL MU-MIMO
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.server.app*"' examples/ieee80211ax/multi_user/mu_mimo/results/UlMu*.sca
```

### Quantitative Summary:
- **`DlMuMimo` (20 MHz, 3 STAs)**:
  - `host[0]`: 125 packets received.
  - `host[1]`: 123 packets received.
  - `host[2]`: 123 packets received.
- **`DlMuMimo80MHz` (80 MHz, 8 STAs)**:
  - `host[0..7]`: ~90-97 packets received each.
- **`UlMuMimo` (20 MHz, 3 STAs Uplink)**:
  - `server.app[0]`: 1021 packets received.

---

## PCAP Tshark Packet Exchange Analysis

To record PCAP traces and inspect them with TShark, run the simulation with PCAP recording and checksum computation enabled:

```sh
bin/inet -u Cmdenv -c DlMuMimo examples/ieee80211ax/multi_user/mu_mimo/downlink.ini --result-dir=examples/ieee80211ax/multi_user/mu_mimo/results --**.numPcapRecorders=1 --**.checksumMode=\"computed\" --**.fcsMode=\"computed\"
```

Use TShark to print the timeline of packet exchanges:

```sh
tshark -n -r examples/ieee80211ax/multi_user/mu_mimo/results/DlMuMimo-#0Lan80211AxDlOfdma.ap.wlan[0].pcap -c 20
```

The decoded output timeline shows:
1. **Downlink Data and ACKs**: The AP transmits UDP data frames to the stations (e.g. frame 1, 5, 9), which acknowledge them.
2. **Action Frame Handshake**: Stations establish block acknowledgment session configurations with the AP (e.g. frames 3, 7, 11, 13).
3. **Sounding Verification**: Under DL MU-MIMO, trace logs show NDPA, NDP, and BFRP trigger frames coordinating channel measurement and CSI feedback between AP and clients.

---

### Qtenv Watch Points:
Launch any MU-MIMO configuration in Qtenv (`-u Qtenv`):
1. **CSI Feedback**: Select the AP's `ap.wlan[0].mac.hcf.csiManager` module and inspect the `csiTable` watch. It shows the active CSI records containing spatial SNR and feedback timestamps collected from each sounding sequence.
2. **Scheduler Groups**: Inspect the HCF scheduler's `lastScheduleSummary` and `lastRuAllocations` watches. These confirm that the scheduler grouped the stations into a full-channel MU-MIMO group instead of slicing the channel into OFDMA RUs.
3. **PHY Metadata**: Select the AP radio transmitter and inspect the `lastHeTransmissionSummary` and `lastHeUserPhyParameters`. These detail the HE MU PPDU layout, spatial stream index (NSS) assigned per user, coding scheme (LDPC/BCC), and calculated duration.
