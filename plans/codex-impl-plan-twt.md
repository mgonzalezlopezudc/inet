# Standards-Wire-Compatible 802.11ax TWT Support

## Summary

Implement individual and broadcast TWT for HE infrastructure BSSs, including implicit and explicit scheduling, announced and unannounced service periods, runtime management primitives, real radio sleep, AP buffering, and HE DL/UL OFDMA eligibility. Protected TWT is explicitly unsupported: protected-operation requests fail and protected TWT action frames are dropped with diagnostics.

TWT is disabled by default and requires `modeSet=ax` plus `qosStation=true`.

## Implementation changes

### Existing generated messages and serializers

- Extend `Ieee80211Frame.msg` with:
  - `Ieee80211TwtSetupFrame`, `Ieee80211TwtTeardownFrame`, and `Ieee80211TwtInformationFrame`;
  - individual and broadcast TWT parameter-set fields, including negotiation type, request/setup command, flow or broadcast ID, target wake time, duration, interval, persistence, trigger, implicit, and announced/unannounced state;
  - `Ieee80211PsPollFrame`, so announced TWT uses a real PS-Poll rather than a simulation-only notification.

- Extend `Ieee80211MgmtFrame.msg` with broadcast TWT elements on Beacon frames and TWT capability fields in HE capability elements.

- Extend `Ieee80211Primitives.msg` with runtime APIs:
  - `Ieee80211Prim_TwtSetupRequest` / `Confirm` / `Indication`;
  - `Ieee80211Prim_TwtTeardownRequest` / `Confirm`;
  - `Ieee80211Prim_TwtInformationRequest` / `Confirm`;
  - `Ieee80211Prim_BroadcastTwtScheduleRequest` / `Confirm`, with create, update, and delete operations.
  - Setup primitives carry peer, negotiation type, requested parameters, and command. Broadcast membership is a setup request with broadcast-membership negotiation type.

- Update `Ieee80211MacHeaderSerializer` and `Ieee80211MgmtFrameSerializer` to serialize and validate all new fields exactly on the wire. Invalid action/category combinations, malformed element lengths, reserved values, and protected-TWT frames are rejected.

### Existing C++ classes

- `Ieee80211HeCapabilities`, `Ieee80211HeMgmtElements`, and `Ieee80211Mib`:
  - advertise requester, responder, and broadcast TWT capabilities;
  - retain peer capability state needed to reject unsupported setup attempts.

- `Ieee80211MgmtBase`:
  - add action-frame dispatch instead of unconditionally dropping action frames;
  - dispatch only TWT actions to management; preserve Block Ack and existing HE action processing.

- `Ieee80211MgmtSta`:
  - handle all STA-originated TWT primitives;
  - maintain pending setup transactions by dialog token;
  - install/remove agreements through the TWT manager;
  - send TWT Setup, Teardown, Information, and announced-SP PS-Poll frames.

- `Ieee80211MgmtAp`:
  - accept runtime TWT primitives on `agentIn`;
  - apply deterministic admission:
    - Request → accept with configured AP defaults;
    - Suggest → accept exact values if feasible, otherwise Alternate with AP defaults;
    - Demand → accept exact values or Reject;
    - broadcast join → accept only an advertised schedule with available membership.
  - add active broadcast schedules to Beacons and process membership/setup/teardown actions.

- `Ieee80211AgentSta`:
  - expose public helper methods for the new primitives;
  - forward TWT confirms and indications to subclasses/custom agents;
  - do not automatically establish TWT; application or custom-agent code decides when to issue runtime requests.

- `Ieee80211Mac`:
  - resolve the TWT manager;
  - expose narrow APIs for TWT radio-state updates and transmission eligibility;
  - return to sleep after transmission when no service period remains active.

- `Hcf` and `HeHcf`:
  - defer all STA-originated data transmission while its TWT radio is asleep;
  - let the TWT manager notify them when an SP opens;
  - exclude sleeping individual-TWT STAs and non-member broadcast-TWT STAs from SU, DL MU, UL Trigger, and BSRP scheduling;
  - reserve announced-SP downlink delivery until the AP receives PS-Poll;
  - leave non-TWT and legacy stations eligible exactly as before.

- `RecipientQosMacDataService`:
  - forward TWT action frames to management rather than deleting them;
  - retain existing DELBA and other action-frame behavior.

### New C++ classes

| New class | Responsibility |
|---|---|
| `TwtAgreement` / `TwtBroadcastSchedule` | Value types for individual agreements, broadcast schedules, membership, timing, and lifecycle state. |
| `Ieee80211TsfClock` | Converts simulation time to 64-bit TSF microseconds; no drift in v1, isolated for future clock models. |
| `ITwtManager` | Typed contract used by MAC and management: setup, update, teardown, eligibility, and SP notifications. |
| `Ieee80211TwtManager` | Owns active agreements/schedules, schedules SP timers, merges overlapping awake windows, controls radio sleep/wake, and publishes TWT signals/statistics. |
| `TwtPsPollFs` | Frame-sequence handler for announced TWT: transmits PS-Poll, waits for its response, then releases AP downlink eligibility for that SP. |

### NED modules and parameters

- Create `IIeee80211TwtManager.ned` and `Ieee80211TwtManager.ned`.

- Update `Ieee80211Interface.ned`:
  - add an always-present `twt` submodule, disabled by default;
  - pass its path to `mac` and `mgmt`.

- Update `Ieee80211Mac.ned`, `Ieee80211MgmtSta.ned`, and `Ieee80211MgmtAp.ned` with `twtModule` parameters.

- `Ieee80211TwtManager.ned` parameters:
  - `enabled`, `maxIndividualAgreementsPerPeer=8`, `maxBroadcastMembers`;
  - AP defaults for interval, wake duration, first-wake offset, persistence, and admission capacity;
  - `allowExplicit`, `allowAnnounced`, `allowBroadcast`, and `allowTriggerEnabled`;
  - signals/statistics for agreements, SP duration, awake/sleep time, buffered bytes, missed SPs, setup failures, and energy-related radio-mode residence.

## TWT behavior

- Individual agreements are keyed by peer MAC address plus flow ID. Broadcast membership is keyed by AP plus broadcast TWT ID.

- Implicit agreements calculate the next SP by adding the negotiated interval. Explicit agreements only advance when a valid TWT Information update supplies the next target wake time.

- The STA enters receiver/transceiver mode at the merged start of all active SPs and `RADIO_MODE_SLEEP` after the merged end. A currently transmitting frame may complete; no new exchange starts if it cannot fit in the remaining SP.

- Unannounced SPs permit AP delivery immediately. Announced SPs require the STA’s PS-Poll before AP buffered downlink traffic becomes eligible.

- The AP retains data in existing per-STA queues while a TWT STA sleeps. Broadcast schedules are advertised in Beacon frames; schedule updates follow beacon persistence rules, and missed Beacons retain the most recent schedule only for its negotiated persistence interval.

- Trigger-enabled SPs reuse existing HE Trigger/OFDMA machinery, but allocations are restricted to currently awake eligible members.

## Test plan and acceptance criteria

- Unit tests:
  - serializer byte vectors and round trips for Setup, Teardown, Information, PS-Poll, individual/broadcast elements, and HE capabilities;
  - malformed/reserved/protected-frame rejection;
  - Request/Suggest/Demand admission outcomes;
  - explicit next-TWT updates, interval arithmetic, overlap merging, teardown, membership, and beacon-persistence expiry.

- Integration scenarios under `examples/ieee80211/twt`:
  - individual implicit/unannounced delivery and radio sleep;
  - individual explicit/announced delivery, proving no AP data precedes PS-Poll;
  - broadcast schedule advertisement, join/leave, persistence, and shared-SP delivery;
  - DL/UL OFDMA with TWT and legacy STAs, proving sleeping STAs are never selected;
  - rejected unsupported capability, expired/missed schedule, and protected-TWT cases.

- Energy acceptance:
  - run deterministic baseline and TWT scenarios with identical offered traffic, seed, and radio energy model;
  - require identical delivered-packet count when SP capacity is sufficient;
  - require lower radio energy consumption and lower awake residence time for TWT than the always-awake baseline;
  - record energy, SP timing, buffering, and packet-delivery scalars for regression comparison.

- Compatibility:
  - all existing configurations remain unchanged with TWT disabled;
  - no new behavior is applied to non-HE, non-QoS, legacy, or TWT-incapable peers.
