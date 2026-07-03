## Contents

* 29. Causal Timeline Format
* 30. Diagnostic Invariants
* PHY invariants
* DCF invariants
* RTS/CTS invariants
* EDCA invariants
* Fragmentation invariants
* Aggregation invariants
* Management invariants
* 31. Applying a Fix
* 32. Verification
* 33. Required Final Report
* Environment
* Reproduction command
* Relevant configuration
* Expected exchange
* Observed exchange
* Evidence
* First divergence
* Root cause
* Fix
* Verification
* 34. Preferred Execution Sequence
* 35. Safety and Accuracy Rules

---

# 29. Causal Timeline Format

Build a compact timeline:

```text
t=1.241000000  client.app creates packet id=...
t=1.241002100  client.wlan queue receives packet, AC=BE
t=1.241010000  channel becomes idle
t=1.241044000  DIFS/AIFS completes
t=1.241053000  backoff begins, counter=7
t=1.241080000  medium busy, counter freezes at 4
t=1.241220000  medium idle
t=1.241254000  contention resumes
t=1.241290000  channel access granted
t=1.241291000  RTS selected because MPDU exceeds threshold
t=1.241300000  RTS transmission starts
t=1.241334000  AP receives RTS successfully
t=1.241350000  AP transmits CTS
t=1.241380000  client CTS reception fails due to interference
t=1.241410000  CTS timeout fires
t=1.241411000  retry counter increments
```

For each line, identify the evidence source:

```text
[PCAP]
[CMdenv]
[EVENTLOG]
[VECTOR]
[LLDB]
[SOURCE]
```

---

# 30. Diagnostic Invariants

Apply only when the relevant mechanism is enabled and modeled.

## PHY invariants

* Transmitter and receiver frequency ranges must overlap.
* A successful decode requires a reception attempt.
* A reception attempt requires detection and synchronization according to the receiver model.
* A frame below sensitivity should not decode successfully.
* Interference must overlap in time and relevant frequency to affect reception.
* Half-duplex radios cannot ordinarily receive while transmitting.

## DCF invariants

* Backoff decrements only during eligible idle slots.
* Backoff freezes while the medium is busy.
* Unicast DATA using normal ACK policy expects an ACK.
* Retransmission retains MPDU identity and sets Retry.
* Retry-limit exhaustion drops the frame.

## RTS/CTS invariants

* CTS follows successfully received RTS after SIFS.
* Protected DATA follows CTS after SIFS.
* Duration values reserve the remaining exchange.
* Stations that decode the reservation defer through NAV.

## EDCA invariants

* Each access category has its own contention parameters and state.
* AIFS depends on access category.
* Internal collision produces one local winner.
* TXOP duration must not exceed the applicable limit except for allowed completion behavior defined by the implementation.

## Fragmentation invariants

* Fragments of one MSDU share sequence identity.
* Fragment numbers progress.
* Defragmentation does not deliver incomplete data.
* Duplicate fragments are not delivered twice.

## Aggregation invariants

* Aggregated contents obey compatibility and size rules implemented by the policy.
* Deaggregation reconstructs the original units.
* Block Ack correlation matches MPDU sequence numbers where applicable.

## Management invariants

* Data exchange requiring association does not begin before association.
* STA and AP association state must agree.
* Scanning occurs on the channel being observed.
* Simplified management must not be expected to perform detailed handover exchanges.

---

# 31. Applying a Fix

A fix must address the demonstrated cause.

Valid categories include:

* Correct module path
* Correct radio/medium compatibility
* Correct channel or frequency
* Correct PHY mode compatibility
* Correct transmit power or receiver threshold
* Correct propagation or obstacle configuration
* Correct interference model
* Correct QoS classifier
* Correct EDCA parameter
* Correct RTS threshold
* Correct retry or timeout implementation
* Correct ACK or Block Ack policy
* Correct aggregation or fragmentation policy
* Correct association state transition
* Correct AP forwarding behavior
* Correct C++ state-machine logic

Do not “fix” the simulation by:

* Disabling interference
* Setting unrealistically high transmit power
* Making sensitivity unrealistically low
* Increasing every timeout
* Increasing every retry limit
* Setting every queue to unlimited
* Disabling ACKs
* Disabling validation
* Replacing the radio with an ideal model permanently
* Removing mobility
* Removing competing stations

unless the experiment intentionally requires that abstraction.

---

# 32. Verification

After applying the fix:

1. Rerun the exact failing seed.
2. Confirm the expected frame exchange.
3. Confirm timing relationships.
4. Confirm retry behavior.
5. Confirm no new packet drops.
6. Confirm association and forwarding state.
7. Compare captures before and after.
8. Compare targeted scalars and vectors.
9. Rerun several seeds.
10. Rerun the original non-debug configuration.
11. Remove temporary instrumentation.
12. Document any model-fidelity limitation.

For a PHY fix, verify multiple distances or interference levels.

For a MAC fix, verify:

* No traffic
* Light traffic
* Saturated traffic
* Multiple contenders
* Broadcast
* Unicast
* QoS classes
* Relevant aggregation and retry modes

---

# 33. Required Final Report

## Environment

```text
OMNeT++ version:
INET version and commit:
Simulation configuration:
Run:
Seed:
Executable:
Radio type:
Radio-medium type:
MAC mode:
Management type:
```

## Reproduction command

```sh
<exact command>
```

## Relevant configuration

```ini
<minimal relevant configuration>
```

## Expected exchange

```text
<expected frame and state sequence>
```

## Observed exchange

```text
<observed frame and state sequence>
```

## Evidence

Include:

* Capture filename
* Capture frame number
* Simulation time
* Event number
* Module path
* Frame subtype
* TA, RA, SA, DA, and BSSID
* Sequence and fragment numbers
* Retry bit
* TID and access category
* PHY mode
* Received power
* Noise and interference
* SNIR or error decision
* NAV and backoff state
* Retry count
* Drop reason
* Source location
* Stack trace when relevant

## First divergence

State the first point where expected and observed behavior differ.

## Root cause

Distinguish among:

* Configuration defect
* Topology defect
* PHY-model behavior
* MAC-model behavior
* Missing implementation
* Application or network-layer issue
* INET defect
* Project-specific code defect

## Fix

Provide the smallest justified change.

## Verification

Report:

* Exact failing seed now passes
* Frame exchange now matches expectations
* Relevant counters and drops
* Multi-seed outcome
* Remaining limitations

---

# 34. Preferred Execution Sequence

```text
1. Record OMNeT++, INET, configuration, run, and seed.
2. Inspect the instantiated 802.11 interface, MAC, management, radio, and medium.
3. Build a feature-support matrix.
4. Create a dedicated debugging configuration.
5. Add native 802.11 PCAPng recording at sender, receiver, and AP.
6. Run in Cmdenv.
7. Decode frame types, addresses, retries, and timing with TShark.
8. Identify the first observation point where behavior diverges.
9. Enable selective logs for that MAC or PHY component.
10. Query available scalar and vector results with opp_scavetool.
11. Enable a narrow event-log interval if timer causality is unclear.
12. Inspect the installed INET source for the relevant decision.
13. Run under LLDB with a targeted breakpoint.
14. Demonstrate the root cause using a causal timeline.
15. Apply the smallest justified fix.
16. Rerun the same seed and compare evidence.
17. Verify multiple seeds and the original configuration.
18. Remove temporary instrumentation.
```

---

# 35. Safety and Accuracy Rules

* Treat the checked-out INET source as authoritative for implementation behavior.
* Treat the applicable IEEE standard as authoritative for normative protocol behavior.
* Never assume the simulator implements every standard amendment.
* Never infer PHY reception solely from a MAC capture.
* Never infer collision solely from a missing ACK.
* Never infer data loss when ACK loss could explain a retry.
* Never infer hidden nodes without a carrier-sense and interference analysis.
* Never infer QoS priority solely from application type.
* Never compare Wi-Fi captures using capture frame numbers alone.
* Never assume Address 1 is the final destination.
* Never assume a nominal bitrate determines airtime.
* Never hardcode timing values without identifying the PHY mode.
* Never assume radiotap fields exist.
* Never enable unrestricted trace logs or event logs by default.
* Never replace a realistic radio with an ideal radio as the final repair.
* Never increase thresholds, timeouts, or retry limits without causal evidence.
* Always record the exact seed and run.
* Always distinguish standard behavior, INET abstraction, and project-specific behavior.
* Always report unsupported or simplified features explicitly.
