# INET IEEE 802.11 — Architectural Requirements

These requirements extend the general rules in
[`architectural-requirements.md`](architectural-requirements.md) for development under:

* `src/inet/linklayer/ieee80211/`
* `src/inet/physicallayer/wireless/ieee80211/`

All general INET architectural, naming, testing, observability, packet, configuration and sealing requirements remain applicable.

This document contains only rules that are specific to IEEE 802.11 development. Do not duplicate general INET requirements here.

Requirement identifiers have the form `AR-WLAN-<AREA>-<NAME>`.

---

## Standard Semantics

### AR-WLAN-STD-TRACE — Normative behavior is traceable to the IEEE 802.11 standard

Every implementation of a normative state transition, timing rule, frame field, validity constraint or protocol decision must be traceable to a specific IEEE 802.11 revision and clause.

Place the reference at the main decision point or in its focused test, not on every line of implementation. Code must not invent behavior from intuition, another simulator or a particular device implementation without documenting that it is a deliberate model extension.

### AR-WLAN-STD-GATING — Amendment-specific behavior is explicitly gated

Behavior introduced by an amendment or PHY generation must execute only when the configured operation mode, local capabilities and relevant peer capabilities permit it.

Legacy operation must not change merely because newer functionality is compiled into the model. Do not detect capabilities through concrete C++ types, module paths, frame-field presence or unrelated configuration parameters.

---

## Component Responsibilities

### AR-WLAN-ARCH-BOUNDARIES — MAC, management, rate selection and PHY have distinct responsibilities

The MAC controls channel access and frame exchanges. Management controls association, authentication, scanning and peer capabilities. Rate selection chooses among legal transmission modes. The PHY calculates and models PPDUs, transmissions and receptions.

Components communicate through typed contracts, requests, indications and immutable result objects. A component must not downcast another component to access its implementation state or directly mutate state owned by another layer.

### AR-WLAN-ARCH-OWNERSHIP — Every mutable protocol state has exactly one owner

Each protocol state must have one authoritative owner. Other components query that owner or receive an immutable snapshot.

This applies in particular to:

* association and peer capability state;
* sequence-number allocation;
* retry counters;
* NAV, backoff and TXOP state;
* Block Ack agreements and reorder windows;
* per-access-category contention state;
* aggregation state;
* power-save state;
* selected PHY transmission parameters.

Do not maintain synchronized mutable copies of the same state in several modules.

### AR-WLAN-ARCH-VARIANTS — Variations are implemented through replaceable policies

Behavior that varies by amendment, station role or algorithm must be isolated behind a replaceable component, strategy or typed contract when the variation has independent state or substantial logic.

Do not grow common classes through scattered conditions such as `isHe`, `isAp`, `useOFDMA` or `supportsBlockAck`. Small local conditions are acceptable; repeated amendment or role checks across multiple methods indicate a missing abstraction.

---

## Frames and Metadata

### AR-WLAN-FRAME-REPRESENTATION — On-air information is represented once as typed packet content

Every field transmitted over the wireless medium belongs in a typed frame, header or trailer chunk with the correct serialized length and encoding.

Local simulation information belongs in packet tags, requests or indications and must not silently become part of the frame. Conversely, information required by a receiver must not exist only as a sender-side tag.

New frame formats must provide the applicable serializer, dissector and printer support. Common operations such as frame classification, address interpretation, sequence-control handling and TID extraction must be implemented once and reused.

Generated `*_m.h` and `*_m.cc` files must never be edited manually.

---

## PHY Modes and Timing

### AR-WLAN-PHY-AUTHORITY — PHY mode objects are authoritative for legality, rate and duration

The selected IEEE 802.11 PHY mode is the single source of truth for:

* channel width;
* modulation and coding;
* number of spatial streams;
* guard interval;
* RU allocation where applicable;
* preamble and PHY-header structure;
* symbol count;
* data rate;
* PPDU duration.

MAC and rate-selection code must not duplicate PHY lookup tables or duration formulas.

Combinations of MCS, bandwidth, guard interval, NSS, RU and preamble type must be validated at one boundary before transmission. An invalid combination must fail explicitly rather than being silently adjusted.

### AR-WLAN-PHY-TIMING — Protocol timing is derived, unit-safe and centralized

SIFS, slot time, interframe spaces, response timeouts, NAV durations and transmission durations must be derived from configured standard parameters and the selected PHY mode.

Do not spread numeric timing constants or duplicate timing equations across MAC, management and PHY code. NED parameters expose independent model inputs; values that can be calculated from those inputs are calculated rather than separately configured.

Rounding and symbol-boundary rules must be applied in one canonical implementation.

---

## MAC Operation

### AR-WLAN-MAC-EXCHANGE — Each frame exchange is controlled by one explicit state machine

A frame exchange—including transmission, expected responses, response timing, timeout, retry and completion—must have one authoritative owner and an explicit state.

ACK, CTS, Block Ack and management-frame response handling must not be distributed across unrelated modules with partially duplicated decisions.

Each timer represents one protocol event. Backoff freeze and resume preserve the remaining contention state rather than approximating it by restarting. Simultaneous events that may affect the outcome must have a deterministic and documented priority.

### AR-WLAN-MAC-SEQUENCE — Sequence space, aggregation and Block Ack use shared rules

Sequence-number allocation, comparison and window advancement must use common modulo-sequence-space operations. Raw integer comparisons must not be used where wrap-around is possible.

A retransmitted MPDU retains its protocol identity and sequence number. Aggregation must not hide or replace the identity of its constituent MPDUs.

Block Ack agreement state and reorder-window state each have one owner. The transmitter and receiver may maintain their respective protocol state, but no component may keep an independent shadow copy of another component’s window.

### AR-WLAN-MAC-QOS — QoS classification and EDCA state are defined once

The mapping from user priority or TID to access category must be centralized and applied once before contention.

Each access category owns its queue, contention parameters, retry state and TXOP state. Code must not reconstruct the access category from unrelated packet properties later in the transmission path.

Internal collisions between access categories, TXOP limits and per-AC retry behavior must be resolved by the channel-access component rather than by queue ordering accidents.

### AR-WLAN-MAC-MULTIUSER — Multi-user scheduling is separate from PPDU construction

An OFDMA or MU-MIMO scheduler selects users and resources and produces a validated, immutable transmission plan. PHY code converts that plan into a PPDU but does not make scheduling decisions.

The plan must contain the information needed to validate and reproduce the transmission, such as recipients, traffic identifiers, resource units or spatial streams, transmission modes and relevant timing.

The scheduler must use public queue, capability and channel-state contracts. It must not inspect or mutate concrete PHY internals or bypass normal queue ownership.

---

## Observability

### AR-WLAN-OBS-EVENTS — Semantic events are emitted once by their owner

The component that owns a protocol action emits its semantic event exactly once.

Examples include:

* contention started or completed;
* transmission attempt started;
* frame transmitted;
* expected response received or timed out;
* retry performed or discarded;
* Block Ack agreement changed;
* aggregation created;
* reorder-window advanced;
* association state changed;
* multi-user allocation selected.

Signals carry sufficient context to classify the event without reading private module state. Statistics, visualizers and trace collectors subscribe externally and do not participate in protocol decisions.

---

## Verification

### AR-WLAN-QUAL-TESTS — Normative behavior is verified by focused tests and legacy regressions

Every new normative behavior must have a focused test that verifies the relevant state, frame content or event timeline.

Tests must cover the boundaries that apply to the change, including:

* timeout and interframe-space boundaries;
* sequence-number wrap-around;
* retry limits;
* malformed or unsupported frames;
* capability combinations;
* AP and non-AP roles;
* unicast, multicast and broadcast behavior;
* coexistence with legacy operation modes;
* aggregation and Block Ack window boundaries;
* single-user and multi-user operation.

Fingerprints supplement these tests but do not replace them. An intentional fingerprint change must be reviewed separately from the implementation.

Scheduling, user selection and tie-breaking must be deterministic and based on modeled state such as address, association identifier, TID or queue order—not pointer values, allocation order or unordered-container iteration.

---

## Applying These Requirements

The global agent-review checklist remains mandatory. The IEEE 802.11 checklist adds only the questions specific to this document.

Exceptions use the existing
[`architecture-exceptions.md`](architecture-exceptions.md) ledger with an `AR-WLAN-*` rule identifier. Do not create a separate exception mechanism.

Naming uses the existing
[`naming-conventions.md`](naming-conventions.md). IEEE terminology should follow the standard while still obeying INET casing conventions, for example `Ieee80211`, `BlockAck`, `Mpdu`, `Ppdu`, `Tid`, `Txop` and `MuMimo`.

Sealing continues to use the existing
[`sealing.md`](sealing.md) and
[`sealing-status.md`](sealing-status.md) files.
