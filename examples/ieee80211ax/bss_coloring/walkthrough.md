# 802.11ax HE BSS Coloring & Spatial Reuse Simulation

This example illustrates the BSS Coloring mechanism introduced in the IEEE 802.11ax (Wi-Fi 6) standard. It demonstrates how Spatial Reuse (OBSS/PD) allows overlapping basic service sets (OBSS) operating on the same channel to transmit concurrently instead of backing off, resulting in higher aggregate throughput.

## Background: BSS Coloring & Spatial Reuse

Stations on the same channel normally defer when physical or virtual carrier
sense reports the medium busy. In a dense deployment, overlapping BSSs can
therefore suppress one another even when a concurrent transmission would have
been decodable. Exact CCA thresholds depend on signal type, bandwidth, and PHY
rules; they should not be reduced to one universal sensitivity value.

802.11ax introduces **BSS Coloring** to address this:
1. Every BSS is assigned a numerical identifier called a "color" (between 1 and 63), carried in the HE PHY preamble header.
2. A receiver distinguishes between **Intra-BSS** frames (same color as its own BSS) and **Inter-BSS / OBSS** frames (different color).
3. For an eligible inter-BSS PPDU, the receiver can apply the configured
   **OBSS Packet Detect (OBSS/PD)** rule instead of ordinary preamble detection.
4. If the PPDU is below the applicable threshold and the other spatial-reuse
   conditions hold, it need not keep the local medium busy. This creates an
   opportunity for concurrent transmission; it does not guarantee that the
   concurrent frames will decode successfully.

BSS coloring supplies classification information. OBSS/PD-based spatial reuse
is the policy that uses that information. The IEEE PHY also couples more
aggressive OBSS/PD operation to transmit-power constraints; this focused INET
scenario fixes power and explores the receive-side decision.

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

There are two primary configurations and four diagnostic variants in
[omnetpp.ini](omnetpp.ini):

### 1. `BssColoringDisabled`
- Spatial reuse is turned off: `**.receiver.enableSpatialReuse = false`.
- Because the signal from the other BSS arrives at -80 dBm (above -85 dBm sensitivity), the nodes will hear each other's frames, mark the channel as busy, and defer.
- **Result**: The two BSSs share the channel, resulting in serialized/alternating transmissions. The combined aggregate throughput is capped by the single-channel capacity.

### 2. `BssColoringEnabled`
- Spatial reuse is turned on: `**.receiver.enableSpatialReuse = true` with `**.receiver.obssPdThreshold = -62dBm`.
- When `ap1` transmits, `ap2` and `sta2` receive the frame with BSS Color 1. Since their own BSS Color is 2 (Inter-BSS) and the received power (-80 dBm) is below the OBSS/PD threshold (-62 dBm), the receiver ignores it.
- **Result**: `ap2` can transmit to `sta2` concurrently with `ap1`'s transmission to `sta1`. The aggregate network throughput is significantly higher.

### Diagnostic variants

- `ObssPdConservative` and `ObssPdAggressive` sweep the threshold while keeping
  topology and colors fixed. Compare aggregate goodput, per-BSS fairness, and
  reception errors: a more permissive reuse decision can also raise
  interference at the intended receiver.
- `BssColoringCollision` deliberately assigns the same color to both BSSs, so
  color alone can no longer classify the neighbor as an OBSS for this rule.
- `TwoNav` enables the HE intra-BSS/basic NAV distinction. Dual NAV protects
  virtual carrier-sense reservations; it is related to spatial reuse but is
  not the same mechanism as OBSS/PD.

---

## Running the Simulation

You can run the simulation using Qtenv or Cmdenv.

### Running with Qtenv (GUI)
Run the following command to launch the simulation in the GUI:
```sh
bin/inet -u Qtenv -c BssColoringEnabled examples/ieee80211ax/bss_coloring/omnetpp.ini
```
You can visually observe the concurrent transmissions in Qtenv when BSS Coloring is enabled.

For the underlying spatial-reuse decisions, inspect any STA radio receiver. The watches `enableSpatialReuse`, `srgBssColors`, `lastHeReceptionSummary`, `lastSpatialReuseBssTypeName`, `lastSpatialReuseEligible`, `lastSpatialReuseIgnoredPpdu`, `lastSpatialReuseObssPdThreshold`, and `lastSpatialReuseReason` show whether an inter-BSS PPDU was ignored because it was below the applicable OBSS/PD threshold.

### Running with Cmdenv (Command Line)
Run the configurations to generate results:
```sh
# Run disabled configuration
bin/inet -u Cmdenv -c BssColoringDisabled examples/ieee80211ax/bss_coloring/omnetpp.ini

# Run enabled configuration
bin/inet -u Cmdenv -c BssColoringEnabled examples/ieee80211ax/bss_coloring/omnetpp.ini

# Useful controls
bin/inet -u Cmdenv -c ObssPdConservative examples/ieee80211ax/bss_coloring/omnetpp.ini
bin/inet -u Cmdenv -c ObssPdAggressive examples/ieee80211ax/bss_coloring/omnetpp.ini
bin/inet -u Cmdenv -c BssColoringCollision examples/ieee80211ax/bss_coloring/omnetpp.ini
bin/inet -u Cmdenv -c TwoNav examples/ieee80211ax/bss_coloring/omnetpp.ini
```

---

## Verifying Results

After running the simulations, the results are stored in the `results/` folder. You can use `opp_scavetool` to compare the total received packet count at `sta1` and `sta2`.

```sh
# Query total received packets at the application layer
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.sta*"' examples/ieee80211ax/bss_coloring/results/*.sca
```

### Expected Output Summary

```
BssColoringDisabled-#0.sca:
scalar  BssColoringNetwork.sta1[0].app[0]  packetReceived:count  4
scalar  BssColoringNetwork.sta1[1].app[0]  packetReceived:count  3
scalar  BssColoringNetwork.sta2[0].app[0]  packetReceived:count  166
scalar  BssColoringNetwork.sta2[1].app[0]  packetReceived:count  178

BssColoringEnabled-#0.sca:
scalar  BssColoringNetwork.sta1[0].app[0]  packetReceived:count  143
scalar  BssColoringNetwork.sta1[1].app[0]  packetReceived:count  142
scalar  BssColoringNetwork.sta2[0].app[0]  packetReceived:count  92
scalar  BssColoringNetwork.sta2[1].app[0]  packetReceived:count  99
```

### Interpretation of Results

1. **BssColoringDisabled (Baseline)**:
   - **BSS 1** (STA1[0..1]) receives **7 packets** (4 and 3).
   - **BSS 2** (STA2[0..1]) receives **344 packets** (166 and 178).
   - *Why?* AP1 and AP2 defer to each other because the OBSS signal is received at $-80$ dBm (above the $-85$ dBm sensitivity). Due to standard CSMA/CA backoff and channel access dynamics, one BSS can dominate the channel while the other is nearly starved. The aggregate throughput is capped by the single-channel capacity (351 packets total in the current run).

2. **BssColoringEnabled (Wi-Fi 6 Spatial Reuse)**:
   - **BSS 1** (STA1[0..1]) receives **285 packets** (143 and 142).
   - **BSS 2** (STA2[0..1]) receives **191 packets** (92 and 99).
   - *Why?* With BSS Coloring enabled, both BSSs identify each other's frames as OBSS (Inter-BSS). Since the $-80$ dBm signal power is below the OBSS/PD and energy detection thresholds of $-62$ dBm, they ignore each other and transmit concurrently.
   - *Conclusion*: Neither BSS is starved, and the aggregate network throughput increases by about **36%** (from 351 packets to 476 packets total), showing the clear benefits of 802.11ax BSS Coloring in dense environments.
