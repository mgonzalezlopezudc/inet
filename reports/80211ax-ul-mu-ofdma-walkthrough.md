# 802.11ax UL MU OFDMA Implementation Walkthrough

> Historical note: this walkthrough predates the immutable TXVECTOR/RXVECTOR,
> transactional UL plan, standards-layout Trigger serializer, and separate HE
> TB recipient context. It is retained for development history. The
> authoritative boundary and migration table are in
> [80211ax-supported-feature-matrix.md](80211ax-supported-feature-matrix.md);
> current runnable behavior is documented in
> [the UL OFDMA example](../examples/ieee80211ax/ul_ofdma/walkthrough.md).

This document describes how the INET codebase implements the uplink multi-user OFDMA (UL MU OFDMA) features of IEEE 802.11ax/HE. It explains the core design decisions, architecture, scheduling, trigger frame coordination, Uplink OFDMA Random Access (UORA), Multi-STA Block Ack, and the verification of these features.

## Implementation Walkthrough

Uplink MU OFDMA is coordinated by the Access Point (AP), which schedules transmissions from multiple Non-AP Stations (STAs) in frequency (RUs) and time. The key modules involved are:

1. **Trigger Frames and Coordination**:
   - The AP triggers UL MU OFDMA transmissions through the HE HCF uplink service in [HeHcfUl.cc](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfUl.cc), which determines when to send trigger frames based on BSRP (Buffer Status Report Poll) or direct scheduled triggers.
   - The Trigger frame carries Common Info such as UL Length, bandwidth,
     GI/HE-LTF settings, LDPC-extra/pre-FEC state, Spatial Reuse, and AP
     transmit power. User Info carries the STA Association ID, encoded RU
     allocation, MCS, coding, spatial-stream allocation, and target RSSI.
     Transaction correlation remains in a model-only tag rather than a MAC
     wire field.

2. **UL OFDMA Schedulers**:
   - Schedulers are implemented under `src/inet/linklayer/ieee80211/mac/scheduler/`. Schedulers decide which STAs are allocated to which RUs.
   - **`HeUlSchedulerEqualSizedRUs`**: Contiguously partitions the channel into equal-sized RUs (e.g. 26-tone or 52-tone RUs) and assigns them to active STAs.
   - **`HeUlSchedulerBacklogBased`**: Allocates RUs of varying sizes dynamically based on the queue backlog size reported by STAs during sounding or previous BSRP cycles.

3. **Uplink OFDMA Random Access (UORA)**:
   - Part of the RUs allocated in a Trigger frame can be designated for random access (using Association ID = 0).
   - STAs with packets to transmit use the HE OFDMA Backoff (OBO) counter. When the OBO reaches 0, the STA randomly selects one of the available random-access RUs to transmit its HE Trigger-Based (HE TB) PPDU.

4. **Multi-STA Block Ack**:
   - After the STAs transmit concurrently on their assigned RUs, the AP receives the HE TB PPDU container and acknowledges all successful transmissions in a single **Multi-STA Block Ack (M-BA)** frame.
   - The M-BA contains individual Block Ack records (TID, starting sequence number, and bitmap) for each STA's transmission.

## Important Design Decisions

- **Container-Based HE TB PPDU**: In the physical layer, concurrent uplink transmissions are modeled using a single HE TB container packet containing sub-transmissions on different RUs. Sibling RU transmissions are modeled as non-interfering and are received independently by the AP.
- **Context-Preserved Method Calls**: Methods on `Ieee80211Mac` and `Ieee80211TwtManager` are guarded by `Enter_Method` to ensure correct context switching in OMNeT++ when called from external HCF and scheduling submodules.
- **UORA Contention Representation**: The OBO and OCW (OFDMA Contention Window) parameters are tracked in `HeHcf` to correctly simulate standard UORA collision and retry behaviors.

## Verification

### Automated Simulations
The UL OFDMA examples are located under [examples/ieee80211ax/ul_ofdma/](../examples/ieee80211ax/ul_ofdma/).
You can run the regression scenarios (ScheduledOnly, MixedUora, EqualRus) using:

```sh
export CCACHE_DISABLE=1
source setenv -q
opp_run -u Cmdenv -l src/libINET.so -n src:examples:tutorials:showcases examples/ieee80211ax/ul_ofdma/omnetpp.ini -c MixedUora
```

### Unit Tests
The UL coordination and scheduler code are covered by focused tests under `tests/unit/`:
- `HeUlScheduler_1.test` verifies the correctness of RU layouts and UORA allocations.
- `Ieee80211HeUlControlFrames_1.test` verifies correct trigger frame generation.
