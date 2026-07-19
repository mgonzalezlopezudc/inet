# 802.11ax Uplink OFDMA and UORA

This example answers two related questions:

1. How does an AP coordinate simultaneous uplink transmissions on separate
   resource units (RUs)?
2. How does Uplink OFDMA Random Access (UORA) let stations contend *inside* a
   Trigger opportunity, and what changes when the AP offers more RA-RUs?

The important distinction is who wins access. With EDCA, stations contend for
the whole channel. With scheduled UL OFDMA, the AP names a station in each RU.
With UORA, the AP advertises RUs with AID 0 and eligible stations use an OFDMA
backoff (OBO) counter before selecting one of them.

## Read the exchange first

A scheduled UL OFDMA exchange has this shape:

```text
AP                 STA 1                 STA 2
 |---- Trigger ------>|-------------------->|
 |<--- HE TB PPDU ----|<--- HE TB PPDU -----|  simultaneous, different RUs
 |---- Block Ack ---->|-------------------->|
```

For UORA, the Trigger does not name a particular associated station in the
RA-RU. Each STA maintains an OFDMA contention window (OCW) and OBO counter:

1. The Trigger advertises one or more eligible RA-RUs.
2. The STA decrements OBO by the number of eligible RA-RUs.
3. When eligible, it selects one advertised RA-RU uniformly at random.
4. A success resets OCW; a failed attempt expands OCW and draws a new OBO.

These are the modeled consequences of IEEE Std 802.11-2024 Clauses
26.5.4.1–26.5.4.3 (`80211ax-2024:chunk:09810`–`09812`). More RA-RUs can
accelerate OBO countdown and reduce collision pressure, but they also consume
channel capacity that could have been assigned to scheduled users. Therefore,
the useful quantity is not the attempt count alone: inspect successful UORA
transmissions and per-STA fairness as well.

---

## Network Topology

The network [Lan80211AxUlOfdma.ned](Lan80211AxUlOfdma.ned) extends the common
single-BSS HE network. The AP is at `(25m,25m)`, a wired server is behind it,
and all application traffic flows from the wireless stations to the server.

```
 [host 0]  [host 1]  ...  [host N]
      \        |              /
       +-------+-------------+
                    |
                  [ AP ]---100G Ethernet---[server]
```

The scheduler examples use three STAs, 1000-byte application packets, and a
`5ms` interval. Their fixed positions are 5 m from the AP. The UORA experiment
uses eight STAs placed uniformly in the central 20 m square and 100-byte
packets. A 26-tone RA-RU at the robust default MCS cannot carry the general
1000-byte workload within the modeled HE PPDU duration limit, so the smaller
packet deliberately isolates access contention from fragmentation.

All conditions send one warm-up packet at `0.2s` to establish Block Ack state,
start measured traffic at `0.3s`, and stop at `2s`.

---

## Configurations in `omnetpp.ini`

The [omnetpp.ini](omnetpp.ini) file contains several adjacent HE feature
demonstrations. This walkthrough deliberately considers the following seven
configs; every row appears in the run-0 scalar check and in the generated PCAP
statistics below.

| Config | STAs and load | UL access | RA-RUs | Teaching role |
|---|---|---|---:|---|
| `EdcaBaseline` | 3, 1000 B / 5 ms | EDCA only | 0 | Non-triggered control |
| `ScheduledOnly` | 3, 1000 B / 5 ms | Backlog-scheduled | 0 | Scheduled RU allocation |
| `EqualRus` | 3, 1000 B / 5 ms | Equal-sized scheduled RUs | 0 | Scheduler-shape comparison |
| `MixedUora` | 3, 1000 B / 5 ms | Scheduled + UORA | 1–3 | Mixed-access exchange |
| `UoraLightContention` | 8, 100 B / 4 ms | Scheduled + UORA | 1 | Observable, lighter contention |
| `UoraHeavyContention` | 8, 100 B / 1 ms | Scheduled + UORA | 1 | Load-pressure control |
| `UoraMoreRandomAccessRus` | 8, 100 B / 1 ms | Scheduled + UORA | 5 | RA-RU capacity treatment |

`UoraHeavyContention` and `UoraMoreRandomAccessRus` differ only in RA-RU
count. That matched pair is the clean test of the parameter. Five 26-tone
RA-RUs were selected after a 1/3/5/7/9-RU sweep: five exposes a large UORA
capacity and fairness gain while retaining scheduled capacity; seven and nine
begin trading too much of the 20 MHz channel away from scheduled users.

Configs for UL MU-MIMO, operating-mode indication, fragmentation, NDP
feedback, and Multi-TID Block Ack remain available in `omnetpp.ini`, but they
are outside this walkthrough's comparison and no claims about them are mixed
into the results below.

---

## Running the Simulation

Run one config with Cmdenv from the repository root:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/ul_ofdma/omnetpp.ini \
  -c UoraMoreRandomAccessRus -r 0
```

Regenerate the five-seed UORA comparison and provenance-tracked figure:

```sh
python3 examples/ieee80211ax/analysis/run_campaign.py uora -j$(nproc)
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/first_tranche.py uora
```

---

## Verifying Results

The dashboard uses `heUlRandomAccessAttempt:count` and
`heUlRandomAccessSuccess:count` from all eight STAs. Each bar is a mean over
runs 0–4; error bars are two-sided 95% Student-t confidence intervals.

```sh
opp_scavetool query -l \
  -f 'name =~ "heUlRandomAccessAttempt:count" OR name =~ "heUlRandomAccessSuccess:count"' \
  examples/ieee80211ax/ul_ofdma/results/Uora*.sca
```

![Five-seed UORA comparison](../analysis/figures/uora/uora-dashboard.png)

| Condition | Success probability | Successful transmissions | Success fairness |
|---|---:|---:|---:|
| Light, 1 RA-RU | 0.675 ± 0.051 | 27.6 ± 18.4 | 0.726 ± 0.125 |
| Heavy, 1 RA-RU | 0.599 ± 0.068 | 45.8 ± 27.3 | 0.651 ± 0.168 |
| Heavy, 5 RA-RUs | 0.684 ± 0.025 | 214.2 ± 128.6 | 0.890 ± 0.049 |

The one-RU comparison shows the effect of offered load: heavy traffic creates
more attempts and successes but lowers the success probability. Holding heavy
load fixed and increasing the allocation to five RA-RUs produces 4.7 times as
many successful UORA transmissions and substantially improves the fairness of
successes across STAs. This demonstrates increased random-access capacity; it
does not claim that every workload should reserve five RUs.

---

## PCAP Tshark Packet Exchange Analysis

Regenerate the run-0 AP captures, packet-type plot, and generated appendix with:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/analyze_pcap_types.py \
  --generate --subdir ul_ofdma
```

The generator records `mac` at `ap.wlan[0]`, computes checksums/FCS, and uses
PCAPng. Capture session `20260719T115224Z` contains one AP observation point
for every config in the coverage table. For example:

```sh
tshark -n -r \
  'examples/ieee80211ax/ul_ofdma/results/packet-statistics/20260719T115224Z/UoraMoreRandomAccessRus/UoraMoreRandomAccessRus-#0Lan80211AxUlOfdma.ap.wlan[0].pcap' \
  -c 20
```

The fresh run-0 scalar and PCAP evidence covers all seven considered configs:

| Config | Server packets | Basic Triggers | BSRP Triggers | UORA attempts | UORA successes | AP observations |
|---|---:|---:|---:|---:|---:|---:|
| `EdcaBaseline` | 1023 | 0 | 0 | 0 | 0 | 2483 |
| `ScheduledOnly` | 630 | 679 | 2 | 0 | 0 | 4668 |
| `EqualRus` | 630 | 679 | 2 | 0 | 0 | 4668 |
| `MixedUora` | 1000 | 411 | 102 | 40 | 40 | 4432 |
| `UoraLightContention` | 3119 | 273 | 101 | 31 | 22 | 10311 |
| `UoraHeavyContention` | 4500 | 172 | 101 | 74 | 50 | 11718 |
| `UoraMoreRandomAccessRus` | 5315 | 180 | 101 | 300 | 205 | 12458 |

This table is a coverage check, not a seven-way throughput ranking: the first
four rows use three STAs and 1000-byte packets, whereas the UORA rows use eight
STAs and 100-byte packets. The valid matched performance comparisons are light
versus heavy at one RA-RU, and heavy one-RU versus heavy five-RU.

Frame statistics establish which protocol-visible exchange occurred. They do
not by themselves identify an AID-0 RA-RU attempt or prove whether two STAs
collided. The UORA attempt/success scalars provide that direct model evidence.

---

## Interpretation of Results

Start with the controls. `EdcaBaseline` emits no Trigger frames. The scheduled
configs emit hundreds, proving that the AP-controlled path is active. This
small three-STA workload is an exchange demonstration, not evidence that UL
OFDMA always beats EDCA: Trigger overhead and narrower per-user RUs can outweigh
coordination gains when only a few stations are active.

Then compare the matched UORA rows. One RA-RU is a contention bottleneck under
heavy load. Five RA-RUs make OBO counters expire faster and give eligible STAs
more choices, producing many more successes and a more even distribution
across stations. The success probability rises only modestly; the striking
gain is *capacity per sequence of Trigger opportunities*.

Finally, keep the evidence boundary explicit:

- A Trigger count proves AP coordination, not successful payload delivery.
- A QoS frame in an AP capture proves an AP-side observation, not an
  application-level delivery by itself.
- UORA is probabilistic; use multiple seeds and confidence intervals.
- Reserving more RA-RUs is a tradeoff. The five-RU result is specific to this
  eight-STA mixed scheduled/random-access workload.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
## 802.11 Packet Type Statistics
![802.11 Packet Type Statistics](packet_statistics.png)

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260719T115224Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %**: The sum of this frame type's estimated airtimes divided by the simulation time limit. Concurrent transmissions from multiple capture points are counted separately, so this value can exceed 100%; it is not the union of busy channel time.

### Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | EdcaBaseline produced protocol-visible wireless observations | 2483 AP/global transmission observations |
| **PASS** | EqualRus produced protocol-visible wireless observations | 4668 AP/global transmission observations |
| **PASS** | MixedUora produced protocol-visible wireless observations | 4432 AP/global transmission observations |
| **PASS** | ScheduledOnly produced protocol-visible wireless observations | 4668 AP/global transmission observations |
| **PASS** | UoraHeavyContention produced protocol-visible wireless observations | 11718 AP/global transmission observations |
| **PASS** | UoraLightContention produced protocol-visible wireless observations | 10311 AP/global transmission observations |
| **PASS** | UoraMoreRandomAccessRus produced protocol-visible wireless observations | 12458 AP/global transmission observations |

### Configuration: `EdcaBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2483**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24c219" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC] | 1460 | 58.80% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -62.9 dBm | - | 95.31% | 45.35% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#5e93e8" /></svg> | Control: Ack [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1023 | 41.20% | 14.0 B | 0.0 B | 43.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 4.69% | 2.23% |

### Configuration: `EqualRus`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4668**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24c219" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC] | 640 | 13.71% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.7 dBm | - | 30.80% | 19.88% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14690c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC] | 2037 | 43.64% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -63.7 dBm | - | 62.91% | 40.60% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d28a04" /></svg> | Control: Trigger [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 681 | 14.59% | 46.1 B | 1.5 B | 39.0 us | 0.1 us | 5010 MHz | - | 10.0 dBm | 2.06% | 1.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0621d0" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 680 | 14.57% | 58.0 B | 0.0 B | 39.8 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.10% | 1.35% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#5e93e8" /></svg> | Control: Ack [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 630 | 13.50% | 14.0 B | 0.0 B | 43.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.13% | 1.38% |

### Configuration: `MixedUora`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4432**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24c219" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC] | 1401 | 31.61% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.5 dBm | - | 66.87% | 43.52% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1a7c13" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 60 | 1.35% | 34.0 B | 0.0 B | 121.3 us | 0.0 us | 5005 MHz, 5015 MHz | -64.2 dBm | - | 0.56% | 0.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14690c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC] | 741 | 16.72% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz | -63.6 dBm | - | 22.69% | 14.77% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480e" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 206 | 4.65% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -63.8 dBm | - | 3.44% | 2.24% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d28a04" /></svg> | Control: Trigger [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 512 | 11.55% | 51.4 B | 10.8 B | 39.4 us | 0.7 us | 5010 MHz | - | 10.0 dBm | 1.55% | 1.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0621d0" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 512 | 11.55% | 46.9 B | 3.2 B | 39.1 us | 0.2 us | 5010 MHz | - | 10.0 dBm | 1.54% | 1.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#5e93e8" /></svg> | Control: Ack [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1000 | 22.56% | 14.0 B | 0.0 B | 43.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.35% | 2.18% |

### Configuration: `ScheduledOnly`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4668**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24c219" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC] | 640 | 13.71% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.7 dBm | - | 45.57% | 19.88% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1a7c13" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 657 | 14.07% | 34.0 B | 0.0 B | 121.3 us | 0.0 us | 5005 MHz | -61.4 dBm | - | 9.14% | 3.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14690c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC] | 78 | 1.67% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz | -64.5 dBm | - | 3.56% | 1.55% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480e" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 1302 | 27.89% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz, 5017 MHz | -64.8 dBm | - | 32.43% | 14.15% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d28a04" /></svg> | Control: Trigger [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 681 | 14.59% | 46.1 B | 1.5 B | 39.0 us | 0.1 us | 5010 MHz | - | 10.0 dBm | 3.05% | 1.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0621d0" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 680 | 14.57% | 58.0 B | 0.0 B | 39.8 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.10% | 1.35% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#5e93e8" /></svg> | Control: Ack [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 630 | 13.50% | 14.0 B | 0.0 B | 43.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.15% | 1.38% |

### Configuration: `UoraHeavyContention`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **11718**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#23bf18" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 9669 | 82.51% | 166.2 B | 0.9 B | 92.2 us | 7.0 us | 5010 MHz | -59.3 dBm | - | 78.69% | 44.57% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24c219" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC] | 324 | 2.76% | 195.0 B | 147.9 B | 142.7 us | 80.9 us | 5010 MHz | -58.8 dBm | - | 4.08% | 2.31% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1daa22" /></svg> | Data: QoS Data [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 18 | 0.15% | 168.0 B | 2.0 B | 437.6 us | 22.5 us | 5005 MHz, 5015 MHz | -62.0 dBm | - | 0.70% | 0.39% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1a7c13" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC] | 113 | 0.96% | 34.0 B | 0.0 B | 121.3 us | 0.0 us | 5005 MHz, 5015 MHz | -58.4 dBm | - | 1.21% | 0.69% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14690c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC] | 301 | 2.57% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz | -53.7 dBm | - | 10.59% | 6.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480e" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 16 | 0.14% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -60.0 dBm | - | 0.31% | 0.17% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d28a04" /></svg> | Control: Trigger [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 273 | 2.33% | 56.0 B | 13.0 B | 39.7 us | 0.9 us | 5010 MHz | - | 10.0 dBm | 0.96% | 0.54% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c88037" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 355 | 3.03% | 24.0 B | 0.0 B | 37.6 us | 0.0 us | 5010 MHz | -57.7 dBm | - | 1.18% | 0.67% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0621d0" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 273 | 2.33% | 48.2 B | 4.6 B | 39.2 us | 0.3 us | 5010 MHz | - | 10.0 dBm | 0.94% | 0.53% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#11289c" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 170 | 1.45% | 32.0 B | 0.0 B | 38.1 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.57% | 0.32% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#5e93e8" /></svg> | Control: Ack [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 172 | 1.47% | 14.0 B | 0.0 B | 43.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.66% | 0.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3598e3" /></svg> | Control: Ack [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 16 | 0.14% | 14.0 B | 0.0 B | 36.9 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.05% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c71b0f" /></svg> | Management: Action [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 18 | 0.15% | 37.0 B | 0.0 B | 38.4 us | 0.0 us | 5010 MHz | -59.0 dBm | 10.0 dBm | 0.06% | 0.03% |

### Configuration: `UoraLightContention`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **10311**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#23bf18" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 7747 | 75.13% | 167.2 B | 1.8 B | 94.0 us | 9.8 us | 5010 MHz | -58.2 dBm | - | 71.80% | 36.42% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24c219" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC] | 414 | 4.02% | 189.6 B | 131.2 B | 139.7 us | 71.8 us | 5010 MHz | -56.1 dBm | - | 5.70% | 2.89% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14690c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC] | 403 | 3.91% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz | -55.3 dBm | - | 15.84% | 8.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480e" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 2 | 0.02% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5003 MHz | -66.0 dBm | - | 0.04% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d28a04" /></svg> | Control: Trigger [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 374 | 3.63% | 53.3 B | 12.0 B | 39.5 us | 0.8 us | 5010 MHz | - | 10.0 dBm | 1.46% | 0.74% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c88037" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 664 | 6.44% | 24.0 B | 0.0 B | 37.6 us | 0.0 us | 5010 MHz | -57.1 dBm | - | 2.46% | 1.25% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0621d0" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 373 | 3.62% | 46.7 B | 2.8 B | 39.1 us | 0.2 us | 5010 MHz | - | 10.0 dBm | 1.44% | 0.73% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#11289c" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 274 | 2.66% | 32.0 B | 0.0 B | 38.1 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 1.03% | 0.52% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#5e93e8" /></svg> | Control: Ack [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 27 | 0.26% | 14.0 B | 0.0 B | 43.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.12% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3598e3" /></svg> | Control: Ack [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 16 | 0.16% | 14.0 B | 0.0 B | 36.9 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.06% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c71b0f" /></svg> | Management: Action [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 17 | 0.16% | 37.0 B | 0.0 B | 38.4 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.06% | 0.03% |

### Configuration: `UoraMoreRandomAccessRus`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **12458**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#23bf18" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC, A-MPDU] | 10139 | 81.39% | 166.3 B | 1.0 B | 92.4 us | 7.5 us | 5010 MHz | -59.2 dBm | - | 73.40% | 46.85% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24c219" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, BCC] | 301 | 2.42% | 196.9 B | 153.3 B | 143.7 us | 83.8 us | 5010 MHz | -57.2 dBm | - | 3.39% | 2.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14690c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC] | 533 | 4.28% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz, 5014 MHz, 5016 MHz, 5018 MHz | -56.1 dBm | - | 16.65% | 10.62% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480e" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC] | 145 | 1.16% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5003 MHz, 5007 MHz | -58.7 dBm | - | 2.47% | 1.58% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d28a04" /></svg> | Control: Trigger [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 281 | 2.26% | 71.1 B | 1.4 B | 40.7 us | 0.1 us | 5010 MHz | - | 10.0 dBm | 0.90% | 0.57% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c88037" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 445 | 3.57% | 24.0 B | 0.0 B | 37.6 us | 0.0 us | 5010 MHz | -58.4 dBm | - | 1.31% | 0.84% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0621d0" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 281 | 2.26% | 54.8 B | 14.9 B | 39.6 us | 1.0 us | 5010 MHz | - | 10.0 dBm | 0.87% | 0.56% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#11289c" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 248 | 1.99% | 32.0 B | 0.0 B | 38.1 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.74% | 0.47% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#5e93e8" /></svg> | Control: Ack [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 52 | 0.42% | 14.0 B | 0.0 B | 43.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.18% | 0.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3598e3" /></svg> | Control: Ack [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, LDPC] | 16 | 0.13% | 14.0 B | 0.0 B | 36.9 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.05% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c71b0f" /></svg> | Management: Action [HE-SU, HE-MCS 11, 20 MHz, GI 3.2 us, BCC] | 17 | 0.14% | 37.0 B | 0.0 B | 38.4 us | 0.0 us | 5010 MHz | -58.2 dBm | 10.0 dBm | 0.05% | 0.03% |

### Analysis of Packet Distribution
`EdcaBaseline` provides the non-triggered control. The scheduled and mixed-access configurations contain repeated **Trigger** frames, solicited HE-TB observations, and AP **Block Ack** responses, which is the expected HE UL-MU exchange structure (IEEE Std 802.11-2024, Clause 26.5.2 and Annex G.5). The three UORA configurations expose load and RA-RU-count effects, but frame-subtype counts alone cannot distinguish an AID-0 random-access attempt from scheduled access or prove a collision. Use the per-STA `heUlRandomAccessAttempt` and `heUlRandomAccessSuccess` scalars for that decision evidence.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->
