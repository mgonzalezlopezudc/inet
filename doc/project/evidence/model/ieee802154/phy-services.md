# IEEE 802.15.4 — PHY service contract

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [applicability.md](applicability.md), [coverage.md](coverage.md), [catalog.md](../../standard/ieee802154/catalog.md)

Package **IEEE802154-1C** defines the boundary between the replacement MAC and a substitutable
PHY provider. The paired C++/NED roles are `IIeee802154PhyServiceProvider` and
`IIeee802154PhyServiceClient`, under `physicallayer/wireless/ieee802154/contract`.
They do not install an operational provider on the existing narrowband or UWB interfaces.
The source requirements remain those in the audited catalog; no protocol support is inferred
from the presence of these interfaces.

IEEE Std 802.15.4-2024, 12.1 (continuation under `ieee802154-2024:figure:12-1`, physical
PDF p. 600) explicitly leaves PD-SAP and PLME-SAP undefined and routes PHY PIB access through
MLME-GET/SET. These typed methods and their local statuses are implementation contracts,
not reconstructed standardized primitive signatures. The MAC management owner will delegate
PHY attribute access through this boundary.

## Binding, admission and ownership

A concrete provider binds one typed `clientModule`, and a concrete client binds one typed
`providerModule`. The interfaces contain no mutable state or callback defaults. Binding is fixed
for those module instances; client deletion invalidates the provider's reference. A replacement
client is a new composition, not permission to reuse outstanding IDs.

The client issues nonzero, strictly increasing 64-bit IDs across every primitive, refused attempt,
reset and stop/start. The provider remembers the last attempted valid ID. `UINT64_MAX` is usable
once; the client must then stop issuing requests. Distinct IDs prevent an old completion from
matching a replacement operation. A provider cancels its underlying timers/operations on abort;
IDs do not excuse leaving old operations running.

The provider admits one operation at a time. A reset may preempt that operation. Admission
`BUSY`, `UNAVAILABLE`, `INVALID_REQUEST` or `UNSUPPORTED_OPERATION` transfers no packet and
produces no callback. An accepted operation produces exactly one terminal confirmation, unless
the bound client has disappeared; in that case the provider cleans up without calling it.
Admission is distinct from eventual success, medium occupancy, invalid configuration, and
transceiver readiness. Result enums are local API vocabulary, not IEEE numeric status codes.

An accepted transmit takes a complete PSDU, including MAC FCS, before returning or calling back.
The provider consumes or destroys it before terminal confirmation. A refused transmit leaves
the PSDU with the caller. Borrowed requests are copied before deferred use. Callback records are
borrowed for the call; receive indications transfer complete PSDUs to the client, which enters
module context and takes them immediately. The MAC still parses and verifies its own FCS.

Transmit-start is nonterminal and occurs at actual PPDU start. Its end and PSDU-start fields
are explicitly predictions. An already TRANSMIT-ready provider starts at the requested absolute
`startAt`, or the current instant when absent. A past target completes INVALID_PARAMETER
without transmission; an unavailable transceiver does not silently move the target. This gives
the MAC an exact ACK target after readiness. Cancellation before PPDU start aborts normally; after start it
returns `NOT_CANCELLABLE`, because transmitted airtime cannot be retracted. Reset/stop can
interrupt the radio and return an abort, but must not later report a successful transmission.
No start or terminal callback may follow a terminal result for the same ID.

## Results and configuration

| Operation | Successful output | Failure/abort output |
| --- | --- | --- |
| Transmit | Actual frame timing at PPDU end | No successful frame-timing record |
| CCA | IDLE or BUSY and actual observation start/end | Neither outcome nor measurement interval |
| ED | Level and actual observation interval; optional mean received power | No level, mean power or successful interval |
| State | Final OFF/RECEIVE/TRANSMIT state and actual readiness instant | No claim of requested readiness |
| GET | Exact attribute name and typed value | No value |
| SET | Exact attribute name; complete mutation precedes confirmation | Previous configuration remains unchanged |
| RESET | Requested default/retain choice; radio OFF | STOPPED on lifecycle interruption; reset itself is not cancellable |

GET/SET may complete inline. Modeled measurements, readiness and airtime retain their actual
event boundaries; no zero-delay message is added just to implement a procedure call. Local
parameter/status checking can complete an accepted request inline before any modeled operation.

The channel descriptor names PHY, band and channel together. M1 accepts O-QPSK/2450 MHz,
channels 11–26. Unsupported PHY/band combinations are rejected before overlapping numeric
channel values can acquire a different interpretation. Configuration mutation during a state
transition, transmission, CCA/ED or reception is refused BUSY and does not alter that operation.
An idle mutation validates the complete requested value before making it visible.

The typed PIB value carries durations as `simtime_t`, Tx power attributes and peer powers as
signed integer dBm, the implementation-dependent CCA threshold as `W`, counters/offsets as
integers, and explicit channel/CCA-mode/peer-power values. The production adapter converts
integer dBm to its radio's physical power units. It must validate attribute domains before
claiming PHY PIB support. There is no generic string-to-value fallback.
Channel availability and power limits come from the selected provider, not these value types.

Peer power lookup uses full native short/extended identity. Duplicate keys and invalid powers
reject the entire write. A destination-less transmit uses `phyTxPower`; broadcast/group
destinations use `phyBroadcastTxPower`; an ordinary destination first uses its exact peer entry,
then `phyUnicastTxPower`. These are explicit model lookup choices. The PHY owns the table and
the lookup; it does not call a concrete MAC or parse a MAC header to discover the destination.
The shared address value is the same protocol identity type already used by MAC service values.
The provider snapshots channel and selected power when accepting a scheduled transmit; another
operation cannot mutate them before its terminal result. Table 12-2's broadcast/unicast limits
and the hardware maximum still apply. Per-peer entries are not permission to exceed that maximum.

The selected O-QPSK channel changes do not change symbol rate or turnaround. CCA-duration writes
coordinate with the [MAC PIB initialization context](pib.md#initialization-and-reset):
the management owner updates the context used by a future default reset before confirming
the PHY change, while preserving the current writable `macUnitBackoffPeriod`. The production
MLME path delegates typed PHY GET/SET and performs this refresh. MLME-RESET retains PHY
configuration in the implementation-dependent PHY reset permitted by Table 8-12; default MAC
reset therefore derives its backoff default from the retained actual CCA duration. Direct PHY
reset may independently select PHY default restoration. Neither route silently leaves stale
MAC reset context behind a confirmed management change.

Reset first closes admission, cancels active work and clears owned packets, then reports RESET
for the interrupted request. Reentrant submissions during that callback see BUSY. It restores
defaults only when requested, always places the transceiver OFF, clears the barrier, and then
confirms reset. A reset is not cancellable. Stop/crash first closes admission and wins over
an in-progress reset: each surviving client receives STOPPED, never a later SUCCESS. Ordinary
terminal callbacks run after detachment and may submit another request immediately.

## Time, units and owners

All frame/measurement event fields are absolute simulation instants. Profile intervals are
durations. The selected O-QPSK inputs already recorded in the catalog and applicability audit
are 16 us per symbol, 250 kbit/s, a 127-octet PSDU limit, two MAC FCS octets, 160 us SHR and
32 us PHR, and 192 us turnaround. A complete `n`-octet PSDU therefore occupies
`(6 + n) * 32 us`. FCS belongs to `n`; it is not added a second time. These are selected-profile
inputs, not universal 802.15.4 values. The timing query reports the selected provider's snapshot.

| Boundary or timer | Owner | Meaning |
| --- | --- | --- |
| Request acceptance | PHY service provider | Ownership/admission boundary; not PPDU start or readiness |
| State transition | PHY provider | One transition and readiness confirmation; no second MAC turnaround |
| PPDU start | PHY provider | Actual start callback, with explicitly predicted remaining boundaries |
| PSDU start | PHY provider | After SHR/PHR; reported retrospectively in successful frame timing |
| PSDU/FCS completion and PPDU end | PHY provider | Equal for selected O-QPSK; transmit confirm or receive indication occurs at this instant |
| CCA/ED start/end | PHY provider | Actual observation after readiness; setup is outside the measurement interval |
| Random backoff | Channel-access provider, step 5 | One attempt's symbols and NB/BE; calls the PHY for each CCA |
| Immediate ACK target | MAC exchange owner, step 6 | Received PPDU end plus the selected AIFS; PHY must be ready at that target |
| IFS eligibility | MAC exchange owner, step 6 | Applies the audited short/long-frame and post-ACK constraints; overlapping readiness is not blindly added |
| Direct ACK deadline/retry | MAC transaction owner, step 6 | Starts from transmitted PPDU end; cancellation retires the old request/deadline |

The recorded bounded ACK model is `transmittedPpduEnd + 2 * Dmax + 192 us + 352 us`.
`Dmax` is an explicit bound on each propagation leg; 352 us includes the five-octet immediate
ACK PSDU and six PHY octets. This is a derived model contract, not an IEEE timeout constant.
The contract fixture uses zero propagation and explicitly scheduled event times. A real provider
and topology must establish their propagation/motion bound and ACK readiness in steps 4/6 before
this expression becomes their production test oracle. Unbounded propagation is not silently
treated as zero.

Completion exactly at the deadline is eligible. PHY completion events precede the timeout at
the same simulation time (normal priority 0 versus timeout priority +1 in the fixture). A
completion timestamp after the deadline is ineligible even if its callback happens to execute
before a delayed timeout handler. Stale request IDs cannot complete the current exchange.
Production timeout logic and these propagation assumptions remain unimplemented at this stage.

## Verification boundary

The [module case](../../../../../tests/module/Ieee802154PhyServiceContract_1.test) is a passing test-local client/provider
fixture for admission, ownership, field validity, timing, atomic mutation, cancellation,
reset/stop, stale IDs and deleted-client cleanup. It proves a substitutable service boundary,
not an operational PHY, CCA energy measurement, ED/LQI calibration or MAC exchanges.
Protocol tests and a protocol reviewer document do not apply to this structural contract commit;
the operational behavior and direct protocol evidence belong to steps 4–6.

The restored source PDF matches the pinned SHA-256
`851d81127297f898c956fdd1ce92929324d68cd696fd338d1e876f6a372f22d3`. The corpus was rebuilt
and linted before retrieving 11.1.3.1 (pp. 565–566), 11.2.8 (pp. 596–597), 12.1 (p. 600),
12.3.1 (p. 601), and Tables 12-2/12-3 (pp. 601–603). These establish the descriptor, CCA,
undefined SAP boundary and typed PHY PIB choices. Source spans and material reference
resolution are recorded with the execution results. Production PHY integration and the
normative C-PHY/C-ACK checks remain unexecuted.
