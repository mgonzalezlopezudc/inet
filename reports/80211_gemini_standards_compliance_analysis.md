# IEEE 802.11 Standards Compliance Analysis in INET

This document provides a detailed analysis of the current IEEE 802.11 implementation in the INET Framework. It evaluates the level of compliance with the standards (especially IEEE 802.11ax/HE), identifies missing features, flags poor design assumptions, reviews the testing/simulation infrastructure, and presents a SWOT analysis with proposed next steps.

---

## 1. Executive Summary

INET provides a robust, heavily modularized implementation of the IEEE 802.11 MAC and physical layers, ranging from legacy 802.11a/b/g to 802.11ax (High Efficiency / Wi-Fi 6). 
The physical layer calculations for HE are highly standard-compliant, implementing precise formulas for LDPC codeword sizing, padding, and trigger-based timings. However, many advanced MAC features (like PCF, HCCA, and Multi-TID Block Ack) are left as unimplemented skeletons, and multi-user physical mechanisms (like MU-MIMO and OBSS Spatial Reuse) rely on simplified, cosmetic, or lookup-table abstractions rather than actual physical-layer modeling.

---

## 2. Standards Compliance Evaluation

### Physical Layer (PHY) Models & Modes
*   **Compliant Features:**
    *   **Operating Modes:** Supports a wide range of standards: legacy `a`, `b`, `g(erp/mixed)`, `p`, `ac` (VHT), and `ax` (HE).
    *   **Timing Parameters:** Slot times, SIFS, DIFS, and preamble durations are accurately calculated.
    *   **802.11ax Timing Calculations:** The `Ieee80211HePhyCalculator.h` implements highly detailed, standard-compliant formulas:
        *   LDPC codeword sizing (648, 1296, or 1944 bits) with shortening and repetition bits.
        *   Tail/service bit placement and number of encoders.
        *   Pre-FEC/Post-FEC padding calculations.
        *   Guard Interval (0.8, 1.6, and 3.2 µs) and HE-LTF symbol duration scaling.
        *   Common and per-user HE-SIG-A/HE-SIG-B symbol count calculations.
*   **Omissions / Non-Compliant Areas:**
    *   **Wi-Fi 7 (802.11be / EHT) and Wi-Fi 6E (6 GHz band):** No support for EHT physical modes or 320 MHz channel widths. 6 GHz is defined at the MIB level but lacks fully integrated physical characteristics.

### MAC Sublayer & Channel Access
*   **Compliant Features:**
    *   **DCF & HCF (EDCA):** Fully supports contention-based QoS prioritization.
    *   **Action Frame Sequences:** Models ADDBA (Add Block Ack Request/Response) and DELBA (Delete Block Ack) exchanges.
    *   **HE Multi-User (MU) Coordination:**
        *   Trigger frames (Basic and BSRP) are supported.
        *   Downlink and Uplink OFDMA (Orthogonal Frequency Division Multiple Access) with puncture-aware Resource Unit (RU) layouts.
        *   Uplink OFDMA Random Access (UORA) with OFDMA Contention Window (OCW) updates on collision/success.
*   **Omissions / Non-Compliant Areas:**
    *   **HCCA & PCF:** Both are unimplemented skeletons in `coordinationfunction/` (declared in NED, but C++ classes are empty).
    *   **Multi-TID Block Ack:** The classes `Ieee80211MultiTidBlockAckReq` and `Ieee80211MultiTidBlockAck` are defined in `.msg`, but their handlers and serializers are commented out as `TODO unimplemented`.
    *   **Power Management:** Listen interval, sleep/wake transitions, and legacy power-save poll (PS-Poll) details are omitted or simplified.

---

## 3. Poor Assumptions & Simplifications

1.  **Simplified MU-MIMO Modeling (CSI Leakage):**
    *   *Assumption:* MU-MIMO transmission operates via a simple lookup table (`HeMuMimoCsiManager.h`). During a sounding exchange, stations record a static or configured "leakage" value (e.g., `0.1` or overridden per pair of AIDs) representing co-scheduled stream interference.
    *   *Impact:* There is no physical-layer beamforming matrix calculation, precoder determination, or multipath fading. SNR for MU-MIMO users is computed analytically using a simple formula (`userSnir = (snr * signalShare) / (1.0 + snr * interferenceShare)`), which limits the realism of beamforming performance in complex multipath environments.
2.  **Cosmetic BSS Color / Spatial Reuse:**
    *   *Assumption:* `bssColor` is negotiated during association and serialized into HE-SIG-A headers. However, the physical layer receiver (`Ieee80211Receiver.cc`) and analog models ignore BSS color when evaluating Clear Channel Assessment (CCA) or frame reception.
    *   *Impact:* Dynamic CCA threshold tuning (OBSS PD threshold adjustment) based on BSS color (Spatial Reuse) is entirely missing. Frames from overlapping BSSs are treated as standard co-channel interference.
3.  **Bcc-only / LDPC PER Gain Approximation:**
    *   *Assumption:* INET does not implement bit-level LDPC coding/decoding. Instead, `Ieee80211YansErrorModel.cc` models LDPC performance by adding a hardcoded `1.5 dB` boost to the received SNR, while still using the standard analytical BPSK/QAM error curves.
4.  **AID/STA ID Collision Handling:**
    *   *Assumption:* In HE MU downlink planning, the AP computes fallback 11-bit STA IDs from MAC addresses. If two scheduled stations collide on the same ID, the scheduling transaction is aborted completely.
    *   *Impact:* A real AP negotiates unique Association IDs (AIDs) to prevent collisions. Aborting the schedule limits multi-user efficiency if AIDs collide.

---

## 4. Testing & Examples Quality Analysis

### Unit Tests (`tests/unit`)
*   **Strengths:**
    *   **Surgical and Targeted:** The unit tests (e.g., [Ieee80211HeMuAddbaValidation_1.test](file:///home/user/omnetpp_ws/inet/tests/unit/Ieee80211HeMuAddbaValidation_1.test), [Ieee80211HeMuRx_1.test](file:///home/user/omnetpp_ws/inet/tests/unit/Ieee80211HeMuRx_1.test), [Ieee80211HeMuBlockAckGating_1.test](file:///home/user/omnetpp_ws/inet/tests/unit/Ieee80211HeMuBlockAckGating_1.test)) are highly detailed. They construct specific packet streams using mock coordination functions and check precise states (such as Block Ack bitmap entries and recipient gating).
    *   **Timing Coverage:** Verifies mathematical models for OFDMA RUs and packet extensions.
*   **Weaknesses:**
    *   **Test Runner Bug:** The Python-based test runner script (`bin/inet_run_unit_tests` invoking `inet/simulation/project.py`) throws a blocking `IsADirectoryError [Errno 21]` if a `.omnetpp` directory exists in the workspace root. This is because the script checks for the existence of `.omnetpp` (`os.path.exists`) but assumes it is a project definition JSON file rather than a directory.

### Example Simulations (`examples/ieee80211`)
*   **Strengths:**
    *   **HE Features Walkthrough:** The walkthrough in `examples/ieee80211/he_features/walkthrough.md` is exceptionally clear and provides step-by-step guidance on how to observe LDPC, packet extensions, capability negotiation, and preamble puncturing.
*   **Weaknesses:**
    *   **No Fingerprint/Regression Coverage:** The advanced HE features (`he_features`), downlink OFDMA (`dl_ofdma`), and uplink OFDMA (`ul_ofdma`) configurations **are not registered** in `tests/fingerprint/examples.csv` or `showcases.csv`. This means they are not regression-tested. If a change breaks the scheduler or trigger logic, it will not be caught by standard fingerprint verification tests.
    *   **Small Scale:** The examples only feature small, stationary topologies (1 AP, 3-4 STAs). They do not evaluate dense network settings or mobility.

---

## 5. SWOT/SWAT Analysis: 802.11 Standards Compliance Focus

This section evaluates the current INET 802.11 implementation under the SWOT/SWAT framework, specifically focusing on its alignment with standard specifications (IEEE 802.11 legacy, HT, VHT, and HE).

### Strengths (Standard Compliance)
*   **Highly Accurate HE PHY Timing Calculations:** The implementation in `Ieee80211HePhyCalculator.h` is extremely compliant with the IEEE 802.11ax specification. It precisely models the physical layer framing:
    *   **LDPC Codeword Allocation:** Implements standard rules for splitting payloads across 648, 1296, or 1944-bit codewords, calculating exact shortening and repetition bits, and managing multiple encoders.
    *   **Pre-FEC and Post-FEC Padding:** Adheres to the standard formulas for calculating padding factors and symbol capacities.
    *   **Signaling and Preamble Durations:** Correctly determines guard interval scaling, HE-LTF symbol duration multipliers, and HE-SIG-A/HE-SIG-B content channel lengths.
*   **Compliant Uplink OFDRA Random Access (UORA):** Models UORA contention backoffs as specified in 802.11ax. The OFDMA Contention Window (OCW) dynamically scales (initialized to `ocwMin`, doubled on collision, and reset to `ocwMin` on success) and the random RU selection follows the trigger frame constraints.
*   **Robust Preamble Puncturing Validation:** Enforces standard rules for 80/160 MHz channel puncturing (ensures the primary subchannel is active, that the mask does not disable all subchannels) and generates puncture-aware RU layouts where no RU overlaps punctured frequencies.
*   **Modular Coordination Frame Sequences:** The HCF and DCF frame sequence models (like `HeDlMuTxOpFs`, `HeSoundingFs`) strictly coordinate frame exchanges (RTS/CTS, trigger frames, MU block acks, sounding) matching standard sequence timelines.
*   **Fine-grained EDCA Compliance:** Implements separate backoff engines and QoS queues for AC_VO, AC_VI, AC_BE, and AC_BK, ensuring correct traffic prioritization.

### Weaknesses (Standard Compliance)
*   **Unimplemented Coordination Functions:** Standard Point Coordination Function (PCF) and HCF Controlled Channel Access (HCCA) are not implemented, leaving only empty skeleton classes in `coordinationfunction/Pcf.cc` and `Mcf.cc`.
*   **Cosmetic OBSS Spatial Reuse (BSS Color):** Although BSS Color is successfully negotiated during association and serialized in the PHY headers, the reception logic in `Ieee80211Receiver.cc` ignores it. The dynamic CCA threshold tuning (OBSS PD threshold adjustment) required by the 802.11ax standard to allow concurrent transmissions is entirely missing.
*   **No Multi-TID Block Ack Support:** Multi-TID Action Frames and agreements (which standardize aggregation across multiple traffic categories) are declared in `Ieee80211Frame.msg` but not implemented in C++ serializers or frame sequencing handlers.
*   **Lookup-Table MU-MIMO Modeling:** Downlink MU-MIMO relies on a simplified analytical "csi leakage" matrix lookup (`HeMuMimoCsiManager.h`) rather than simulating actual spatial channel correlation, beamforming weights, or multipath fading.
*   **Hardcoded LDPC Error Performance:** The LDPC packet error rate (PER) gain is represented by a static `1.5 dB` boost in `Ieee80211YansErrorModel.cc` rather than bit-level LDPC coding simulations.

### Opportunities (Standard Compliance)
*   **Spatial Reuse Implementation:** Enhance `Ieee80211Receiver` to dynamically adjust Clear Channel Assessment (CCA) thresholds based on the negotiated BSS Color, bringing the model into full compliance with 802.11ax Spatial Reuse.
*   **Multi-Link Operation (MLO) for Wi-Fi 7:** Leverage INET's modular MAC architecture to implement standard MLO (defined in IEEE 802.11be), allowing hosts to bind and coordinate multiple physical radios (2.4, 5, and 6 GHz).
*   **Multi-TID Aggregation:** Implement C++ support for the Multi-TID Block Ack variants to allow simultaneous aggregation of mixed-priority QoS queues under a single session.

### Threats (Standard Compliance)
*   **Inaccurate Network Performance Predictions:** Because spatial reuse is cosmetic and MU-MIMO is modeled analytically via static leakages, simulations in dense deployment scenarios (like university campuses or enterprise OBSS networks) can produce highly inaccurate throughput and delay results, diverging significantly from real-world IEEE 802.11ax compliant hardware.
*   **Regression of Compliant Mechanics:** Advanced HE features (like UORA, puncturing, and trigger sequencing) are excluded from the fingerprint regression suite. There is a strong risk that future framework updates will cause silent breakages or timing drift, compromising standards compliance without triggering verification failures.


---

## 6. Proposed Next Steps

1.  **Fix Python Test Runner:**
    Modify [project.py](file:///home/user/omnetpp_ws/inet/python/inet/simulation/project.py#L540-L545) to ensure it only reads `.omnetpp` if it is a regular file:
    ```python
    project_file_name = os.path.join(path, ".omnetpp")
    if os.path.isfile(project_file_name):  # Change from os.path.exists to os.path.isfile
        with open(project_file_name) as project_file:
            ...
    ```
2.  **Add HE Examples to Fingerprint Tests:**
    Register the configurations in `examples/ieee80211/he_features/omnetpp.ini`, `dl_ofdma/omnetpp.ini`, and `ul_ofdma/omnetpp.ini` inside [examples.csv](file:///home/user/omnetpp_ws/inet/tests/fingerprint/examples.csv) to ensure they are regression-tested automatically.
3.  **Implement Spatial Reuse Logic (BSS Color CCA Tuning):**
    Update `Ieee80211Receiver::computeIsReceptionAttempted` and `Ieee80211Receiver::computeReceptionResult` to adjust the energy detection / sensitivity threshold dynamically when a frame carries an OBSS (differing) BSS color.
4.  **Implement Multi-TID Block Ack:**
    Flesh out the serialization and coordination handling for multi-TID Block Acks to fully support QoS aggregation across multiple traffic categories.
5.  **Refine MU-MIMO Modeling:**
    Integrate a physical spatial channel model that calculates path losses based on antenna arrays and beamforming precoding coefficients.
