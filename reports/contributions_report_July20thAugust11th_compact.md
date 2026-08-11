# INET Project Contributions Report

## Scope and Statistics

This report summarizes the **IEEE 802.11 contributions** from July 20th through August 11th, 2026, using the repository Git history and Codex session history. 

*   **Scoped project totals:** **194 commits**, **1,960,302 insertions**, and **1,433,272 deletions**.
*   **`src/` totals, excluding tests:** **83 source-changing commits**, **30,936 insertions**, and **8,199 deletions**.

The project-wide totals include regenerated manifests, packet metrics, evidence ledgers, plots, and rewritten walkthroughs. The `src/` totals provide a closer measure of **hand-written implementation changes**; test files are not included in that subtotal.

## Main Contributions

### 1. HE Baseline, Dense-IoT Validation, and Architecture — July 20th

The period began with a **validated HE baseline**, a completed **64-station dense-IoT scenario**, an explicit **802.11ax fidelity profile**, and stronger project-specific architecture and agent-routing rules. This established reproducible reference behavior for subsequent PHY and MAC changes.

An **802.11ax fidelity profile is a documented modeling contract**: it states which HE capabilities, bands, channel widths, RU layouts, signaling fields, timing rules, and deterministic policies the model claims to represent; which behaviors are supported, model-only, or incomplete; and which unit tests, simulations, and standard-derived vectors validate those claims. It therefore defines a **precise, reproducible scope of conformance** rather than implying that every part of the standard is implemented.

**Key commits**

*   Established the validated HE baseline.
*   Defined the **802.11ax fidelity profile and its validation scope**.
*   Implemented and verified the 64-station scenario.
*   Published the dense-IoT walkthrough.
*   Added architectural requirements and project-specific agent routing.

### 2. Canonical HE PHY Signaling — July 20th–24th

HE signaling was migrated to **explicit, canonical representations** aligned with TXVECTOR, RXVECTOR, PHY headers, PPDU layout, and logical HE-SIG fields. HE SU transmissions received a **native PHY identity**, exact MU/TB signaling was serialized, and the associated codecs, calculators, serializers, and unit tests were integrated.

**Key commits**

*   Gave HE SU transmissions a native PHY identity.
*   Added logical HE-SIG codecs and validation.
*   Split HE PHY header formats.
*   Added the canonical HE TXVECTOR foundation.
*   Added the canonical HE RXVECTOR and PPDU layout.
*   Integrated canonical HE PHY vectors.
*   Serialized exact HE MU and HE-TB signaling.
*   Calculated exact HE data fields.
*   Completed the canonical HE signaling migration.

This unified **PHY calculation, MAC scheduling, radio transmission, Radiotap export, and PCAP analysis** around the same HE data model.

### 3. Transactional HE MU Scheduling and State Ownership — July 22nd–24th

Downlink MU scheduling and triggered uplink scheduling were redesigned around **explicit plans, ownership, and commit points**. Temporal and directional rate state, transaction ownership, in-progress frame handling, and MU invariants were strengthened to distinguish staged frames, failed exchanges, retries, and completed transmissions.

**Key commits**

*   Made downlink MU scheduling transactional.
*   Made triggered uplink scheduling transactional.
*   Made HE rate state temporal and directional.
*   Hardened HE transaction ownership.
*   Proved HE MU transaction invariants.

The transactional design provides several concrete benefits. The scheduler first constructs a **complete MU transmission plan**, retains explicit ownership of the frames and resources in that plan, and commits the resulting state only after the exchange outcome is known. This prevents a **newly staged frame from being mistaken for a retransmission**, prevents one user’s failure from corrupting another user’s outcome, and keeps retry, acknowledgment, and queue state consistent. It also makes recovery paths and invariants **deterministic and testable**, so PCAP evidence can be traced back to one specific scheduling decision instead of to a mixture of stale and current state.

### 4. Shared Walkthrough and Evidence Pipeline — July 25th–30th

The analysis workflow was reorganized into a **repeatable evidence pipeline** for configurations, scalar/vector results, PCAP metrics, plots, frame-exchange timelines, and validation status. The work completed the IEEE 802.11 walkthrough set and introduced shared scenario-oriented analysis and publication tooling.

**Key commits**

*   Introduced the walkthrough-writer skill, contract, template, references, and validator.
*   Completed the IEEE 802.11 walkthrough collection.
*   Implemented the plot and table generation workflow.
*   Added the scenario-oriented Wi-Fi analysis interface.
*   Simplified the analysis scripts and centralized figure handling.
*   Unified example directories and scalar/vector mappings.
*   Established the canonical analysis layout.
*   Restructured the 802.11ax examples, manifests, tests, and documentation.

### 5. HE OFDMA Reliability and Block Ack Corrections — July 28th–31st

Analysis and PCAP evidence exposed accounting and recovery defects in HE OFDMA. Scheduler byte planning, retry counters, QoS Null behavior, telemetry, failed-BAR recovery, compressed BAR handling, BSR backlog reporting, UORA state, and UL-SU Multi-TID Block Ack behavior were corrected.

**Key commits**

*   Corrected retry-counter accounting for triggered UL failures and ordinary QoS Block Ack failures.
*   Corrected `plannedBytes` accounting and shared A-MPDU overhead calculations.
*   Corrected QoS Null MU-EDCA behavior.
*   Added selective recording and targeted UL OFDMA instrumentation.
*   Added graceful handling for missing telemetry.
*   Fixed failed-BAR recovery accounting.
*   Added the missing HE-TB compressed-BAR recovery path.
*   Included in-progress and retrying frames in STA BSR reports.
*   Fixed UORA OCW/OBO state handling.
*   Corrected the UL-SU Multi-TID Block Ack receive path and regenerated its evidence.

The result is closer agreement between **queued bytes, scheduled bytes, transmitted PSDUs, acknowledgments, retries, and published evidence**.

### 6. MU-MIMO, SU-MIMO, and BAR Scenarios — July 27th–August 3rd

The work expanded spatial multiplexing into **validated downlink and uplink scenarios**. It covered DL MU-MIMO, UL MU-MIMO, OFDMA with per-user SU-MIMO, MU-BAR capability handling, and sequential-versus-triggered BAR comparisons across offered loads.

**Key commits**

*   Created and validated the DL MU-MIMO configurations and walkthrough.
*   Added OFDMA plus per-user SU-MIMO support.
*   Implemented UL OFDMA SU-MIMO support.
*   Reworked the UL MU-MIMO example around matched geometry, offered loads, and CSI leakage levels.
*   Corrected HE-TB spatial configuration derivation.
*   Completed the MU-BAR capability path.
*   Added the sequential-versus-triggered BAR example and campaign.
*   Corrected DL MU-MIMO configurations, SNIR calculation, analysis, and regression coverage.

### 7. HT/VHT and Cross-Generation Feature Completion — August 3rd–9th

The scope broadened to **HT and VHT behavior, examples, and analysis**. The work covered channel widths, CCA, PPDU duration, aggregation, implicit Block Ack, Greenfield preambles, primary/secondary channels, MCS and guard intervals, sounding, MRQ/MFB, and VHT DL MU-MIMO.

**Key commits**

*   Added the pre-HT/HT/VHT feature suite and unified topology.
*   Implemented HT40− and HT40+ channel behavior.
*   Implemented HT CCA classification and sensitivity regression coverage.
*   Implemented HT PPDU-duration trimming.
*   Fixed HT implicit Block Ack state and failure handling.
*   Added pre-ADDBA frame buffering.
*   Implemented the Greenfield preamble.
*   Implemented primary/secondary channel handling for wide channels.
*   Added MCS coverage across supported PHY families.
*   Completed guard-interval modeling and corrected fallback selection.
*   Implemented and verified HT MRQ/MFB and sounding.
*   Implemented and repaired standard-aligned VHT DL MU-MIMO behavior.

### 8. Acknowledgment, PCAP, and Observability Improvements — August 7th–9th

Packet recording and analysis were consolidated across profiles. The changes added **implicit Block Ack coverage**, improved PCAP performance and capture scoping, decoded Management Action details, decomposed MU captures, and added queue-evolution plots.

**Key commits**

*   Added single-MPDU A-MPDU implicit Block Ack support.
*   Extended implicit Block Ack support across the supported profiles.
*   Corrected the default acknowledgment-support settings.
*   Added PCAP scope-recording options.
*   Implemented VHT MU PCAP export.
*   Improved PCAP recording performance and capture-adapter isolation.
*   Added detailed Management Action category and code decoding.
*   Added queue-evolution plots to the example analysis outputs.

### 9. Agent Routing and Workflow Reliability — July 20th–August 8th

The project-support layer gained **architectural requirements, model routing, orchestration unification, progressive-disclosure result-analysis guidance, path-alignment rules, and tool-call reliability improvements**. These changes made standards analysis, implementation, simulation, regression, evidence analysis, and review more repeatable.

**Key commits**

*   Added the architectural-requirements skill.
*   Updated the Codex and Kimi agent squads to apply the architectural requirements.
*   Added project-specific model routing and orchestration support.
*   Synchronized the orchestration systems.
*   Unified the orchestration skills.
*   Restructured result plotting for progressive disclosure.
*   Added path-alignment and tool-call reliability rules.

## Overall Assessment

The period produced a coherent progression from **baseline validation** to **canonical PHY representation**, **transactional MAC behavior**, broad regression scenarios, evidence-driven walkthroughs, and reusable engineering workflows. The most important result is the **closed validation loop**:

```text
standard or observed discrepancy
        ↓
source-level design and implementation
        ↓
focused simulation or regression
        ↓
PCAP and scalar/vector evidence
        ↓
reusable walkthrough and workflow guidance
```

Together, these contributions improve **protocol fidelity, cross-generation coverage, reproducibility, and maintainability** for future IEEE 802.11 development.

