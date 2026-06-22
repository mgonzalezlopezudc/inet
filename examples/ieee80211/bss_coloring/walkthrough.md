# 802.11ax HE BSS Coloring & Spatial Reuse Simulation

This example illustrates the BSS Coloring mechanism introduced in the IEEE 802.11ax (Wi-Fi 6) standard. It demonstrates how Spatial Reuse (OBSS/PD) allows overlapping basic service sets (OBSS) operating on the same channel to transmit concurrently instead of backing off, resulting in higher aggregate throughput.

## Background: BSS Coloring & Spatial Reuse

In legacy 802.11 standards, all stations on the same channel share the channel capacity via CSMA/CA. When a station hears any preamble/frame above its Clear Channel Assessment Carrier Sense (CCA-CS) threshold (typically -82 dBm for 20 MHz), it considers the medium busy and defers transmission. In high-density deployments, this causes severe throughput degradation due to overlapping BSSs (OBSS) deferring to one another.

802.11ax introduces **BSS Coloring** to address this:
1. Every BSS is assigned a numerical identifier called a "color" (between 1 and 63), carried in the HE PHY preamble header.
2. A receiver distinguishes between **Intra-BSS** frames (same color as its own BSS) and **Inter-BSS / OBSS** frames (different color).
3. If an incoming frame is Inter-BSS, the receiver can apply a higher Carrier Sense threshold known as the **OBSS Packet Detect (OBSS/PD)** threshold (e.g., -62 dBm).
4. If the received power of the Inter-BSS frame is below the OBSS/PD threshold, the receiver ignores the frame, leaving the channel state as **IDLE** for transmission. This allows the local station to transmit concurrently.

---

## Network Topology

The network [BssColoringNetwork.ned](BssColoringNetwork.ned) consists of two overlapping BSSs:
- **BSS 1**: `ap1` located at `(150, 250)` and its associated station `sta1` at `(100, 250)`. BSS Color is set to 1.
- **BSS 2**: `ap2` located at `(350, 250)` and its associated station `sta2` at `(400, 250)`. BSS Color is set to 2.
- A wired server connects to each AP (`server1` -> `ap1` and `server2` -> `ap2`) to generate downlink UDP traffic.

```
       [server1]                   [server2]
           | (wired)                   | (wired)
           v                           v
        [ap1] <-------- 200m -------> [ap2]
        (Color 1)                   (Color 2)
           |                           |
        (50m)                       (50m)
           v                           v
        [sta1]                      [sta2]
```

### Path Loss & Power Calculations
- APs transmit at 20mW (13 dBm) on a 5 GHz 20 MHz channel (Channel 36, 5.18 GHz).
- The distance between `ap1` and `ap2` is 200 meters. The Free Space Path Loss (FSPL) at 200m is approximately 92.8 dB.
- The signal from the overlapping BSS (e.g., `ap2` heard at `ap1` or `sta1`) arrives at approximately **-80 dBm**.
- This signal power is:
  - **Above** the receiver sensitivity (-85 dBm) and legacy CCA-CS threshold.
  - **Below** the configured OBSS/PD threshold (-62 dBm).

---

## Configurations in `omnetpp.ini`

There are two main configurations defined in [omnetpp.ini](omnetpp.ini):

### 1. `BssColoringDisabled`
- Spatial reuse is turned off: `**.receiver.enableSpatialReuse = false`.
- Because the signal from the other BSS arrives at -80 dBm (above -85 dBm sensitivity), the nodes will hear each other's frames, mark the channel as busy, and defer.
- **Result**: The two BSSs share the channel, resulting in serialized/alternating transmissions. The combined aggregate throughput is capped by the single-channel capacity.

### 2. `BssColoringEnabled`
- Spatial reuse is turned on: `**.receiver.enableSpatialReuse = true` with `**.receiver.obssPdThreshold = -62dBm`.
- When `ap1` transmits, `ap2` and `sta2` receive the frame with BSS Color 1. Since their own BSS Color is 2 (Inter-BSS) and the received power (-80 dBm) is below the OBSS/PD threshold (-62 dBm), the receiver ignores it.
- **Result**: `ap2` can transmit to `sta2` concurrently with `ap1`'s transmission to `sta1`. The aggregate network throughput is significantly higher.

---

## Running the Simulation

You can run the simulation using Qtenv or Cmdenv.

### Running with Qtenv (GUI)
Run the following command to launch the simulation in the GUI:
```sh
opp_run -u Qtenv -l src/libINET.so -c BssColoringEnabled examples/ieee80211/bss_coloring/omnetpp.ini
```
You can visually observe the concurrent transmissions in Qtenv when BSS Coloring is enabled.

### Running with Cmdenv (Command Line)
Run the configurations to generate results:
```sh
# Run disabled configuration
opp_run -u Cmdenv -l src/libINET.so -c BssColoringDisabled examples/ieee80211/bss_coloring/omnetpp.ini

# Run enabled configuration
opp_run -u Cmdenv -l src/libINET.so -c BssColoringEnabled examples/ieee80211/bss_coloring/omnetpp.ini
```

---

## Verifying Results

After running the simulations, the results are stored in the `results/` folder. You can use `opp_scavetool` to compare the total received packet count at `sta1` and `sta2`.

```sh
# Query total received packets at the application layer
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.sta*"' examples/ieee80211/bss_coloring/results/*.sca
```

### Expected Output Summary

```
BssColoringDisabled-#0.sca:
scalar  BssColoringNetwork.sta1[0].app[0]  packetReceived:count  171
scalar  BssColoringNetwork.sta1[1].app[0]  packetReceived:count  171
scalar  BssColoringNetwork.sta2[0].app[0]  packetReceived:count  2
scalar  BssColoringNetwork.sta2[1].app[0]  packetReceived:count  1

BssColoringEnabled-#0.sca:
scalar  BssColoringNetwork.sta1[0].app[0]  packetReceived:count  121
scalar  BssColoringNetwork.sta1[1].app[0]  packetReceived:count  107
scalar  BssColoringNetwork.sta2[0].app[0]  packetReceived:count  145
scalar  BssColoringNetwork.sta2[1].app[0]  packetReceived:count  144
```

### Interpretation of Results

1. **BssColoringDisabled (Baseline)**:
   - **BSS 1** (STA1[0..1]) receives **342 packets** (171 each).
   - **BSS 2** (STA2[0..1]) receives **3 packets** (2 and 1).
   - *Why?* AP1 and AP2 defer to each other because the OBSS signal is received at $-80$ dBm (above the $-85$ dBm sensitivity). Due to standard CSMA/CA backoff and channel access dynamics, BSS 1 dominates the channel and completely starves BSS 2. The aggregate throughput is capped by the single-channel capacity (345 packets total).

2. **BssColoringEnabled (Wi-Fi 6 Spatial Reuse)**:
   - **BSS 1** (STA1[0..1]) receives **228 packets** (121 and 107).
   - **BSS 2** (STA2[0..1]) receives **289 packets** (145 and 144).
   - *Why?* With BSS Coloring enabled, both BSSs identify each other's frames as OBSS (Inter-BSS). Since the $-80$ dBm signal power is below the OBSS/PD and energy detection thresholds of $-62$ dBm, they ignore each other and transmit concurrently.
   - *Conclusion*: Neither BSS is starved, and the aggregate network throughput increases by **50%** (from 345 packets to 517 packets total), showing the clear benefits of 802.11ax BSS Coloring in dense environments.
