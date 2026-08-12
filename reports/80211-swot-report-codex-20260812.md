# Static review of INET’s IEEE 802.11 implementation

## Overall assessment

INET’s 802.11 implementation is technically ambitious and generally well structured at the module level. Its strongest areas are configurability, typed protocol models, PHY validation, observability, and test coverage.

The main weakness is unevenness: newer HE/EHT code follows clearer contracts, while older MAC and management code concentrates too many responsibilities in large classes and still contains known semantic gaps.

- **Code quality:** good overall, but inconsistent between newer and legacy areas.
- **Readability:** good in focused components; difficult in the central HCF and HE coordination code.
- **Efficiency:** no serious problem can be established statically. Several repeated scans, packet copies, allocations, and sorts deserve profiling at large scale.
- **Maintainability:** acceptable today, but the central coordinators will become increasingly difficult to extend.

This assessment used INET’s architectural rules, especially responsibility boundaries, single ownership of protocol state, explicit exchange state, centralized PHY calculations, and replaceable amendment-specific behavior.

## SWOT

### Strengths

- **Strong modular composition.** MAC roles such as coordination functions, transmission, reception, policies, and rate control are exposed through replaceable NED components. This makes experiments and alternative implementations easier to configure. See [Ieee80211Mac.ned](src/inet/linklayer/ieee80211/mac/Ieee80211Mac.ned:116) and [Hcf.ned](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.ned:98).

- **Clear packet and PHY representations.** Typed headers, tags, transmission vectors, and PHY layouts make invalid combinations easier to detect than loosely structured metadata would.

- **Good fail-fast validation.** The HE PHY calculator checks bandwidth, guard interval, RU layout, stream count, coding, and other inputs at a central boundary. Invalid input returns a specific error rather than being silently corrected. See [Ieee80211HePhyCalculator.cc](src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.cc:534).

- **Improving state ownership.** `HcfExchangeCoordinator` explicitly tracks preparation, transmission, response waiting, recovery, completion, and timers. Its transition checks make illegal exchange states visible. See [HcfExchangeCoordinator.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeCoordinator.cc:23).

- **Good standards traceability in newer code.** Many important decisions cite specific IEEE 802.11-2024 clauses next to the implementation.

- **Extensive tests.** The repository contains broad unit and module coverage for HE/EHT, Block Ack, serialization, scheduling, association, rate selection, PHY calculations, and legacy retransmission. This is a significant asset even though the tests were not executed for this review.

- **Useful observability.** The implementation exposes signals and statistics for frame sequences, collisions, aggregation, Block Ack agreements, MU allocation, spatial reuse, and selected rates.

### Weaknesses

- **The main HCF class is too large.** [Hcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc:197) is approximately 2,300 lines and handles channel access, ACK and Block Ack, aggregation, retries, protection, HT sounding, management delivery, mode selection, timers, and frame construction. Its `transmitFrame()` path alone performs several distinct protocol responsibilities ([Hcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc:1945)).

- **HE coordination remains highly complex.** Although its implementation is divided across files, [HeHcf.h](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h:147) exposes a large state surface covering scheduling, queue banks, UL transactions, sounding, association retirement, operating modes, timers, and recovery.

- **Some feature gating is brittle.** HE operation is selected by comparing the mode-set name with the literal string `"ax"` ([HeHcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc:625)). An explicit capability query would be safer for aliases, future profiles, and EHT compatibility.

- **Known management-state gaps affect correctness.** Authentication and association state may be committed before the response is acknowledged; comments already identify this problem. Lost response frames could therefore leave modeled state ahead of the actual exchange. See [Ieee80211MgmtAp.cc](src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtAp.cc:365) and [Ieee80211MgmtAp.cc](src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtAp.cc:440).

- **Address interpretation is incomplete.** `isSentByUs()` uses Address 3 even though its meaning depends on the To DS/From DS combination, which the code itself acknowledges ([Hcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc:2236)). This is risky for multicast, distribution-system, and aggregate scenarios.

- **Manual ownership increases review difficulty.** HCF constructs multiple helper objects with `new` and deletes them in a distant destructor. No leak was established, but this style makes exceptional paths and later changes harder to audit.

- **Some normal-looking components are placeholders.** HCCA is present in the composition but throws when used. Several NED defaults are explicitly described as wrong, such as the generic BPSK transmitter and receiver modulation defaults ([Ieee80211Transmitter.ned](src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.ned:20)).

- **The TODO backlog mixes limitations with defects.** Important protocol gaps, optimizations, stale design notes, and harmless future work are all represented as TODO/FIXME comments, making prioritization difficult.

### Opportunities

- Continue the `HcfExchangeCoordinator` direction by extracting:

  - immutable transmission preparation;
  - aggregation planning;
  - ACK/Block Ack result processing;
  - address interpretation;
  - protection and duration calculation.

  HCF should coordinate these services instead of implementing all their details.

- Introduce explicit pending authentication and association transactions. Commit MIB state only when the response ACK outcome is known.

- Replace string- and concrete-type-based feature detection with capability contracts such as “supports HE MU scheduling” or “provides canonical HE transmission context.”

- Complete management-frame serialization round trips, or clearly reject fields that cannot be represented. At present, some values are placeholders or discarded during deserialization.

- Move manually owned helpers to explicit RAII ownership where OMNeT++ ownership rules permit it.

- Turn meaningful TODOs into a tracked limitation list containing scope, expected behavior, affected modes, and test status. Remove stale scratchpad notes.

### Threats and future risks

- Supporting legacy modes through EHT in one inheritance hierarchy will continue increasing conditional logic and regression risk.

- Partial features can appear available simply because their modules and parameters exist. This can mislead users unless unsupported combinations fail early with clear messages.

- Central classes are approaching a point where small changes may affect unrelated exchanges, particularly retries, aggregation, management, and MU operation.

- New profile names or capability combinations may silently bypass HE behavior because of literal-name gating.

- Unresolved address-role and association-timing behavior could produce credible-looking but inaccurate results in less common scenarios.

## Efficiency observations

These are static indications, not measured performance problems:

- Buffer-status calculation scans queued and in-progress packets and their region tags ([Hcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc:87)).
- Block Ack fragment lookup uses a linear scan within each sequence entry ([ReceiveBuffer.cc](src/inet/linklayer/ieee80211/mac/blockackreordering/ReceiveBuffer.cc:25)).
- HE PHY validation rebuilds and sorts same-RU user lists for each user ([Ieee80211Transmitter.cc](src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.cc:472)).
- Some HE response paths duplicate whole packets for ownership handoff.
- CCA evaluation creates a filtered reception vector and records spatial-reuse decisions for each interferer ([Ieee80211Receiver.cc](src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.cc:733)).

These should be benchmarked before optimization. The highest-value efficiency improvement would probably be removing repeated work at scale, not micro-optimizing ordinary frame processing.

## Recommended priority

1. Correct authentication and association response-ACK state handling.
2. Centralize address-role interpretation and test all To DS/From DS combinations.
3. Break up HCF transmission preparation and result processing.
4. Replace literal mode names and repeated concrete-type checks with capability interfaces.
5. Complete or explicitly restrict management serialization.
6. Profile queue scans, packet duplication, repeated RU grouping, and signal volume.
7. Classify and clean up the permanent TODO/FIXME backlog.

No code was compiled, executed, benchmarked, or modified. Consequently, runtime correctness and performance conclusions remain risks or opportunities identified from source structure—not confirmed defects or measured bottlenecks.