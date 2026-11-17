# IEEE 802.15.4 — coverage ledger

> **Kind:** ledger · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [conformance.md](conformance.md), [derive-tests-from-a-standard.md](../../../guide/derive-tests-from-a-standard.md)

The legacy model-family survey remains at **1 (Survey)**. The replacement has executed
native integration, codec, PHY/access and production protocol evidence recorded in
[results.md](results.md). The current per-statement mapping is
[statement-results.md](statement-results.md), with explicit native-binding limits.
These results do not assert whole-standard or waveform conformance.
The selected M1/M2 execution closes steps 0–10: 17 production protocol fixtures
pass 564 simulations, 14 module fixtures and eight unit fixtures pass, and the
native wire/capture, external PHY, managed example and migration gates have
executed results. [verification-20261115.json](verification-20261115.json)
records counts, exact test selectors, source identity and explicit limits.
PIB-8 is conditional on an indexed service absent from this whole-value binding;
all other verdicts retain their catalog predicates. Later feature packages remain
outside these selected milestones.
The conventional beacon/slotted/GTS extension is tracked separately in
[followup-results.md](followup-results.md) and its
[twelve-statement mapping](statement-results.md#conventional-superframe-extension).
It adds periodic/linked schedules, acquisition/loss, slotted access, both CFP
directions, deallocation and activity expiry under the
[bounded profile](beacon-profile.md). B-99's package regression and stored trajectory
controls pass. Enhanced-frame source decisions are in
[enhanced-frame-profile.md](enhanced-frame-profile.md); structural, unsecured DATA/ACK
and one-shot discovery stages have executed evidence in the
[enhanced statement mapping](statement-results.md#enhanced-frame-extension).
Scheduled enhanced integration and the final combined publication matrix remain open.
The step-0 table below preserves its initial extraction verdicts; the current executable
support table distinguishes subsequent evidence from remaining obligations. Successful
corpus retrieval validates tooling, not simulation behavior.

| Area | Survey evidence | Next evidence needed |
| --- | --- | --- |
| Edition and family | Pinned 2024 base; later amendments explicitly excluded | Older 2006/2007 texts for exact comparison when a change relies on an older claim |
| MAC access and ACK/retry behavior | Claim sources mapped to 6.3 and 6.6 | Specification-derived catalog, English checks, then production-path exchanges |
| Frame representation | Serializer limitation identified; mapped to clause 7 | Independent header/serialization checks and captured frame evidence |
| Narrowband PHY | Claim sources mapped to 11–13; sensitivity approximation identified | Parameter applicability, duration checks, and appropriately scoped reception/error tests |
| UWB PHY and interface | Historical PHY claim mapped to 16; distinct MAC composition identified | Mode-specific comparison and PHY evidence; separate MAC claim selection |

## Step-0 extraction pass

Initial baseline: `fd6f800222`; latest continuation baseline: `5f3845f055`, 2026-09-19. Scope and open dependencies are in
[applicability.md](applicability.md); commands, source findings and capture inspection are in
[results.md](results.md). The [catalog](../../standard/ieee802154/catalog.md),
[feature map](../../protocol/ieee802154/features.md) and
[English checks](../../protocol/ieee802154/checks.md) define the bounded engineering audit. **Step 0 passed independent review on 2026-09-19;
this verdict covered the applicability audit, not implementation.**

There are 87 extracted statements (85 base-standard statements and two imported address definitions): 84 selected for the M1 engineering subset and 3 deferred
selected-profile obligations. These are counts of the current extraction, not the denominator
of a complete profile. At that extraction baseline all executable verdicts were `NOT_RUN`.
Subsequent results are recorded below. Existing survey claims are not promoted merely by
writing English checks, and per-statement release closure still requires its direct mapping.

### Initial M1 engineering subset and deferred obligations

This is the 2026-09-19 extraction snapshot, before the replacement implementation.
`selected` meant intended for M1; `later` meant owed by a subsequent selected profile.
Neither selection is an applicability exclusion or a current execution verdict.

| Statement | Initial selection | Delivery | English check | Initial executable / verdict |
| --- | --- | --- | --- | --- |
| [IEEE802154-ADDRESS-1](../../standard/ieee802154/catalog.md#ieee802154-address-1) | selected; owed | 1a, 1d, 2 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-ADDRESS-2](../../standard/ieee802154/catalog.md#ieee802154-address-2) | selected; owed | 1a, 1d, 2 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-1](../../standard/ieee802154/catalog.md#ieee802154-wire-1) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-2](../../standard/ieee802154/catalog.md#ieee802154-wire-2) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-3](../../standard/ieee802154/catalog.md#ieee802154-wire-3) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-4](../../standard/ieee802154/catalog.md#ieee802154-wire-4) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-5](../../standard/ieee802154/catalog.md#ieee802154-wire-5) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-6](../../standard/ieee802154/catalog.md#ieee802154-wire-6) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-ACCESS-1](../../standard/ieee802154/catalog.md#ieee802154-access-1) | selected; owed | 5 | [IEEE802154-C-ACCESS](../../protocol/ieee802154/checks.md#ieee802154-c-access) | None / `NOT_RUN` |
| [IEEE802154-ACCESS-2](../../standard/ieee802154/catalog.md#ieee802154-access-2) | selected; owed | 5 | [IEEE802154-C-ACCESS](../../protocol/ieee802154/checks.md#ieee802154-c-access) | None / `NOT_RUN` |
| [IEEE802154-SEQUENCE-1](../../standard/ieee802154/catalog.md#ieee802154-sequence-1) | selected; owed | 6 | [IEEE802154-C-SEQUENCE](../../protocol/ieee802154/checks.md#ieee802154-c-sequence) | None / `NOT_RUN` |
| [IEEE802154-SEQUENCE-2](../../standard/ieee802154/catalog.md#ieee802154-sequence-2) | selected; owed | 6 | [IEEE802154-C-SEQUENCE](../../protocol/ieee802154/checks.md#ieee802154-c-sequence) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-1](../../standard/ieee802154/catalog.md#ieee802154-receive-1) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-2](../../standard/ieee802154/catalog.md#ieee802154-receive-2) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-3](../../standard/ieee802154/catalog.md#ieee802154-receive-3) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-4](../../standard/ieee802154/catalog.md#ieee802154-receive-4) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-ACK-1](../../standard/ieee802154/catalog.md#ieee802154-ack-1) | selected; owed | 6 | [IEEE802154-C-ACK](../../protocol/ieee802154/checks.md#ieee802154-c-ack) | None / `NOT_RUN` |
| [IEEE802154-ACK-2](../../standard/ieee802154/catalog.md#ieee802154-ack-2) | selected; owed | 6 | [IEEE802154-C-ACK](../../protocol/ieee802154/checks.md#ieee802154-c-ack) | None / `NOT_RUN` |
| [IEEE802154-ACK-3](../../standard/ieee802154/catalog.md#ieee802154-ack-3) | selected; owed | 6 | [IEEE802154-C-ACK](../../protocol/ieee802154/checks.md#ieee802154-c-ack) | None / `NOT_RUN` |
| [IEEE802154-ACK-4](../../standard/ieee802154/catalog.md#ieee802154-ack-4) | selected; owed | 6 | [IEEE802154-C-ACK](../../protocol/ieee802154/checks.md#ieee802154-c-ack) | None / `NOT_RUN` |
| [IEEE802154-ACK-5](../../standard/ieee802154/catalog.md#ieee802154-ack-5) | later; owed | M2 step 7 | [IEEE802154-C-ACK](../../protocol/ieee802154/checks.md#ieee802154-c-ack) | None / `NOT_RUN` |
| [IEEE802154-SECURITY-1](../../standard/ieee802154/catalog.md#ieee802154-security-1) | selected; owed | 6 | [IEEE802154-C-SECURITY](../../protocol/ieee802154/checks.md#ieee802154-c-security) | None / `NOT_RUN` |
| [IEEE802154-SECURITY-2](../../standard/ieee802154/catalog.md#ieee802154-security-2) | selected; owed | 6 | [IEEE802154-C-SECURITY](../../protocol/ieee802154/checks.md#ieee802154-c-security) | None / `NOT_RUN` |
| [IEEE802154-SECURITY-3](../../standard/ieee802154/catalog.md#ieee802154-security-3) | selected; owed | 6 | [IEEE802154-C-SECURITY](../../protocol/ieee802154/checks.md#ieee802154-c-security) | None / `NOT_RUN` |
| [IEEE802154-SECURITY-4](../../standard/ieee802154/catalog.md#ieee802154-security-4) | selected; owed | 6 | [IEEE802154-C-SECURITY](../../protocol/ieee802154/checks.md#ieee802154-c-security) | None / `NOT_RUN` |
| [IEEE802154-SECURITY-5](../../standard/ieee802154/catalog.md#ieee802154-security-5) | selected; owed | 6 | [IEEE802154-C-SECURITY](../../protocol/ieee802154/checks.md#ieee802154-c-security) | None / `NOT_RUN` |
| [IEEE802154-PHY-1](../../standard/ieee802154/catalog.md#ieee802154-phy-1) | selected; owed | 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-2](../../standard/ieee802154/catalog.md#ieee802154-phy-2) | selected; owed | 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-3](../../standard/ieee802154/catalog.md#ieee802154-phy-3) | selected; owed | 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-4](../../standard/ieee802154/catalog.md#ieee802154-phy-4) | selected; owed | 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-5](../../standard/ieee802154/catalog.md#ieee802154-phy-5) | selected; owed | 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-6](../../standard/ieee802154/catalog.md#ieee802154-phy-6) | selected; owed | 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-7](../../standard/ieee802154/catalog.md#ieee802154-phy-7) | selected; owed | 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-8](../../standard/ieee802154/catalog.md#ieee802154-phy-8) | selected; owed | 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-SERVICE-1](../../standard/ieee802154/catalog.md#ieee802154-service-1) | selected; owed | 1a, 3, 6 | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-PIB-1](../../standard/ieee802154/catalog.md#ieee802154-pib-1) | selected; owed | 1b | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-PIB-2](../../standard/ieee802154/catalog.md#ieee802154-pib-2) | selected; owed | 1b | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-PIB-3](../../standard/ieee802154/catalog.md#ieee802154-pib-3) | selected; owed | 1b | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-SCAN-1](../../standard/ieee802154/catalog.md#ieee802154-scan-1) | later; owed | M2 step 8 | [IEEE802154-C-SCAN](../../protocol/ieee802154/checks.md#ieee802154-c-scan) | None / `NOT_RUN` |
| [IEEE802154-SCAN-2](../../standard/ieee802154/catalog.md#ieee802154-scan-2) | later; owed | M2 step 8 | [IEEE802154-C-SCAN](../../protocol/ieee802154/checks.md#ieee802154-c-scan) | None / `NOT_RUN` |
| [IEEE802154-WIRE-7](../../standard/ieee802154/catalog.md#ieee802154-wire-7) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-8](../../standard/ieee802154/catalog.md#ieee802154-wire-8) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-9](../../standard/ieee802154/catalog.md#ieee802154-wire-9) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-10](../../standard/ieee802154/catalog.md#ieee802154-wire-10) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-11](../../standard/ieee802154/catalog.md#ieee802154-wire-11) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-12](../../standard/ieee802154/catalog.md#ieee802154-wire-12) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-13](../../standard/ieee802154/catalog.md#ieee802154-wire-13) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-14](../../standard/ieee802154/catalog.md#ieee802154-wire-14) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-15](../../standard/ieee802154/catalog.md#ieee802154-wire-15) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-WIRE-16](../../standard/ieee802154/catalog.md#ieee802154-wire-16) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-5](../../standard/ieee802154/catalog.md#ieee802154-receive-5) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-6](../../standard/ieee802154/catalog.md#ieee802154-receive-6) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-7](../../standard/ieee802154/catalog.md#ieee802154-receive-7) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-8](../../standard/ieee802154/catalog.md#ieee802154-receive-8) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-9](../../standard/ieee802154/catalog.md#ieee802154-receive-9) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-10](../../standard/ieee802154/catalog.md#ieee802154-receive-10) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-11](../../standard/ieee802154/catalog.md#ieee802154-receive-11) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-12](../../standard/ieee802154/catalog.md#ieee802154-receive-12) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-13](../../standard/ieee802154/catalog.md#ieee802154-receive-13) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-14](../../standard/ieee802154/catalog.md#ieee802154-receive-14) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-15](../../standard/ieee802154/catalog.md#ieee802154-receive-15) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-PHY-9](../../standard/ieee802154/catalog.md#ieee802154-phy-9) | selected; owed | 1c, 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-10](../../standard/ieee802154/catalog.md#ieee802154-phy-10) | selected; owed | 1c, 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-11](../../standard/ieee802154/catalog.md#ieee802154-phy-11) | selected; owed | 1c, 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-12](../../standard/ieee802154/catalog.md#ieee802154-phy-12) | selected; owed | 1c, 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-13](../../standard/ieee802154/catalog.md#ieee802154-phy-13) | selected; owed | 1c, 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-14](../../standard/ieee802154/catalog.md#ieee802154-phy-14) | selected; owed | 1c, 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PHY-15](../../standard/ieee802154/catalog.md#ieee802154-phy-15) | selected; owed | 1c, 4 | [IEEE802154-C-PHY](../../protocol/ieee802154/checks.md#ieee802154-c-phy) | None / `NOT_RUN` |
| [IEEE802154-PIB-4](../../standard/ieee802154/catalog.md#ieee802154-pib-4) | selected; owed | 1b | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-PIB-5](../../standard/ieee802154/catalog.md#ieee802154-pib-5) | selected; owed | 1b | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-PIB-6](../../standard/ieee802154/catalog.md#ieee802154-pib-6) | selected; owed | 1b | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-PIB-7](../../standard/ieee802154/catalog.md#ieee802154-pib-7) | selected; owed | 1b | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-PIB-8](../../standard/ieee802154/catalog.md#ieee802154-pib-8) | selected; owed | 1b | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-PIB-9](../../standard/ieee802154/catalog.md#ieee802154-pib-9) | selected; owed | 1b | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-SERVICE-2](../../standard/ieee802154/catalog.md#ieee802154-service-2) | selected; owed | 1a, 3, 6 | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-SERVICE-3](../../standard/ieee802154/catalog.md#ieee802154-service-3) | selected; owed | 1a, 3, 6 | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-SERVICE-4](../../standard/ieee802154/catalog.md#ieee802154-service-4) | selected; owed | 1a, 3, 6 | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-SERVICE-5](../../standard/ieee802154/catalog.md#ieee802154-service-5) | selected; owed | 1a, 3, 6 | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-SERVICE-6](../../standard/ieee802154/catalog.md#ieee802154-service-6) | selected; owed | 1a, 3, 6 | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-SERVICE-7](../../standard/ieee802154/catalog.md#ieee802154-service-7) | selected; owed | 1a, 3, 6 | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-WIRE-17](../../standard/ieee802154/catalog.md#ieee802154-wire-17) | selected; owed | 2–3 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-RECEIVE-16](../../standard/ieee802154/catalog.md#ieee802154-receive-16) | selected; owed | 6 | [IEEE802154-C-RECEIVE](../../protocol/ieee802154/checks.md#ieee802154-c-receive) | None / `NOT_RUN` |
| [IEEE802154-SERVICE-8](../../standard/ieee802154/catalog.md#ieee802154-service-8) | selected; owed | 1a, 6 | [IEEE802154-C-SECURITY](../../protocol/ieee802154/checks.md#ieee802154-c-security) | None / `NOT_RUN` |
| [IEEE802-ADDRESS-1](../../standard/ieee802/catalog.md#ieee802-address-1) | selected; owed | 1a, 2, 6 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802-ADDRESS-2](../../standard/ieee802/catalog.md#ieee802-address-2) | selected; owed | 1a, 2, 6 | [IEEE802154-C-WIRE](../../protocol/ieee802154/checks.md#ieee802154-c-wire) | None / `NOT_RUN` |
| [IEEE802154-SERVICE-9](../../standard/ieee802154/catalog.md#ieee802154-service-9) | selected; owed | 1a, 1b | [IEEE802154-C-SERVICE](../../protocol/ieee802154/checks.md#ieee802154-c-service) | None / `NOT_RUN` |
| [IEEE802154-ACCESS-3](../../standard/ieee802154/catalog.md#ieee802154-access-3) | selected; owed | 1c, 6 | [IEEE802154-C-ACK](../../protocol/ieee802154/checks.md#ieee802154-c-ack) | None / `NOT_RUN` |

### Current executable support

| Check | Intended evidence | Current support evidence |
| --- | --- | --- |
| IEEE802154-C-WIRE | Unit/serializer plus native-interface production fixture and independent octets | Address/FrameWire/Fcs/Commands/Beacons and Capture unit fixtures, native-interface module and admitted four-frame serializer fixture pass. Scapy independently checks fields/CRCs. The authorized protected-reader repair preserves truncated captures. |
| IEEE802154-C-ACCESS | Module plus protocol trace reaching actual transmit/no-transmit outcome | Production UnslottedAccess module and protocol fixture pass; 20-seed contention campaign passes. |
| IEEE802154-C-SEQUENCE | Protocol exchange through serialization and receiver indication | SequenceWrap protocol fixture passes 260-request exchanges across peers, wrap, retry and restart. |
| IEEE802154-C-RECEIVE | Protocol injection plus upper indication and ACK observation | ReceiveSecurity protocol fixture passes its selected PAN/address/security/monitor cases. Uncovered predicates remain owed; this is not a blanket statement verdict. |
| IEEE802154-C-ACK | Protocol loss injection and PHY timing | DataAck and InterframeSpacing pass wrong-DSN, corrupt, unsupported-version, exact-deadline and late ACK cases. Indirect verifies retry only after a new poll; Broadcast verifies actual unacknowledged beacon release. |
| IEEE802154-C-SECURITY | Protocol/service fixture separating status, ACK and delivery | Secured version-0/1 common framing, actual ACK and distinct COMM-STATUS/no-plaintext paths pass. No cryptographic processing is implemented. |
| IEEE802154-C-PHY | Module timing/energy observations; separate external validation | PhyServices, Measurements, EnergyObservation and PhyPib pass. ExternalPhy observes encoded SHR/PHR/PSDU views, exact airtime and BER/PER calculations against a pinned external reference. Hardware PER, chip/pulse waveforms and acquisition probability are outside this packet-level model. |
| IEEE802154-C-SERVICE | Module PIB/reset plus production request capacity tests | MacPib and production lifecycle/PHY-delegation/indirect paths pass. Admission checks actual maximum/oversize/empty/non-octet MSDUs, invalid requests, packet ownership and DSN preservation. NativeGate proves the public default interface's actual DATA/ACK and receiver-owned dispatch. |
| IEEE802154-C-SCAN | Production START, passive/ED/active/orphan scan and beacon cases | Sorting, descriptor identity, peaks, notifications, restoration, zero-duration ED, exact-deadline/late beacons, all-unscanned active failure, orphan realignment and PAN conflict have production evidence. |

Commands, build modes, exact executed counts and artifacts are in [results.md](results.md).
Supporting cases do not automatically close every statement within a feature group. The
[statement mapping](statement-results.md) gives each extracted statement its direct cases
and limits. M2's procedure inventory and source decisions are in
[managed-applicability.md](managed-applicability.md). Release build/regression status remains
separate in [results.md](results.md), including inherited failures and expected-error checks.

## Pass log

| Date | Pass | Outcome |
| --- | --- | --- |
| 2026-09-18 | Standards/claim survey | Level 1; no simulation checks |
| 2026-09-19 | Initial step-0 extraction | 40 source-checked entries, nine English procedures, role/version/receive matrices and first consumer/capture inventory; applicability closure and executable tests still owed |
| 2026-09-19 | Extended step-0 audit | 82 entries and nine expanded English procedures; complete base MAC/PHY PIB field inventory, service/PICS comparison and PHY feasibility inspection; no runtime support verdict |
| 2026-09-19 | Service/integration dependency closure | 83 statements; direct security-error service routing, channel descriptor, clause-6 procedure dispositions and native consumer compatibility recorded; runtime tests remain NOT_RUN |
| 2026-09-19 | Companion address reference | Pinned IEEE Std 802-2024; two imported definitions and three address vectors resolve group-bit layout; sender/receiver AR distinction resolved without an invented receive-filter rule |
| 2026-09-19 | Bounded M1 step-0 closure | PASS after independent review; 87 mapped statements and nine procedures, explicit policy/timing/admission decisions and bounded reference dispositions; no executable support evidence |

## Step-1a value evidence

The native [Ieee802154Address](../../../../../src/inet/linklayer/ieee802154/Ieee802154Address.h)
value has one passing [unit case](../../../../../tests/unit/Ieee802154Address_1.test), run in debug
mode. It checks none/short/extended identity, full-width values including equal-low-48-bit pairs,
strict parsing, atomic rejection, comparisons, hashing and reserved/group/broadcast classification.

This is partial supporting evidence for the address portion of package 1a. It does not execute
IEEE802154-C-WIRE, prove byte order, or exercise the real interface. The catalog-to-protocol-check
verdicts above remain NOT_RUN; package 1d production integration remains owed.
Commands and limitations are in [results.md](results.md#step-1a-native-address-value).

## Step-1a service-contract evidence

The paired MAC service interfaces have one passing debug
[module case](../../../../../tests/module/Ieee802154MacServiceContract_1.test) using a test-local
provider and client. It exercises admission, packet ownership, inline/deferred completion,
request correlation, cancellation, reset ordering, lifecycle changes and deleted-client cleanup.
Independent review passed with 18 PASS, 8 N/A, 0 FLAG and 0 QUESTION.

Together with the native value case, this closes package 1a's contract gate. It does not establish
operational MCPS/MLME support, PIB validation, reset-interruption edge paths, production address
integration or any of the nine normative protocol checks. Their verdicts remain NOT_RUN.
Commands, intermediate fixture correction and limits are in
[results.md](results.md#step-1a-mac-service-contracts).

## Step-1b PIB evidence

The [selected MAC PIB](pib.md) has one passing debug
[unit case](../../../../../tests/unit/Ieee802154MacPib_1.test) that directly invokes the compiled
store. It checks all 43 selected attribute defaults/access rules, type/domain rejection, failed
write preservation, startup BE-pair validation, address identity, rounding and reset behavior.
Independent review returned 11 PASS, 15 N/A, 0 FLAG and 0 QUESTION.

This closes package 1b at the standalone store boundary. It is supporting evidence for
IEEE802154-C-SERVICE, not execution of that protocol check: no production service provider,
NED import, radio mutation, notifications or full MLME reset is exercised. The normative
check verdicts remain NOT_RUN. Commands and limitations are in
[results.md](results.md#step-1b-selected-mac-pib).

## Step-1c PHY contract evidence

The paired PHY service roles and [timing/ownership contract](phy-services.md) have one passing
debug [module case](../../../../../tests/module/Ieee802154PhyServiceContract_1.test), fixed run
and seed 0. It covers accepted/refused PSDU ownership, inline status results, copied native
identity, channel descriptor validation, actual versus predicted timestamps, CCA/ED result
validity, exact requested transmit start, cancellation, reset/stop barriers, restart, stale IDs,
deadline event precedence and deleted-client cleanup.

This closes package 1c's structural contract boundary. Its provider is test-local: no existing
radio is adapted, no actual channel energy is measured and no full MAC exchange executes.
The normative C-PHY/C-ACK verdicts remain NOT_RUN. Runtime PHY PIB context refresh and validation
of real propagation/ACK timing assumptions remain production work. Commands, primary-source
retrieval and limitations are in [results.md](results.md#step-1c-phy-service-contracts).
