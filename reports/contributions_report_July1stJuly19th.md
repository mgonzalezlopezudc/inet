# INET Project Contributions Report (July 1st – July 19th, 2026)

This report analyzes the contributions made to the INET repository from July 1st, 2026, up to the current date (July 19th, 2026), excluding all work related to the IEEE 802.11be (EHT / Wi-Fi 7) standard. 

---

## Commit and Line Statistics

*   **Overall Project Totals (Including 802.11be / EHT Work):**
    *   Total Commits: **306**
    *   Lines of Code Added (Insertions): **120,416**
    *   Lines of Code Deleted (Deletions): **1,165**

*   **Project Totals Excluding 802.11be / EHT Work:**
    *   Total Commits: **294**
    *   Lines of Code Added (Insertions): **116,275**
    *   Lines of Code Deleted (Deletions): **822**

*(Note: The 12 excluded 802.11be commits accounted for 4,141 insertions and 343 deletions).*

---

## Chronological Analysis of Main Contributions

The contributions below are presented in chronological order of their start dates.

### Contribution 1: RFC Ingestion and Search Pipeline (July 1st, 2026)
This contribution introduced an integrated offline pipeline to track, fetch, cache, and search IETF RFC text documents inside the repository to verify standard compliance during C++ development.

*   **Key Commit:**
    *   `469a0e5ffa` (July 1, 2026): *feat: RFC ingestion pipeline*
*   **Key Files & Modules:**
    *   [inet_process_rfcs](bin/inet_process_rfcs): Command-line wrapper utility.
    *   [processor.py](python/inet/rfcs/processor.py) & [main.py](python/inet/rfcs/main.py): Ingestion logic checking byte lengths, SHA-256 hashes, ETags, fetch times, and tracking metadata errata.
*   **Importance:** Establishes a localized, reproducible way to cross-reference simulated behaviors (like TCP, UDP, or IPv6 packet structures) against normative Internet standards directly within the development shell.
*   **Implementation Effort:** **Medium**. Written in Python; utilizes structured text parsing, caching headers, and command-line execution validation.

---

### Contribution 2: Multi-Agent AI Assistance Infrastructure (July 2nd – July 16th, 2026)
This work defined structured instructions (Skills) and role-specific configurations to support automated coding agent squads inside the INET workspace.

*   **Key Commits:**
    *   `e9620a2ca1` (July 2, 2026): *various skills for OMNeT++ INET dev & debugging*
    *   `bab68879c1` (July 3, 2026): *various INET skills for AI agents*
    *   `979e7e8a43` (July 16, 2026): *Project-scoped seven-agent INET squad using the current Codex custom-agent format*
*   **Key Files & Modules:**
    *   `.agents/skills/`: Custom skill instructions (e.g. `inet-simulation-run`, `inet-lldb-debugging`, `inet-pcap-tshark-analysis`, `inet-80211-packet-debugging`) detailing debugging recipes and testing expectations.
    *   `.codex/agents/`: Configuration files mapping seven specialized agent roles (such as `inet-wifi-specialist.toml`, `inet-regression-guard.toml`, `inet-reviewer.toml`) to bound context lengths and delegation bounds.
*   **Importance:** Speeds up and modularizes automated debugging and testing, preventing recursive logic fan-outs and ensuring code modifications pass deterministic regression suites before submission.
*   **Implementation Effort:** **Medium**. Consisted of mapping skills to file layouts, checking TOML syntax boundaries, and writing detailed Markdown instructions outlining standard OMNeT++ tooling recipes.

---

### Contribution 3: IEEE 802.11ax (TGax) Core MAC Enhancements, Bug Fixes, and Showcases (July 5th – July 19th, 2026)
This is a collection of updates resolving critical logic bugs, scheduling errors, and startup conflicts in the 802.11ax High-Efficiency (HE) MAC layer (`HeHcf`), along with structured showcases.

#### Subcontribution 3.1: 802.11ax Wireless Showcases & Campaign Scripts (July 5th – July 18th, 2026)
*   **Commits:** `9becdde419` (July 5), `67e3c35658` (July 17), `f7b5fa0614` (July 17)
*   **Key Files:** `showcases/wireless/` documentation pages (such as `heofdma`, `bsscoloring`, `twt`, and `hefeatures`) and `examples/ieee80211ax/analysis/analyze_pcap_types.py`.
*   **Description:** Implemented documentation showcases explaining Wi-Fi 6 tradeoffs (downlink/uplink OFDMA, scheduling, UORA, BSS coloring, TWT, and capability puncturing) and built Python script campaigns to auto-update walkthrough markdown tables and charts.
*   **Importance:** Improves onboarding and developer usability by demonstrating complex standard parameters in simple, visual simulation profiles.

#### Subcontribution 3.2: Multi-TID Block Ack Integration (July 12th, 2026)
*   **Commit:** `605a99fc78` (July 12)
*   **Key Files:** [SingleProtectionMechanism.cc](src/inet/linklayer/ieee80211/mac/protectionmechanism/SingleProtectionMechanism.cc) & [OriginatorBlockAckAgreementHandler.cc](src/inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreementHandler.cc)
*   **Description:** Resolved an issue where separate Block Ack records were generated per request instead of packing multiple Traffic Identifiers (TIDs) into a single physical Multi-TID Block Ack frame. Restructured example configurations to trigger downlink Multi-TID A-MPDUs and calculate Multi-TID BAR response duration (18B fixed + 12B per TID).
*   **Importance:** Reduces control frame overhead in networks carrying multiple concurrent traffic categories.

#### Subcontribution 3.3: Scheduler OMI Filtering & Dynamic Fragmentation Policies (July 12th, 2026)
*   **Commit:** `605a99fc78` (July 12)
*   **Key Files:** [HeUlSchedulerBacklogBased.cc](src/inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBacklogBased.cc) & [HeDynamicFragmentationPolicy.cc](src/inet/linklayer/ieee80211/mac/fragmentation/HeDynamicFragmentationPolicy.cc)
*   **Description:** 
    1.  Patched HCF schedulers to check the Operating Mode Indication (OMI) `ulMuDisable` flag and exclude requesting stations from uplink schedules.
    2.  Fixed a bug where `HeDynamicFragmentationPolicy` attempted to read receiver capabilities from a MAC header that had not been prepended yet. Added a fallback to retrieve capabilities from the `MacAddressReq` tag.
*   **Importance:** Enforces dynamic station capabilities and restores automated MAC fragmentation controls.

#### Subcontribution 3.4: Startup Host Warmup Starvation Solution (July 14th, 2026)
*   **Commit:** `e8644a9931` (July 14)
*   **Key Files:** [dl_ofdma/omnetpp.ini](examples/ieee80211ax/dl_ofdma/omnetpp.ini) & [ul_ofdma/omnetpp.ini](examples/ieee80211ax/ul_ofdma/omnetpp.ini)
*   **Description:** Implemented a low-rate, single-packet warmup phase in 802.11ax examples. This allows client hosts to complete initial Block Ack agreements (ADDBA handshakes) before high-rate saturated data traffic begins.
*   **Importance:** Prevents startup traffic queues from blocking agreement negotiations, which previously starved client hosts.

#### Subcontribution 3.5: Downlink MU-OFDMA Scheduling SU Fallback Fix (July 18th, 2026)
*   **Commit:** `c074920392` (July 18)
*   **Key Files:** [HeHcfDl.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfDl.cc) & [IAckHandler.h](src/inet/linklayer/ieee80211/mac/contract/IAckHandler.h)
*   **Description:** Fixed a critical bug where `hasEligibleExistingFrame` treated newly staged (but not yet transmitted) frames in the `inProgress` queue as legacy recovery frames that had to be completed first. This locked the AP into legacy Single-User (SU) fallback mode. Implemented `isRetransmission()` inside originator Ack handlers to explicitly check for frames in an active failed state needing recovery.
*   **Importance:** Restored multi-user scheduling in downlink OFDMA. Without this fix, the AP frequently dropped back to sequential SU transmissions.

#### Subcontribution 3.6: TWT and A-MPDU Retry Bookkeeping (July 18th, 2026)
*   **Commit:** `03ea65d6e3` (July 18)
*   **Key Files:** [Hcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc) & [QosAckHandler.cc](src/inet/linklayer/ieee80211/mac/originator/QosAckHandler.cc)
*   **Description:** Corrected a bug where `QosAckHandler` correctly classified MPDUs as retransmissions after missing/partial Block Acks, but `Hcf` failed to copy that state to the MPDU header Retry bit when rebuilding A-MPDUs.
*   **Importance:** Fixes incorrect header bookkeeping inside captured frames, resolving PCAP packet validation mismatches.

#### Subcontribution 3.7: HE ER-SU Repetition Gain modeling (July 19th, 2026)
*   **Commit:** `27f7017825` (July 19)
*   **Key Files:** [Ieee80211ErrorModelBase.cc](src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211ErrorModelBase.cc)
*   **Description:** Configured `computePacketErrorRate()` to detect Extended Range Single User (`HE_PREAMBLE_ER_SU`) headers and apply a ~3 dB SNR multiplier before header success checks, modeling the standard-specified repeated HE-SIG-A field.
*   **Importance:** Models the robustness gains of ER-SU modes at cell boundaries (e.g. 340 meters) where standard HE-SU fails.

#### Subcontribution 3.8: BSS-Coloring Spatial Reuse Tuning (July 19th, 2026)
*   **Commit:** `563218a67e` (July 19)
*   **Key Files:** [bss_coloring/omnetpp.ini](examples/ieee80211ax/bss_coloring/omnetpp.ini)
*   **Description:** Randomized offered load using `uniform(1ms, 1.4ms)` to avoid synchronized traffic artifacts and structured OBSS/PD thresholds to span standard bounds (conservative `-82 dBm`, tuned `-79 dBm`, aggressive `-62 dBm`).
*   **Importance:** Demonstrated a clear +29% goodput increase and a 35x concurrent airtime increase in the BSS Coloring spatial reuse setup.

*   **Overall Contribution Importance:** Corrects fundamental scheduling, framing, and negotiation bugs in the 802.11ax MAC stack, making simulations highly stable, accurate, and compliant.
*   **Overall Contribution Implementation Effort:** **Medium to High**. Required tracing state transitions, resolving complex startup locks, and coordinating multiple parameter sweeps to verify throughput improvements.

---

### Contribution 4: TGax (802.11ax/Wi-Fi 6) Channel Models & RBIR Error Model (July 16th – July 17th, 2026)
This work introduced physical layer propagation, multi-antenna matrix processing (MIMO), and a subcarrier-level error model for 802.11ax.

*   **Key Commits:**
    *   `62b1cfd6f9` (July 16, 2026): *static TGax SISO*
    *   `b52cd51ce6` (July 16, 2026): *Implemented and verified the next TGax milestone* (Doppler)
    *   `f4de2ed2da` (July 16, 2026): *next MIMO TGax models implemented*
    *   `5f71fce906` (July 16, 2026): *next TGax MIMO channel model implemented* (NLOS Kronecker NLOS)
    *   `ee4703b640` (July 16, 2026): *next TGax channel model implementation* (MRC/SIMO)
    *   `549ce39349` (July 17, 2026): *TGax channel models completed*
*   **Key Files & Modules:**
    *   [TgaxChannelModel](src/inet/physicallayer/wireless/ieee80211/channelmodel/TgaxChannelModel.cc) & [TgaxChannelProfile](src/inet/physicallayer/wireless/ieee80211/channelmodel/TgaxChannelProfile.cc): Indoor profiles A–F with ambient Doppler bell spectrum and cached link state snapshots.
    *   [TgaxIndoorPathLoss](src/inet/physicallayer/wireless/ieee80211/channelmodel/TgaxIndoorPathLoss.cc) & [TgaxUmiPathLoss](src/inet/physicallayer/wireless/ieee80211/channelmodel/TgaxUmiPathLoss.cc): Indoor TGax and outdoor ITU-R UMi path loss models.
    *   [TgaxSisoChannel](src/inet/physicallayer/wireless/ieee80211/channelmodel/TgaxSisoChannel.cc) & [TgaxMimoChannel](src/inet/physicallayer/wireless/ieee80211/channelmodel/TgaxMimoChannel.cc): NLOS Kronecker generator normalization and ordinary-transpose reciprocity.
    *   [Ieee80211RbirErrorModel](src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211RbirErrorModel.cc): Per-tone Received Bit Information Rate (RBIR) packet-level physical layer error evaluation.
    *   [ChannelMatrixCombiner](src/inet/physicallayer/wireless/common/signal/ChannelMatrixCombiner.cc): Selected-transmit-antenna SIMO MRC (Maximal Ratio Combining) with explicit precoding.
*   **Importance:** Moves the wireless simulation away from flat-fading approximations. It enables physical-layer modeling of multi-antenna arrays (MIMO) and subcarrier-level SNIR/RBIR calculations, matching actual physical observations under selective frequency fading.
*   **Implementation Effort:** **Very High**. Required building complex C++ matrix transformations, Laplacian PAS integrations, Kronecker NLOS channel matrix configurations, and detailed integration/unit tests validating matrix math and reciprocity constraints.

---

### Contribution 5: IEEE 802.11 PCAP Radiotap Header Serialization and Extraction (July 17th – July 18th, 2026)
This contribution introduced standardized Radiotap encapsulation into the packet capture framework, allowing Wi-Fi packet metadata to be verified through external packet analyzers.

*   **Key Commits:**
    *   `95ada434f5` (July 17, 2026): *Radiotap encapsulation for IEEE 802.11 PCAP recording*
    *   `1f1f463d0c` (July 18, 2026): *Legacy Rate, HT MCS, A-MPDU, VHT, and 0-length PSDU in PcapRecorder*
    *   `352a342bf6` (July 18, 2026): *Radiotap HE and HE-MU Field Serialization*
    *   `3361a7c18e` (July 18, 2026): *Radiotap Header Fields Integration*
*   **Key Files & Modules:**
    *   [PcapRecorder](src/inet/common/packet/recorder/PcapRecorder.cc): Decodes frames and appends little-endian Radiotap headers with flags, MCS/NSS, bandwidth, coding, spatial streams, guard intervals, and 0-length PSDU.
    *   `examples/ieee80211ax/analysis/analyze_pcap_types.py`: Expands the `tshark` query pipeline to retrieve and decode these fields, calculate precise airtime, and dump statistics tables in the walkthroughs.
*   **Importance:** Provides industry-standard logging capabilities. Developers can inspect PCAP files using common tools with detailed physical parameters, enhancing validation of packet transmissions, aggregations, and frame durations.
*   **Implementation Effort:** **High**. Requires precise byte-alignment handling (e.g. 2-byte and 4-byte boundaries for optional flags) and serializing variable fields in strictly increasing order of their present bit indices. It also required rewriting offline Python parsing, graphing, and validation pipelines.
