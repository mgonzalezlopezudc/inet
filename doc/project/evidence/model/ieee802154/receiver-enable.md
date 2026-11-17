# Scheduled receiver control

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [managed-applicability.md](managed-applicability.md), [phy-provider.md](phy-provider.md)

## Implementation contract

IEEE Std 802.15.4-2024, 8.2.7.2–5/Tables 8-14–17, 6.5.3,
and 6.6.2 define this selected non-beacon service. The implementation maximum
is one RxInstanceDescriptor. An empty list cancels outstanding receiver control;
more than one item is INVALID_PARAMETER. Timestamps/durations are 24-bit symbol
values. Zero OnTime means ASAP, zero duration disables, and duration 0xffffff
means indefinite. Ranging is unsupported with RANGING_NOT_SUPPORTED.

The selected ideal clock counts 16-us symbols from simulation time zero and
wraps modulo 2^24. Nonzero OnTime is resolved by forward modular distance.
More than half a counter period with DeferPermit=false is PAST_TIME; exactly
half is allowed. DeferPermit=true waits for that next counter occurrence.
Invalid/late requests leave the old schedule unchanged. New valid schedules
cancel previous timers before installing their own state.

SUCCESS confirms installation of the validated receive task, not instantaneous
PHY readiness. An ASAP enable starts its duration at actual receive readiness,
after the selected PHY's turnaround and any conflicting task. A future absolute
enable prepares one turnaround before its start. When the PHY is otherwise idle
and not already receive-ready, an absolute target closer than that turnaround
is locally refused BUSY, without confirmation or schedule mutation; this avoids
claiming a physical deadline the binding cannot meet. Conflicting primary tasks
can interrupt/skip an absolute window as 8.2.7.3 permits. Their completion resumes
only the still-unexpired window. No speculative PHY-state cache is added.

The window is a secondary receive task, rather than a new radio owner. DATA/ACK,
POLL and scan keep their responsibilities; the sole MAC/PHY path remains
authoritative. At finite expiry the receive task releases its demand and emits
RX-ENABLE.indication with the deadline's raw counter. Other duties/idle policy
may still require receive or transmit mode. This interpretation reconciles
8.2.7.3's conflicting-duty rule and 6.6.2's idle restoration with the period-end
indication in 8.2.7.5: the indication does not promise every other duty has
physically switched the radio off. For zero OnTime, the timestamp formula uses
the effective actual start, because zero is the special ASAP value.

AutoOff consumes the task at the first delivered PHY packet in its active
interval, including a MAC-rejected frame. It cancels finite expiry notification;
the PHY packet is not thereby an accepted MSDU. Radio settling waits until frame
processing has established any immediate ACK or primary receive responsibility.
No zero-delay self-message is introduced for this ordinary continuation.

The installed task survives its correlated confirmation as configuration/activity;
cancel(id) therefore returns NOT_PENDING after confirmation. Replacement/empty
list controls it. RESET/stop cancel its timers without a second confirmation or
late indication. A pending atomic installation is tracked until confirmation so
reentrant lifecycle interruption cannot double-complete it. Activity identity
uses its existing request ID; replacement/reset invalidates old completion work.

Production evidence covers finite/future/minimum/maximum/indefinite
windows, AutoOff and rejected frames, replacement, invalid/ranging requests,
half-counter and rollover boundaries, DATA/ACK/POLL/scan priority, local deadline
refusal and reset/stop/replacement reentry. Radio mode, native packet delivery,
counter/timing and terminal cardinality are checked independently of the helper.
The source paths, units, callback continuations and lifecycle ownership were
validated before this increment's edits.
