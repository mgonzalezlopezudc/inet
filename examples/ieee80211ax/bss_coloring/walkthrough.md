# Walkthrough - HE BSS Coloring & Spatial Reuse Simulation

This walkthrough guides you through the HE BSS Coloring and Spatial Reuse simulation example in the INET Framework.

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
- **BSS 1**: `ap1` at `(150, 250)` and associated `sta1[0..1]` at `(100, 240/260)`. Color is set to 1.
- **BSS 2**: `ap2` at `(400, 250)` and associated `sta2[0..1]` at `(450, 240/260)`. Color is set to 2.
- Wired servers generate downlink UDP traffic to the client hosts (1000B payloads sent every 5ms). All conditions use a `0.2–0.25s` warm-up trigger and start normal traffic at `0.3s`.

At a distance of 200m between the two APs, the signal from the overlapping BSS arrives at approximately **-80 dBm**.
- This is **above** the receiver sensitivity threshold of -85 dBm.
- This is **below** the configured OBSS/PD threshold of -62 dBm.

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
   - Both BSSs use BSS Color 1. Spatial reuse fails because all frames are classified as Intra-BSS.
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
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.sta*app*"' examples/ieee80211ax/bss_coloring/results/*.sca
```

### Quantitative Summary:

| Configuration | Aggregate goodput | Jain fairness | Concurrent AP airtime |
|---|---:|---:|---:|
| **BssColoringDisabled** | 6.4 Mbps | 1.0 | 49.8% |
| **BssColoringEnabled** | 6.4 Mbps | 1.0 | 49.8% |
| **ObssPdConservative** | 6.4 Mbps | 1.0 | 49.8% |
| **ObssPdAggressive** | 6.4 Mbps | 1.0 | 49.8% |
| **BssColoringCollision** | 6.4 Mbps | 1.0 | 49.8% |

---

## PCAP Tshark Packet Exchange Analysis

To record PCAP traces and inspect them with TShark, run the simulation with PCAP recording and checksum computation enabled:

```sh
bin/inet -u Cmdenv -c BssColoringEnabled examples/ieee80211ax/bss_coloring/omnetpp.ini --result-dir=examples/ieee80211ax/bss_coloring/results --**.numPcapRecorders=1 --**.checksumMode=\"computed\" --**.fcsMode=\"computed\"
```

Use TShark to print the timeline of packet exchanges at the first Access Point (`ap1`):

```sh
tshark -n -r examples/ieee80211ax/bss_coloring/results/BssColoringEnabled-#0BssColoringNetwork.ap1.wlan[0].pcap -c 20
```

The refreshed run-0 captures are nonempty PCAPng files with 362 frames at
each AP. TShark shows the same warm-up/data/action/ACK exchange pattern at
both observation points. The decoded output timeline shows:
1. **Downlink UDP Packets**: `ap1` sends UDP data frames to its stations (e.g. frame 1, 15).
2. **Action Frame Handshake**: Stations establish block acknowledgment session configurations with their AP (e.g. frames 3, 5, 7, 11).
3. **Capture limitation**: The native MAC captures do not expose the full
   OBSS/PD decision or distinguish every medium observation. Together with the
   identical `.sca/.vec` metrics for all five manifest conditions, this run
   does not support claiming a spatial-reuse throughput benefit.

---

## Two NAV (Dual NAV) Verification

To verify the Dual NAV (`TwoNav`) configuration, query the NAV transition vectors in the result files:
- **`nav` (Basic NAV)**: Records transitions of the basic NAV timer (source `navChanged`).
- **`intraBssNavChanged`**: Records transitions of the intra-BSS NAV timer (source `intraBssNavChanged`).

Query the vector metrics in scavetool to see how the client stations update their separate NAV timers:
1. In `BssColoringEnabled` (without TwoNav), every inter-BSS virtual carrier reservation updates the single main `nav:vector`, forcing the station to defer.
2. In `TwoNav`, inter-BSS frame reservations update only the Basic NAV (monitored via `nav:vector`), while local intra-BSS reservations update the Intra-BSS NAV (monitored via `intraBssNavChanged:vector`). This separation allows the MAC layer to ignore Basic NAV updates during eligible spatial-reuse transmission opportunities. (Note: Since there are no client-to-client transmissions in this downlink-heavy scenario, the `intraBssNavChanged` signal remains un-triggered/idle).
