# Complete IEEE 802.11ax DL MU-OFDMA Migration

## Summary

Migrate the existing simplified HE downlink OFDMA implementation into a standards-aware model that:

- Represents every IEEE 802.11ax RU size and valid RU allocation layout for 20, 40, 80, and 160 MHz channels.
- Provides `EqualSizedRUs`, `BacklogBased`, and `HoLMinDelay` scheduling policies.
- Maintains four physical FIFO queues—one per access category—for every STA associated with the AP.
- Supports multiple MPDUs per scheduled STA through per-user A-MPDU payloads.
- Estimates reciprocal path loss from received uplink signal power without reading peer module internals.
- Selects a separate HE MCS for every STA/RU using an SNR lookup table targeting approximately 10% PER.
- Keeps the total PPDU transmit power fixed and uses constant power spectral density across RUs.
- Preserves existing SU transmission, legacy rate control, EDCA contention, and Block Ack behavior outside HE MU-OFDMA.

## 1. Standard RU Representation and Allocation Layouts

Replace the current `channelBandwidth / numberOfUsers` approximation with a table-driven IEEE 802.11ax RU model.

Each `Ieee80211HeRu` must identify:

- RU tone size: 26, 52, 106, 242, 484, 996, or 2×996.
- Standard RU allocation index/position.
- Channel-width segment and frequency placement.
- Center frequency and occupied bandwidth.
- Data and pilot subcarrier counts.
- Optional allocation encoding needed by the HE MU PHY header.

Use these nominal RU bandwidths based on the 78.125 kHz HE subcarrier spacing:

| RU | Nominal occupied bandwidth |
|---|---:|
| 26 tones | 2.03125 MHz |
| 52 tones | 4.0625 MHz |
| 106 tones | 8.28125 MHz |
| 242 tones | 18.90625 MHz |
| 484 tones | 37.8125 MHz |
| 996 tones | 77.8125 MHz |
| 2×996 tones | 155.625 MHz |

The equal-sized layouts are:

| Channel | Valid equal-sized partitions |
|---|---|
| 20 MHz | 9×26, 4×52, 2×106, 1×242 |
| 40 MHz | 18×26, 8×52, 4×106, 2×242, 1×484 |
| 80 MHz | 37×26, 16×52, 8×106, 4×242, 2×484, 1×996 |
| 160 MHz | 74×26, 32×52, 16×106, 8×242, 4×484, 2×996, 1×2×996 |

Mixed-size allocation must not be implemented as arbitrary bin packing. Build a catalog or allocation tree directly from the standard HE RU allocation encodings, including central 26-tone RU cases and recursively subdivided 20/40/80 MHz regions. A scheduler may return only layouts present in this catalog.

Provide common utilities to:

- Enumerate valid layouts for a channel width.
- Find equal-sized layouts.
- Test whether requested RUs fit a layout.
- Allocate a requested RU at a valid free position.
- Replace an RU with its valid child layout.
- Merge adjacent sibling RUs into their valid parent.
- Validate non-overlap, full channel bounds, unique indices, and legal allocation encodings.

The PHY and radio medium must consume the complete RU descriptions from HE MU metadata instead of reconstructing RUs from the number of users.

## 2. Scheduler Contract

Replace the address-only scheduling interface with a context-rich contract.

```cpp
struct HeDlCandidateInfo {
    MacAddress staAddress;
    AccessCategory accessCategory;
    bool anchor;
    int64_t backlogBytes;
    int64_t holPacketBytes;
    simtime_t holEnqueueTime;
    simtime_t holDelay;
    double pathLossDb;
    bool hasFreshPathLoss;
};

struct HeDlScheduleContext {
    std::vector<HeDlCandidateInfo> candidates;
    MacAddress anchorSta;
    Hz channelCenterFrequency;
    Hz channelBandwidth;
    simtime_t txopLimit;
    W totalTransmitPower;
    double noiseFigureDb;
};

struct HeDlRuAllocation {
    MacAddress staAddress;
    Ieee80211HeRu ru;
    int mcs;
    double estimatedSnrDb;
    simtime_t estimatedDuration;
};
```

The scheduler result must be deterministic. Unless a policy defines a stronger ordering, ties use:

1. Anchor first.
2. Larger HoL delay.
3. Larger backlog.
4. MAC address ascending.

The destination of the earliest MU-eligible packet in the EDCAF that won contention is the mandatory `anchorSta`. The anchor must remain scheduled whenever a valid MU allocation exists.

Only QoS data with an active originator Block Ack agreement is MU-eligible. If fewer than two valid users remain after grouping and validation, fall back to the existing SU HCF path without mutating queues.

## 3. `EqualSizedRUs` Scheduler

Keep `HeDlSchedulerEqualSizedRUs` and add:

```ned
string schedulingFunction = default("fBW"); // fBW or fHoL
int maxMuStations = default(-1);             // -1 means channel-layout maximum
```

The scheduler supports both functions described by Kuran et al.

### 3.1 `fBW`: maximize bandwidth use

Choose the equal-sized standard layout with the smallest RU count that:

- Does not exceed the number of candidate STAs.
- Serves as many candidates as possible.
- Avoids creating unassigned RUs.

For a 20 MHz channel:

| Candidate users | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | ≥10 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Number of RUs | 1 | 2 | 2 | 4 | 4 | 4 | 4 | 4 | 9 | 9 |

This means:

- 1 user → one 242-tone RU.
- 2–3 users → two 106-tone RUs.
- 4–8 users → four 52-tone RUs.
- 9 or more users → nine 26-tone RUs.

Generalize the same rule using each wider channel’s equal-layout RU counts.

### 3.2 `fHoL`: minimize HoL delay

Choose the smallest-RU equal layout whose RU count is at least the candidate count, capped by the channel maximum. This may leave empty RUs and increase padding, but allows more queues to be drained.

For a 20 MHz channel:

| Candidate users | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | ≥10 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Number of RUs | 1 | 2 | 4 | 4 | 9 | 9 | 9 | 9 | 9 | 9 |

Generalize this ceiling-to-next-valid-layout rule to 40, 80, and 160 MHz.

Reference:

- Mehmet S. Kuran et al., “Throughput-maximizing OFDMA Scheduler for IEEE 802.11ax Networks,” IEEE PIMRC, 2020, pp. 1–7.

## 4. `BacklogBased` Scheduler

Add `HeDlSchedulerBacklogBased`.

Its objective is to allocate RU sizes proportionally to pending bytes while reducing slow-user TXOP stretching and padding overhead.

### 4.1 Candidate discovery and anchor grouping

Start with STAs that have pending traffic in the winning AC. The anchor is always admitted.

Retrieve the anchor’s latest path-loss estimate. Admit another STA only when:

\[
|PL_i-PL_{\text{anchor}}| \leq \Delta PL_{\max}
\]

Default:

```ned
double deltaPlMax = default(10dB);
```

If the anchor lacks a fresh estimate, admit candidates without path-loss filtering for that TXOP rather than blocking traffic.

Rank admitted non-anchor STAs by:

1. Path-loss difference from the anchor, ascending.
2. Backlog, descending.
3. HoL delay, descending.
4. MAC address.

Limit the selected group to the 26-tone RU capacity or configured `maxMuStations`:

- 20 MHz: 9 users.
- 40 MHz: 18 users.
- 80 MHz: 37 users.
- 160 MHz: 74 users.

### 4.2 Initial RU request from backlog

For each selected STA with backlog \(L_i\):

| Backlog | Requested RU |
|---|---|
| \(L_i \leq 80\) bytes | 26 tones |
| \(80 < L_i \leq 500\) bytes | 52 tones |
| \(500 < L_i \leq 1500\) bytes | 106 tones |
| \(1500 < L_i \leq 6000\) bytes | 242 tones for 20/40 MHz; 484 tones for 80/160 MHz |
| \(L_i > 6000\) bytes | Largest RU supported by the channel |

The largest requests are:

- 20 MHz: 242 tones.
- 40 MHz: 484 tones.
- 80 MHz: 996 tones.
- 160 MHz: 2×996 tones.

Expose all byte thresholds as NED parameters while preserving these defaults.

### 4.3 Standard partition fitting

Select a valid standard allocation layout using this deterministic procedure:

1. Sort candidates by requested RU size descending, keeping the anchor first among equal requests.
2. Search the standard allocation catalog for layouts that can satisfy every request.
3. Prefer the layout with:
   - Most scheduled candidates.
   - Most satisfied requested sizes.
   - Greatest occupied tone count.
   - Lowest predicted duration variance.
4. If no layout fits, reduce the largest non-anchor request by one RU level.
5. If necessary, reduce the anchor only after all equally large non-anchor requests have been reduced.
6. Repeat until a valid layout is found.
7. Never silently omit the anchor.
8. If fewer than two candidates can be fitted, return no MU schedule and trigger SU fallback.

The RU downgrade chain is:

\[
2{\times}996 \rightarrow 996 \rightarrow 484 \rightarrow 242
\rightarrow 106 \rightarrow 52 \rightarrow 26
\]

### 4.4 Duration alignment

Estimate each user duration as:

\[
T_i=T_{\text{preamble}}+T_{\text{header}}+
\frac{\text{scheduled payload bits}_i}{R_{\text{net}}(\text{RU}_i,\text{MCS}_i)}
\]

Iteratively reduce:

\[
\operatorname{Var}(T_1,\ldots,T_n)
\]

using only valid layout-tree operations.

For each iteration:

- If \(T_i > 1.5\overline{T}\), attempt to enlarge that STA’s RU by merging valid adjacent sibling regions.
- If \(T_i < 0.5\overline{T}\), attempt to shrink its RU and make the released region available to a slower user.
- Accept a transformation only if:
  - The result is a valid standard layout.
  - Every selected STA retains at least a 26-tone RU.
  - The anchor remains scheduled.
  - Duration variance decreases.
- Stop when no improving transformation exists or after a bounded number of iterations.

This corrects the direction in the illustrative pseudocode: a user whose duration is too long normally needs a larger RU, while a user whose duration is too short may relinquish bandwidth.

References:

- A. Baraa et al., “Dynamic Resource Unit Allocation for Uplink OFDMA in IEEE 802.11ax,” IEEE Access, vol. 9, 2021.
- K. Kosek-Szott et al., “A Survey on Resource Allocation in IEEE 802.11ax WLANs,” IEEE Communications Surveys & Tutorials, vol. 23, 2021.

## 5. `HoLMinDelay` Scheduler

Add `HeDlSchedulerHoLMinDelay`.

The scheduler minimizes latency and prevents starvation.

### 5.1 Candidate priority

Inspect the enqueue time of the first MU-eligible packet in each STA’s queue for the winning AC.

Sort candidates by:

1. Anchor first.
2. HoL queuing delay descending.
3. HoL packet size descending.
4. Backlog descending.
5. MAC address.

The anchor is mandatory even when another STA has an older packet.

### 5.2 RU sizing

Use the size of the specific HoL MPDU—not total queue backlog—as the initial RU-sizing input:

| HoL packet size | Requested RU |
|---|---|
| ≤80 bytes | 26 tones |
| 81–500 bytes | 52 tones |
| 501–1500 bytes | 106 tones |
| 1501–6000 bytes | 242 tones for 20/40 MHz; 484 tones for 80/160 MHz |
| >6000 bytes | Largest channel-supported RU |

Apply the same standard partition-fitting mechanism as `BacklogBased`, but when capacity is insufficient, preserve older HoL packets before younger packets.

### 5.3 Duration alignment

Calculate duration using the HoL packet and selected per-RU MCS. Optimize variance with the same valid split/merge operations used by `BacklogBased`.

After every STA’s HoL MPDU has been included, additional MPDUs may be packed into that STA’s A-MPDU if they fit the aligned PPDU duration. Additional packing must not cause another selected STA’s HoL packet to be removed.

## 6. Per-STA, Per-AC Queue Architecture

At the AP, create a visible dynamic queue-bank module for every associated STA.

Each station queue bank contains exactly four FIFO queues:

```text
StationQueueBank[STA]
├── AC_BK
├── AC_BE
├── AC_VI
└── AC_VO
```

Required behavior:

- Create the station queue bank when association succeeds.
- For simplified management, where stations may be registered directly, create it during registration or lazily upon the first packet to that associated STA.
- Route each unicast QoS MPDU to the destination STA’s queue selected through the standard TID-to-AC mapping:
  - TID 1,2 → AC_BK.
  - TID 0,3 → AC_BE.
  - TID 4,5 → AC_VI.
  - TID 6,7 → AC_VO.
- Preserve FIFO order inside each STA/AC queue.
- Keep management, multicast, broadcast, and non-QoS traffic in shared legacy queues.
- Provide an aggregate per-AC facade to EDCA so each AC still performs one contention process while the HE scheduler can inspect all station queues belonging to that AC.
- The first MU-eligible packet exposed by the winning AC facade determines the anchor.
- Retries return to the front of the same STA/AC queue while preserving retry state and sequence number.
- Remove a station queue bank on disassociation/deauthentication after dropping its queued traffic through normal packet-drop signals.
- Expose per-queue packet and byte counts, HoL delay, enqueue/dequeue statistics, and drops.
- Make per-STA/per-AC packet capacity configurable; default to the current effective queue capacity unless explicitly overridden.

Packets must retain their original MAC-entry enqueue timestamp. Use a dedicated queue-entry timestamp tag or equivalent state so retry requeueing does not reset the original HoL age.

## 7. Per-User A-MPDU Packing

The current one-MPDU-per-RU representation must be replaced with a per-user PSDU that may contain multiple MPDUs.

For every selected STA:

1. Start with its HoL MU-eligible QoS MPDU.
2. Continue selecting FIFO packets from the same STA, AC, and active Block Ack agreement.
3. Pack MPDUs until one of these limits is reached:
   - The aligned common PPDU duration.
   - Remaining TXOP duration.
   - HE PPDU duration/length limit.
   - Configured per-user A-MPDU limit.
   - Block Ack window limit.
4. Add padding so all user transmissions end at the common HE MU PPDU duration.
5. Record every MPDU in the frame-sequence active-allocation state.
6. Assign sequence numbers only when needed and before transmission.
7. Process Block Ack bitmaps per MPDU.
8. Complete acknowledged MPDUs normally.
9. Requeue unacknowledged MPDUs at the front of their original STA/AC queue unless retry limits are reached.
10. Preserve existing recovery counters and packet-drop behavior.

The HE MU payload metadata must describe each user PSDU length rather than assuming one MPDU length.

## 8. RSSI, Path-Loss, and Link-State Tracking

### 8.1 RSSI tracking

Whenever the AP successfully receives an uplink:

- QoS or non-QoS Data frame.
- Management frame.
- Block Ack frame.

read the attached `SignalPowerInd` tag and convert its received power to dBm:

\[
P_{\text{RX,dBm}}=10\log_{10}(P_{\text{RX,W}}/1\text{mW})
\]

Do not update the estimate from corrupted frames or packets without a valid power indication.

### 8.2 STA transmit-power capability

Extend association information with the STA’s nominal transmit power in dBm.

- The normal STA management implementation populates it from the configured radio transmitter power.
- The association request serializer/deserializer carries the field.
- The AP stores it in the station record.
- Simplified management registers the configured value directly when available.
- If no capability is registered, use:

```ned
double fallbackStaTransmitPower = default(15dBm);
```

Do not read another node’s internal modules during scheduling.

### 8.3 Reciprocal path loss

Calculate:

\[
PL_i=P_{\text{TX,STA},i}-P_{\text{RX},i}
\]

Store per associated STA:

- Advertised transmit power.
- Latest received signal power.
- Estimated path loss.
- Last update time.
- Whether the estimate is valid.

Assume channel reciprocity:

\[
PL_{\text{DL},i}=PL_{\text{UL},i}
\]

Expose a configurable stale-estimate interval. A stale estimate remains observable but is not used for strict path-loss grouping or high MCS selection.

## 9. RU Noise and SNR Estimation

Use:

\[
N_{\text{RU,dBm}}=-174
+10\log_{10}(B_{\text{RU,Hz}})
+NF_{\text{dB}}
\]

where:

- \(-174\) dBm/Hz is the thermal-noise power spectral density at 290 K.
- \(B_{\text{RU}}\) is the RU bandwidth.
- \(NF\) is the receiver noise figure.

Use constant PSD as the default downlink power model.

For total AP PPDU power \(P_{\text{total}}\) and channel bandwidth \(B_{\text{channel}}\):

\[
P_{\text{TX,RU},i}
=P_{\text{total}}
\frac{B_{\text{RU},i}}{B_{\text{channel}}}
\]

Then:

\[
P_{\text{RX,RU},i}=P_{\text{TX,RU},i}-PL_i
\]

\[
SNR_{\text{RU},i}=P_{\text{RX,RU},i}-N_{\text{RU}}
\]

This keeps total PPDU power fixed and avoids increasing aggregate transmitted power when more users are scheduled.

Expose the RU power policy through an internal interface so future experiments can add equal-power allocation, but only constant PSD is required and enabled by default.

If no fresh path-loss estimate exists:

- Use MCS 0 for that STA.
- Permit transmission so the link can establish observations.
- Do not exclude the mandatory anchor solely because its estimate is absent.

## 10. Proactive HE SNR-to-MCS Selection

Legacy SU traffic continues using configured rates or retry-based ARF, AARF, Onoe, Minstrel, and related rate-control modules.

HE MU-OFDMA uses proactive per-user lookup because reactive retry-based adaptation is too slow for dynamically changing RU widths.

For each allocated RU:

1. Estimate `SNR_RU`.
2. Select the highest HE MCS whose threshold does not exceed that SNR.
3. Limit selection to MCS 0–11 and one spatial stream for this migration.
4. Store the chosen MCS in the scheduler result and HE MU user metadata.
5. Use the same MCS for duration calculation and receiver error evaluation.

Default thresholds target approximately 10% PER.

The threshold vector must be configurable:

```ned
string heMcsSnrThresholds;
```

Threshold provenance:

- Digitize 10%-PER crossings from published conventional IEEE 802.11ax receiver PER-vs-SNR curves where available.
- Use the conventional receiver curves rather than DeepWiPHY’s learned receiver results.
- For missing MCS values, perform monotonic interpolation based on adjacent modulation/coding schemes.
- Document each threshold as:
  - Directly digitized.
  - Interpolated.
  - The source figure and scenario.
- Validate that thresholds are strictly nondecreasing.
- Keep the complete threshold table in one documented source file and unit test.

Primary curve reference:

- Yi Zhang et al., “DeepWiPHY: Deep Learning-based Receiver Design and Dataset for IEEE 802.11ax Systems,” IEEE Transactions on Wireless Communications, 2020/2021. The paper treats approximately 10% PER as a usable-link threshold and includes conventional-receiver PER curves for selected HE MCS values.

Because the source does not provide a uniform MCS 0–11 table under identical assumptions, interpolated defaults are an explicit modeling assumption rather than claimed standard constants.

## 11. HE MU PHY and Radio-Medium Changes

Extend HE MU user information so every user carries:

- STA ID.
- RU tone size.
- RU allocation position/index.
- RU center-frequency or standard allocation encoding.
- MCS.
- PSDU length.
- Padding length if needed.

The transmitter must:

- Use a common HE MU preamble and PPDU duration.
- Calculate each user’s data duration using that user’s RU and MCS.
- Set total PPDU duration to the longest padded user duration.
- Preserve one over-the-air PPDU and one fixed total transmit power.

The radio medium must:

- Resolve the exact assigned RU from metadata.
- Create a reception using that RU’s center frequency and bandwidth.
- Apply constant-PSD RU power.
- Compute interference/noise over the RU bandwidth.
- Evaluate the user payload using its selected MCS rather than a PPDU-wide data MCS.
- Deliver only the PSDU assigned to the receiving STA.
- Continue exposing the common legacy preamble/NAV indication to non-target STAs.

Serializer tests must cover every RU size and reject invalid RU positions, MCS values, lengths, or overlapping allocations.

## 12. Configuration and Example Scenarios

Add scheduler module types:

```ned
HeDlSchedulerEqualSizedRUs
HeDlSchedulerBacklogBased
HeDlSchedulerHoLMinDelay
```

Update the OFDMA example with configurations for:

- Equal-sized `fBW`.
- Equal-sized `fHoL`.
- Backlog-based.
- HoL-minimum-delay.
- 20, 40, 80, and 160 MHz channels.
- Different STA path losses.
- Different backlog distributions.
- Different packet ages and ACs.

Important parameters include:

```ned
string schedulingFunction = default("fBW");
int maxMuStations = default(-1);
double deltaPlMax = default(10dB);

int smallBacklogThreshold = default(80B);
int mediumBacklogThreshold = default(500B);
int mtuBacklogThreshold = default(1500B);
int largeBacklogThreshold = default(6000B);

double lowDurationRatio = default(0.5);
double highDurationRatio = default(1.5);

double thermalNoisePsd = default(-174dBmHz);
double receiverNoiseFigure;
double fallbackStaTransmitPower = default(15dBm);
simtime_t linkEstimateMaxAge;
string ruPowerPolicy = default("constantPsd");

string heMcsSnrThresholds;
int perStaAcPacketCapacity;
int maxAmpduMpduCount;
```

## 13. Testing and Acceptance Criteria

### RU model

- Enumerate and validate every supported standard allocation for each channel width.
- Verify equal layouts and mixed layouts.
- Verify central 26-tone cases.
- Reject overlap, out-of-channel placement, invalid merges, and unsupported tone sizes.
- Verify exact PHY serialization round trips.

### Equal-sized scheduler

- Reproduce the complete supplied 20 MHz `fBW` and `fHoL` tables.
- Verify generalized ceiling/floor behavior for 40/80/160 MHz.
- Verify candidate caps, anchor preservation, and deterministic ordering.

### Backlog scheduler

- Test every threshold boundary: 80, 500, 1500, and 6000 bytes.
- Test recursive RU downgrading and valid-layout selection.
- Test 10 dB anchor grouping.
- Test absent and stale link estimates.
- Verify duration variance never increases after an accepted alignment operation.
- Verify all selected layouts remain standards-valid.

### HoL scheduler

- Verify oldest-first service.
- Verify mandatory-anchor precedence.
- Verify starvation prevention over repeated TXOPs.
- Verify RU requests use HoL MPDU size rather than total backlog.
- Verify additional aggregate packing cannot displace a selected HoL MPDU.

### Queue bank

- Verify dynamic creation on association and removal on disassociation.
- Verify four queues per associated STA.
- Verify TID-to-AC mapping.
- Verify FIFO ordering and original enqueue timestamps.
- Verify retry return to the correct queue.
- Verify shared handling of management and group-addressed traffic.

### Link estimation and MCS

- Verify `SignalPowerInd` processing for Data, Management, and Block Ack.
- Verify path-loss calculation and 15 dBm fallback.
- Verify stale-estimate handling.
- Verify constant-PSD RU power and bandwidth-specific noise.
- Verify every MCS threshold boundary and MCS 0 fallback.
- Verify per-user MCS reaches the HE header, duration model, and receiver error model.

### A-MPDU and Block Ack

- Verify multiple MPDUs per STA/RU.
- Verify TXOP, PPDU, Block Ack window, and aggregate limits.
- Verify common duration and padding.
- Verify partial Block Ack success.
- Verify only failed MPDUs are retried.
- Verify retry limits and packet-drop signals.

### Regression and integration

Build with:

```sh
-j$(nproc)
```

Run from the repository root with:

```sh
export CCACHE_DISABLE=1
source /home/user/omnetpp-6.4.0/setenv -f
source setenv -q
bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
```

Also run the existing validation scripts and OFDMA example for every scheduler and channel width.

Acceptance requires:

- No regression in legacy SU, EDCA, rate-control, ACK, or Block Ack tests.
- No arbitrary/nonstandard RU allocation emitted.
- At least two active users for every MU PPDU.
- Scheduler duration estimates matching the generated PHY PPDU within symbol-rounding tolerance.
- Correct packet ownership and no packet loss during failed MU assembly or SU fallback.

## Assumptions and Scope

- This migration covers downlink HE OFDMA.
- Trigger-based uplink OFDMA and uplink scheduling remain out of scope.
- MU-MIMO and multiple spatial streams remain out of scope.
- Equal-sized scheduling defaults to `fBW`.
- The AP winning AC’s earliest MU-eligible destination is the anchor.
- Every associated AP client receives four visible dynamic AC queues.
- Per-user A-MPDU payloads are implemented because they are standard-supported and necessary for meaningful backlog scheduling.
- Constant PSD with fixed total PPDU power is the default power model.
- MCS defaults target approximately 10% PER but are modeling parameters, not values mandated by IEEE 802.11ax.
- Existing one-PPDU HE MU operation, sequential Block Ack flow, visualization, and SU fallback are retained and extended rather than replaced wholesale.
