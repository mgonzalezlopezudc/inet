# 802.11ax HE Advanced Features Showcase

This example illustrates the most recently added 802.11ax HE MAC/PHY features in the INET Framework:

* HE LDPC timing/accounting and LDPC PER gain
* Packet-extension (PE) timing metadata
* HE MU PHY header puncturing metadata
* HE LDPC / preamble-puncturing capability negotiation
* Validated preamble-puncturing configuration
* Puncture-aware RU allocation

All scenarios share the same network, `Lan80211AxHeFeatures`, which consists of a wired server, one access point, and four wireless stations operating on an 80 MHz channel.  80 MHz is required because HE preamble puncturing is only defined for 80 MHz and 160 MHz channels.

---

## 1. Network Configuration

The network is configured in `omnetpp.ini` under `[General]`:

* `**.opMode = "ax"`
* `**.bandName = "5 GHz (80 MHz)"` with `**.centerFrequency = 5.2GHz` and `**.channelNumber = 2`
* `**.wlan[*].radio.receiver.bandwidth = 80MHz`
* `**.mac.hcf.typename = "HeHcf"`
* `**.ap.wlan[*].mac.hcf.dlScheduler.typename = "HeDlSchedulerBacklogBased"`
  * This scheduler is used for all scenarios because it calls `allocateHeRus()` with the punctured-subchannel mask, producing a puncture-aware RU layout.
* `**.displayHeMuSignalDetails = true` and `**.displayHeMuSignalPhyFields = true`

The server sends 1000-byte downlink UDP packets to all four hosts every 0.2 ms. The resulting backlog quickly exceeds the scheduler's 1500 B threshold, so the scheduler requests large RUs (up to 484-tone, which spans a 40 MHz half of the 80 MHz channel). This makes preamble puncturing visibly force the scheduler to downgrade RU sizes so that no RU overlaps the disabled 20 MHz subchannel.

---

## 2. Simulation Scenarios

### Scenario A: `BccBaseline`

**Description:** Baseline HE MU-OFDMA with BCC coding, zero packet-extension duration, and no preamble puncturing.

**What to observe:** This is the reference point. The AP schedules all four stations concurrently. With the backlog-based scheduler, stations receive large RUs (up to 484-tone once backlog has grown). The HE MU PHY header uses `HE_CODING_BCC`, `packetExtensionDurationUs = 0`, and `puncturedSubchannelMask = 0`, so the compact (non-extended) PHY header format is used.

### Scenario B: `HeLdpc`

**Description:** Enable HE LDPC at the AP and all four stations.

**What it demonstrates:**

* **LDPC timing / accounting:** When `HE_CODING_LDPC` is selected, `Ieee80211HePhyCalculator` omits the BCC tail bits and instead computes the LDPC codeword length (648, 1296, or 1944 bits), the number of codewords, shortening bits, and repetition bits.
* **LDPC PER gain:** `Ieee80211YansErrorModel` applies a 1.5 dB SNR boost for LDPC-coded HE transmissions, improving the per-user success rate.

**Configuration:**

```ini
**.ap.wlan[*].mib.heLdpc = true
**.host[*].wlan[*].mib.heLdpc = true
```

**What to observe:**

* Slightly shorter PPDU durations because the BCC tail bits are no longer transmitted and LDPC codeword accounting is used.
* Higher received packet counts at the hosts because of the LDPC SNR boost.
* The HE MU signal details show `coding = LDPC`.

### Scenario C: `PacketExtension`

**Description:** Use an 8 µs HE packet-extension duration.

**What it demonstrates:**

* **PE metadata propagation:** The default PE duration is read from `Ieee80211Mib` (`heDefaultPeDurationUs`), copied into the scheduler's `ScheduleContext`, attached to the MU container via `Ieee80211HeMuCommonReq`, written into the `Ieee80211HeMuPhyHeader`, and finally passed to `computeHePpduParameters()` where it is added to the PPDU duration.
* **Extended HE MU PHY header:** The serializer emits the extended HE MU PHY header whenever the PE duration or puncturing mask is non-zero, preserving backward compatibility for the default case.

**Configuration:**

```ini
**.ap.wlan[*].mib.heDefaultPeDurationUs = 8
**.host[*].wlan[*].mib.heDefaultPeDurationUs = 8
```

**What to observe:**

* PPDU durations are exactly 8 µs longer than in the baseline.
* The HE MU signal details show `packetExtensionDurationUs = 8`.
* The emitted HE MU PHY header is the extended variant.

### Scenario D: `PreamblePuncturing`

**Description:** Puncture the second 20 MHz subchannel of the 80 MHz channel.

**What it demonstrates:**

* **Validated puncturing configuration:** `HeHcf::parseHePreamblePuncturing()` checks that the mask is only used for 80/160 MHz, that the primary 20 MHz subchannel remains active, and that at least one subchannel stays enabled.
* **Puncture-aware RU allocation:** `Ieee80211HeRu::allocateHeRus()` receives the punctured-subchannel mask and marks the corresponding tones as occupied before fitting RUs. No RU is placed on the disabled subchannel.
* **Puncturing metadata:** The mask is carried through the trigger / MU request tags and written into the `Ieee80211HeMuPhyHeader` as `puncturedSubchannelMask`.

**Configuration:**

```ini
**.ap.wlan[*].mac.hcf.hePreamblePuncturing = "0100"
```

Bit 0 is the primary 20 MHz subchannel; `0100` disables subchannel index 1 only.

**What to observe:**

* The scheduler cannot place the large requested RUs on the remaining three 20 MHz subchannels (a 484-tone RU needs two adjacent active 20 MHz subchannels). `fitRequestedRus()` downgrades one or more allocations to smaller RU sizes so that no RU overlaps the punctured subchannel.
* No RU overlaps the punctured subchannel.
* The HE MU signal details show `puncturedSubchannelMask = 0x4` (binary `0100`).
* The extended HE MU PHY header is emitted.

### Scenario E: `MixedLdpcSupport`

**Description:** The AP supports HE LDPC, but `host[3]` does not.

**What it demonstrates:**

* **Capability negotiation:** During association, the AP and each STA exchange `Ieee80211HeCapabilitiesElement`s. `negotiateHeCapabilities()` computes the intersection of local and peer capabilities: `ldpc = local.ldpc && peer.ldpc`. The negotiated result is stored per peer in `Ieee80211Mib`.
* **Scheduling fallback:** `HeHcf` selects `HE_CODING_LDPC` only when **all** scheduled peers support it. Any MU frame that includes `host[3]` therefore falls back to `HE_CODING_BCC`.

**Configuration:**

```ini
**.ap.wlan[*].mib.heLdpc = true
**.host[0].wlan[*].mib.heLdpc = true
**.host[1].wlan[*].mib.heLdpc = true
**.host[2].wlan[*].mib.heLdpc = true
**.host[3].wlan[*].mib.heLdpc = false
```

**What to observe:**

* HE MU frames that do not include `host[3]` use `coding = LDPC`.
* HE MU frames that include `host[3]` use `coding = BCC`.
* This demonstrates that capability negotiation is per-peer and that the scheduler respects the negotiated result.

### Scenario F: `CombinedHeFeatures`

**Description:** Enable LDPC, an 8 µs packet extension, and one punctured 20 MHz subchannel at the same time.

**What it demonstrates:** All featured mechanisms working together in a single configuration.

**Configuration:**

```ini
**.ap.wlan[*].mib.heLdpc = true
**.host[*].wlan[*].mib.heLdpc = true
**.ap.wlan[*].mib.heDefaultPeDurationUs = 8
**.host[*].wlan[*].mib.heDefaultPeDurationUs = 8
**.ap.wlan[*].mac.hcf.hePreamblePuncturing = "0100"
```

**What to observe:**

* Extended HE MU PHY header carrying `coding = LDPC`, `packetExtensionDurationUs = 8`, and `puncturedSubchannelMask = 0x4`.
* PPDU duration includes the PE contribution.
* The RU layout avoids the punctured subchannel.

---

## 3. How to Run

From the repository root, source OMNeT++ and INET, then run the desired configuration:

```sh
source /home/user/omnetpp-6.4.0/setenv -f
source setenv -q
bin/inet -u Qtenv -c HeLdpc examples/ieee80211/he_features/omnetpp.ini
```

Other useful configurations:

```sh
bin/inet -u Qtenv -c BccBaseline examples/ieee80211/he_features/omnetpp.ini
bin/inet -u Qtenv -c PacketExtension examples/ieee80211/he_features/omnetpp.ini
bin/inet -u Qtenv -c PreamblePuncturing examples/ieee80211/he_features/omnetpp.ini
bin/inet -u Qtenv -c MixedLdpcSupport examples/ieee80211/he_features/omnetpp.ini
bin/inet -u Qtenv -c CombinedHeFeatures examples/ieee80211/he_features/omnetpp.ini
```

For a batch run comparing all scenarios:

```sh
bin/inet -c BccBaseline examples/ieee80211/he_features/omnetpp.ini
bin/inet -c HeLdpc examples/ieee80211/he_features/omnetpp.ini
bin/inet -c PacketExtension examples/ieee80211/he_features/omnetpp.ini
bin/inet -c PreamblePuncturing examples/ieee80211/he_features/omnetpp.ini
bin/inet -c MixedLdpcSupport examples/ieee80211/he_features/omnetpp.ini
bin/inet -c CombinedHeFeatures examples/ieee80211/he_features/omnetpp.ini
```

---

## 4. Verification Hints

The following log or signal fields are useful for verifying each mechanism:

| Feature | Where to look |
|---|---|
| LDPC vs BCC selection | HE MU signal detail label / `Ieee80211HeMuPhyHeader::coding` |
| LDPC codeword accounting | `Ieee80211HeUserPhyParameters` (`ldpcCodewordLength`, `ldpcCodewordCount`, `ldpcShorteningBits`, `tailBits`) |
| LDPC PER gain | `udpApp[*]` packet received counts compared with `BccBaseline` |
| PE duration | `Ieee80211HeMuPhyHeader::packetExtensionDurationUs` and PPDU duration |
| Extended PHY header | `Ieee80211HeMuPhyHeaderSerializer` emits extra bytes when PE or puncturing is non-zero |
| Puncturing mask | `Ieee80211HeMuPhyHeader::puncturedSubchannelMask` and the number of scheduled users per PPDU |
| Puncture-aware allocation | RU indices in the HE MU signal details should avoid the punctured 20 MHz subchannel |
| Capability negotiation | Per-peer negotiated capabilities in `Ieee80211Mib` and the resulting `coding` field per frame |

---

## 5. Code Pointers

| Mechanism | Key files |
|---|---|
| LDPC accounting & PE timing | `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h` |
| LDPC PER boost | `src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211YansErrorModel.cc` |
| Capability negotiation | `src/inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h`, `Ieee80211Mib.cc` |
| Puncturing validation & filtering | `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc` |
| Puncture-aware RU allocation | `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h` |
| HE MU PHY header metadata | `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader.msg` |
| Metadata tags & trigger frame | `src/inet/linklayer/ieee80211/mac/Ieee80211Frame.msg`, `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag.msg` |

---

## 6. Notes and Limitations

* HE LDPC in INET is a packet-level model: it does not include a bit-level LDPC codec. Instead it models the timing and PER impact (codeword accounting, tail-bit omission, and a 1.5 dB SNR boost).
* Preamble puncturing capability is advertised by default in the current `Ieee80211HeCapabilities` struct, so all peers in this example support it. The scenario therefore emphasizes **configuration validation** and **puncture-aware allocation** rather than a peer-capability fallback.
* The example focuses on downlink MU-OFDMA. The same metadata paths (PE duration, puncturing mask, coding) are also used for uplink MU-OFDMA via the Trigger frame.
