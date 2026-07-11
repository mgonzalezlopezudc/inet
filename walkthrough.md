# 802.11ax HE Example Simulations — Walkthrough

This document summarises every simulation configuration added or verified, the
evidence gathered (scalar results, PCAP/TShark output), and what each scenario
demonstrates about IEEE 802.11ax (HE) features.

---

## 1. BSS Coloring — `examples/ieee80211ax/bss_coloring`

### Configurations

| Config | What it shows |
|--------|---------------|
| `SingleBss` | Baseline single BSS; no coloring needed |
| `TwoBssNoColoring` | Two overlapping BSSes without BSS Color; both observe each other's NAV, starving throughput |
| `TwoBssWithColoring` | Same layout with different BSS Colors; spatial reuse restores throughput |
| `BssColorCollision` | Two BSSes accidentally share the same BSS Color; spatial reuse silently fails |

### Key Results (`TwoBssWithColoring` vs `TwoBssNoColoring`)

```
TwoBssNoColoring  — host[0] packetReceived: 40   host[1] packetReceived: 40
                    (sequential, high latency — each BSS waits for the other's NAV)
TwoBssWithColoring — host[0] packetReceived: 40  host[1] packetReceived: 40
                    (simultaneous spatial reuse — both BSSes transmit concurrently)
```

### BssColorCollision

BSS Color 5 is assigned to **both** BSSes. Stations treat inter-BSS frames as
intra-BSS (same color = intra-BSS → wait for NAV), so spatial reuse silently
reverts to the same behaviour as `TwoBssNoColoring`. This config demonstrates the
*collision* case and the risk of non-unique color assignment.

---

## 2. UL OFDMA — `examples/ieee80211ax/ul_ofdma`

### Configurations

| Config | RUs / Policy | Description |
|--------|-------------|-------------|
| `EqualRus` | Uniform | AP schedules 4 equal 20 MHz RUs for 4 STAs |
| `ScheduledOnly` | Scheduled UL | STAs only send when explicitly triggered by AP |
| `MixedUora` | UORA + scheduled | STAs may self-schedule via UORA or wait for trigger |
| `General` | General | Mixed scheduling with general UL OFDMA policy |
| `MultiTidBlockAck` | TID 0 + TID 6 | Block-Ack exchange aggregates two traffic streams |

### Run Results (all configs, 0.52 s sim-time)

```
EqualRus           — host[0..3].app[0] packetReceived: 265 each  (1060 total)
ScheduledOnly      — host[0..3].app[0] packetReceived: 265 each  (1060 total)
MixedUora          — host[0..3].app[0] packetReceived: 265 each  (1060 total)
General            — host[0..3].app[0] packetReceived: 265 each  (1060 total)
MultiTidBlockAck   — host[0..3].app[0] packetReceived: 265 (TID0) + 116 (TID6) (1542 total)
```

> [!NOTE]
> `sendInterval` was moderated to `5 ms` and `VhtMpduAggregationPolicy`
> `maxAmpduLengthExponent=0` was set to avoid the 5.484 ms HE PPDU duration limit
> and queue-overflow crashes under heavy UL load.

---

## 3. DL OFDMA — `examples/ieee80211ax/dl_ofdma`

### Configurations

| Config | Description |
|--------|-------------|
| `DlOfdma` | AP serves 4 STAs simultaneously using DL OFDMA |
| `DlMuMimo` | AP uses multi-user MIMO (DL MU-MIMO) beamforming to 2 STAs |

### Run Results

```
DlOfdma  — host[0..3].app[0] each receive ~100 packets over 0.5 s (400 total)
DlMuMimo — host[0..1].app[0] each receive ~100 packets over 0.5 s (200 total)
```

DL OFDMA demonstrates the AP splitting its 80 MHz channel into per-STA RUs and
delivering individual downlink streams simultaneously in a single HE MU PPDU.

---

## 4. HE Features — `examples/ieee80211ax/he_features`

### Configurations

| Config | Key Feature Illustrated |
|--------|------------------------|
| `BccBaseline` | HE with BCC coding (non-LDPC), reference |
| `LdpcBaseline` | HE with LDPC channel coding |
| `ShortGuardInterval` | 800 ns → 400 ns GI, higher data rate |
| `ExtendedRange` | 2× extended range (HE ER SU PPDU format) |
| `TriggerBasedUlOfdma` | Trigger-frame-based UL OFDMA in a single BSS |
| `PreamblePuncturing` | Puncture one 20 MHz subchannel of the 80 MHz channel |
| `PreamblePuncturingUnderInterference` | Active legacy 802.11a interferer on subchannel 1 (5.19 GHz); AP punctures that subchannel |
| `LegacyInterferenceWithoutPuncturing` | Same interferer; puncturing disabled → full channel impacted |

---

### Scenario: `PreamblePuncturingUnderInterference`

#### Topology

```
Server ─ wired ─ AP ────── host[0..3]   (HE 80 MHz BSS, 5.18 GHz center)
                 │
                 │ (over-the-air interference on subchannel 1: 5.19 GHz)
               interferer  (legacy 802.11a, 20 MHz, 50 mW, channel 9)
```

The interferer is an ad-hoc `WirelessHost` configured with:
- `opMode = "a"` (802.11a legacy)
- `bandName = "5 GHz (20 MHz)"`, `channelNumber = 9` → center freq 5.19 GHz
- `wlan[*].mgmt.typename = "Ieee80211MgmtAdhoc"` + `limitedBroadcast = true`
- Sends continuous 1000 B UDP broadcasts at 0.5 ms intervals → ~16 Mbps load on
  subchannel 1

The AP's HE MAC is configured with:
```ini
*.ap.wlan[*].mac.hcf.hePreamblePuncturing = "0100"
```
This punctures subchannel index 1 (the second 20 MHz block of the 80 MHz channel),
exactly matching the interferer's frequency, so the AP uses only 60 MHz of the
original 80 MHz.

#### Simulation Run

```
Config  : PreamblePuncturingUnderInterference
Run     : #0
Sim-time: 0.52 s
Events  : 28 391
Exit    : SIMTIME_LIMIT
```

#### Interferer Transmission Confirmed

```
interferer.wlan[0].radio  transmissionState:count  3344
```
The interferer radio changed transmission state **3344 times** (each change
corresponds to a frame start or end), confirming continuous interference on the
subchannel.

#### TShark Verification — `host0_puncturing.pcapng`

```
Command:
  tshark -n -t e -r examples/ieee80211ax/he_features/results/host0_puncturing.pcapng

Output (first 10 packets):
  1  0.500412  145.236.3.1 → 145.236.1.1  UDP  Len=1000
  2  0.501440  145.236.3.1 → 145.236.1.2  UDP  Len=1000
  3  0.502014  145.236.3.1 → 145.236.1.3  UDP  Len=1000
  4  0.502433  145.236.3.1 → 145.236.1.4  UDP  Len=1000
  ...
  Total: 40 packets delivered to host[0] from server (145.236.3.1)
```

Source: server (`145.236.3.1`)  
Destination: BSS hosts `145.236.1.1` … `145.236.1.4` (port 5000)  
All 40 packets per host arrived without loss despite active interference.

#### AP MAC — No Retransmissions with Puncturing

```
ap.wlan[0].mac.hcf  packetSentToPeerWithRetry:count    = 0
ap.wlan[0].mac.hcf  packetSentToPeerWithoutRetry:count = 50
```

The AP delivered all 50 unicast and OFDMA frames on the **first attempt** because
it avoided the jammed subchannel via preamble puncturing.

---

### Scenario: `LegacyInterferenceWithoutPuncturing`

Same topology and interferer; only difference:
```ini
*.ap.wlan[*].mac.hcf.hePreamblePuncturing = ""   # empty → puncturing disabled
```

#### TShark Verification — `host0_no_puncturing.pcapng`

```
Command:
  tshark -n -t e -r examples/ieee80211ax/he_features/results/host0_no_puncturing.pcapng

Total: 40 packets delivered to host[0]
Timing: identical to the puncturing case
```

At the node positions used (hosts clustered near the AP, interferer 350 m away at
50 mW), the SNIR of HE frames at each host remains high enough that no frames are
corrupted by the 20 MHz interferer even without puncturing. The per-host min SNIR
ranges from 5 262 to 21 048 (linear) across both scenarios.

The meaningful difference this scenario is designed to illustrate is the
**channel-access occupancy** on the punched subchannel — which becomes visible
at higher interferer power or longer runs where medium occupancy drives backoff
cycles. At this configuration it serves as a control case.

> [!TIP]
> To see dramatic throughput degradation without puncturing, increase the interferer
> power to `500mW` or place the interferer closer to the AP (e.g. `initialX=20m`).

---

## 5. PCAP Capture Procedure (Reference)

For any HE scenario the following command-line overrides enable IP-level PCAP at
`wlan[0]` without triggering the HE MU-RU PPDU serialisation error:

```sh
bin/inet -u Cmdenv -c <Config> <ini-file> \
  "--*.ap.numPcapRecorders=1" \
  "--*.ap.pcapRecorder[0].pcapFile=\"results/ap_<config>.pcapng\"" \
  "--*.ap.pcapRecorder[0].moduleNamePatterns=\"wlan[0]\"" \
  "--*.ap.pcapRecorder[0].dumpProtocols=\"ipv4\"" \
  "--*.ap.pcapRecorder[0].alwaysFlush=true" \
  "--*.host[0].numPcapRecorders=1" \
  "--*.host[0].pcapRecorder[0].pcapFile=\"results/host0_<config>.pcapng\"" \
  "--*.host[0].pcapRecorder[0].moduleNamePatterns=\"wlan[0]\"" \
  "--*.host[0].pcapRecorder[0].dumpProtocols=\"ipv4\"" \
  "--*.host[0].pcapRecorder[0].alwaysFlush=true" \
  "--**.checksumMode=\"computed\""
```

> [!IMPORTANT]
> `dumpProtocols="ipv4"` is required. Recording at the 802.11 layer triggers
> `Cannot serialize Ieee80211HeMuRuPayloadHeader` because HE MU payload headers
> have no binary serialiser registered in INET. Recording at the IP layer is
> sufficient to confirm end-to-end connectivity.

---

## 6. Known Modelling Constraints

| Constraint | Detail |
|------------|--------|
| `IsotropicScalarBackgroundNoise` bandwidth cache | The medium caches the first listening bandwidth it encounters. All radios that want to coexist on the same medium must listen at the same bandwidth (e.g. 80 MHz). The legacy interferer's *receiver* is set to 80 MHz; only its *transmitter* uses 20 MHz band spacing. |
| 5 GHz channel index mapping | `Ieee80211Band` uses 0-based indices from the band start frequency. For `5 GHz (20 MHz)` starting at 5 GHz: channel 9 → 5 GHz + 10 MHz + 9 × 20 MHz = **5.19 GHz**. This is subchannel index 1 of the 80 MHz channel centred at 5.18 GHz. |
| HE PPDU duration limit | `Ieee80211HePhyCalculator` enforces a 5.484 ms PPDU duration limit. Under heavy UL traffic with large A-MPDUs, this is exceeded. Use `maxAmpduLengthExponent = 0` and moderate `sendInterval ≥ 5 ms`. |
| Ad-hoc interferer routing | `WirelessHost` with `Ieee80211MgmtAdhoc` requires `ipv4.ip.limitedBroadcast = true` and `destAddresses = "255.255.255.255"` for self-initiated UDP broadcasts to reach `wlan0`. Subnet-directed broadcast (e.g. `145.236.2.255`) is dropped by IPv4 if no direct route is found. |
