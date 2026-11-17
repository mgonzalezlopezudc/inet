# Managed non-beacon PAN applicability

> **Kind:** report · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [applicability.md](applicability.md), [indirect-transactions.md](indirect-transactions.md)

This extends the M1 audit for steps 7–9. It records source-derived requirements and
implementation decisions; it is not a completed M2 gate or executed protocol evidence.
The source is IEEE Std 802.15.4-2024, PDF SHA-256
`851d81127297f898c956fdd1ce92929324d68cd696fd338d1e876f6a372f22d3`.
The following clause, table and figure nodes were retrieved from the local corpus on
2026-11-15, including figure continuations containing normative prose.

## Selected roles and mandatory service inventory

| Procedure | Source nodes | Selected requirement |
| --- | --- | --- |
| Non-beacon PAN start | 6.4.2.2, 8.2.9.2, Tables 8-20/21 | START takes PAN and channel information. Short address 0xffff returns NO_SHORT_ADDRESS. Higher-layer PAN/channel selection follows reset, ED and active scans; the selection algorithm stays outside MAC. |
| Passive scan | 6.4.1.1/2, Figure 6-3, Tables 8-18/19 and 8-3 | Mandatory for all devices. Ascending channels; only beacons received; never poll pending addresses during scan. Deduplicate by channel, PAN and full source address. AutoRequest controls retained list versus notify indications; payload-bearing beacons also notify. Stop at capacity only with AutoRequest true. |
| Active scan | 10.19, Figure 10-147, Tables 8-18/19 | Send Beacon Request on each channel, then receive beacons. Non-beacon coordinator answers with one beacon through unslotted access. Record access-failed channels as unscanned; no found beacons gives NO_BEACON when at least one request was sent. |
| ED scan | 10.18, Tables 8-18/19 | Repeated ED measurements per channel; report maximum, discard received frames. This is different from a single ED average. |
| Scan duration | Table 8-18 and its continuation | ScanDuration is a uint32 number of symbol periods, not the older exponent encoding. Orphan ignores this field. Preserve that unit in the typed API and tests. |
| Orphan recovery | 10.20.2.1, 10.20.3.1, 10.20.4.2/3, Table 10-100, 7.5.4 and Figure 7-30 | Send Orphan Notification per channel, receive only realignment for at most macResponseWaitTime, stop on valid response. Coordinator reports orphan to its higher layer; that layer recognizes membership and supplies the assigned address. |
| PAN conflict | 10.17.2/3/4.1 | Detect conflicting PAN-coordinator beacons under the role predicates. Associated device notifies its coordinator and reports conflict only after ACK. Coordinator reports directly. PAN reselection is higher-layer policy. |
| Association | 10.21.2, 10.21.5.1/2, Tables 10-101/102/104–107 | Request uses extended source and broadcast source PAN. AssociationPermit false ignores request. ACK is not association success. Higher layer decides admission and unique address allocation. Response is indirect, extended at both ends. 0xfffe means associated using extended identity; rejection carries 0xffff. |
| Disassociation | 10.21.3, 10.21.5.3, Tables 10-103/108–110 | Device or coordinator initiation; coordinator may use indirect delivery. Validate sender membership/coordinator identity. Device cleanup restores PAN, short address, association flag and coordinator identity defaults. Higher-layer coordinator removes member references. |
| Indirect data and commands | 10.22.2/3, 6.6.3.4 | One bounded shared store, FIFO per device, expiry, extraction success only after requested ACK. Failed indirect attempts wait for a new poll with unchanged DSN. |
| Poll | 10.22.4.1, Tables 10-112/113 | AR=1; address mode follows local short-address allocation. ACK FP=0 immediately gives NO_DATA; FP=1 enables RX. Empty DATA, MAC command, or expected-time failure gives poll NO_DATA. Nonempty DATA gives SUCCESS plus MCPS indication. Association response processing remains separate from poll status. |
| Common command capability | 7.5.1 and Table 7-11 | All devices must transmit/receive Beacon Request and Coordinator Realignment. Selected roles also require command IDs 1–6. Enhanced requests, GTS and other option-specific commands remain outside this selection. |
| Discovery beacons | 7.3.1.2/4/5/6/7, Figures 7-7/9/12/13 | Legacy source-only MHR and separate BSN. Superframe/GTS/pending-address fields are structurally bounded. At most seven pending addresses, short before extended, no broadcast short. Decoding a periodic beacon during scan does not enable periodic beacon operation. |
| Explicit beacon generation | 8.2.1/Table 8-1, 8.2.10.1–4/Tables 8-22–25 | BEACON.request/confirm are general services. Generate selected legacy beacons on demand. Unsupported enhanced-beacon requests return INVALID_PARAMETER. AutoRespond false reports Beacon Request to the higher layer. BEACON.indication is optional for non-ERDEV. |

## Decisions and unresolved extraction

The pending store uses expiry equality as expired, independent of event insertion order.
Poll expected-time bounds must be an explicit model contract derived from the selected
response access policy and configured propagation bound; no invented IEEE timeout constant
is admitted. Use CSMA-CA for the coordinator response unless every predicate for the optional
immediate response mechanism in 10.22.3 is established.

Clause 10.21.2 says both to wait macResponseWaitTime before non-beacon extraction and to fail
without a response within macResponseWaitTime. The rendered PDF p. 359, Figure 10-152
(printed p. 358), was inspected directly: it places the wait after Association Request
and before Data Request, with Association Response after Data Request. Table 8-36 defines
the interval as time for the response command to become available. The selected interpretation
therefore waits before polling, then applies the poll's expected response interval. It never
fails at the same instant it first initiates extraction. This is an explicit reconciliation
of the prose with the figure and attribute definition, not a separate IEEE timer constant.

The general transaction Frame Pending requirement and the command-specific instruction to
transmit Frame Pending zero need explicit precedence for command transactions. DATA can
advertise additional pending transactions; Association Response must retain its command
format. Further polling remains controlled by higher-layer policy.

Receiver-enable and beacon-notify primitives were additionally retrieved: 8.2.7.2–5,
Tables 8-14–17, 8.2.4.3/Table 8-4, and timestamp clause 6.5.3. RX-ENABLE accepts a list
(implementation maximum may be one), 24-bit symbol timestamps, immediate time zero,
disable duration zero, indefinite duration 0xffffff, AutoOff, replacement and counter-half-period
late handling. Finite expiry indications have their own timestamps. This is not the older
single-duration/defer-only primitive. Discovery descriptors retain beacon payload, quality,
timestamp and security failure classification as specified; no plaintext claim follows from
an unsuccessful unsecuring operation.

Table 8-36 gives ResponseWaitTime range 2–64 base-superframe durations, default 32,
AutoRequest default true and a separately randomized BSN. TransactionPersistenceTime is
0–65535 unit periods, default 0x01f4. For BO=15 the unit is aBaseSuperframeDuration
(960 selected O-QPSK symbols), so the default lifetime is 7.68 s. The PDF wraps the
name as `macTransactionPer-` followed by `sistenceTime`; searching the intact identifier
alone misses this definition. No separate model lifetime parameter or borrowed
older-revision default is needed.

Scan restore/failure semantics and selected-profile obligation coverage still need closure
before the M2 operational implementation gate closes. Crypto, enhanced frames, periodic
beacons, fast association and suspendable CSMA-CA are not enabled by these shared codecs.

## Beacon codec implementation contract

`Ieee802154BeaconCodec` owns only the content layout. It bounds GTS descriptor and
pending-address counts before reads, preserves opaque beacon payload, ignores received
reserved bits and rejects broadcast short pending addresses. Full-frame serialization
uses the existing native address/CRC path with a source-only legacy MHR. The M1 receive
path continues to discard these frames after optional monitor delivery. The proposed
`Ieee802154Beacons_1.test` checks independent octets, both short and extended pending
identities, GTS offsets, every truncated prefix and complete registered frame conversion.
No scan, periodic transmission or GTS operation is inferred from codec coverage.

## Non-beacon START implementation contract

Validated against the production MAC/PHY ownership paths before implementation:
the optional management provider accepts PAN and typed channel requests (Tables
8-20/21). The MAC owns the coordinator role; its NED value initializes that role
and default RESET restores that initial policy. Retain RESET preserves it.
START requires a preconfigured short address other than 0xffff. It first sets
the channel through the sole PHY provider, then commits PAN/role and publishes
the native interface identity only on success. Failed channel validation leaves
PAN and role unchanged. Admission is BUSY with active work or queued indirect
transactions, so queued frames cannot change PAN beneath their owners.
The accepted request is detached before its exactly-one confirmation. RESET
and stop retire it with their local abort reason; its atomic channel update is
not cancellable. Stale PHY confirmations are ignored by operation ID/purpose.
No receive-idle, association, or beacon policy is changed by START.
A proposed focused production module test will verify success, role activation
through indirect admission, NO_SHORT_ADDRESS, invalid-channel atomicity,
interface publication and retain/default RESET behavior.

## Managed PIB implementation contract

Extend the same authoritative `Ieee802154MacPib`, without a second management copy,
with nine selected attributes: BSN, AutoRequest, NotifyAllBeacons, BeaconAutoRespond,
AssociatedPanCoord, AssociationPermit, ResponseWaitTime, TransactionPersistenceTime and
BeaconPayload. Sources are Tables 8-36, 8-38 and 10-111. Boolean types are exact;
response wait is 2–64 base-superframe periods, persistence is 0–65535 periods and BSN
is one octet. Payload is opaque bytes; oversize transmission is a frame-generation
failure rather than an invented PIB range. Invalid mutations remain atomic.

The store takes separately injected DSN and BSN values; the operational owner draws
the randomized initial/reset defaults when the managed capability is enabled. Neither
the helper's deterministic injection nor enabling the attribute proves beacon operation.
`clearAssociation()` implements only the five-field cleanup specified by 10.21.3 and
retains unrelated PIB values. Existing direct-data consumers retain their request API.
Both MAC/PHY value variants admit byte vectors so invalid typed MLME values still reach
the authoritative provider's unknown/read-only/type checks. No PHY byte-vector attribute
is introduced. The expanded `Ieee802154MacPib_1.test` will check all 52 attributes,
managed bounds, byte payload, retain/default reset and selective association cleanup.
