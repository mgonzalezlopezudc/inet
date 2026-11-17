# Selected MAC PIB mutation contract

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [applicability.md](applicability.md), [catalog.md](../../standard/ieee802154/catalog.md#pib-field-domains)

Package 1b defines the store used by `Ieee802154StandardMac`. The store owns validated values;
the operational provider owns admission, request IDs,
active operations, notifications and application of changes to radio/access procedures. PHY PIB
attributes remain with the PHY owner and are delegated through typed MLME GET/SET.

## Representation and attribute selection

The API reuses `Ieee802154MacSetRequest` for startup overrides and
`Ieee802154MacGetConfirm` for GET results, echoing the attribute name. The store produces only
IEEE status alternatives; local admission and abort outcomes belong to the provider.

The selected set contains the original 43 attributes plus nine managed-service attributes
identified in the [managed applicability audit](https://github.com/mgonzalezlopezudc/inet/blob/ec1ee66b1b/doc/project/evidence/model/ieee802154/managed-applicability.md), for 52 total.
GET preserves the exact requested name. Integer values use the
`uint64_t` variant; SET accepts that representation or nonnegative `int64_t`, never an implicit
Boolean conversion. Boolean values require the Boolean variant. Extended addresses use the
native address variant. Short addresses and PAN identifiers remain integer PIB values.
Beacon payload uses a byte-vector variant, without a string encoding.

| Attributes | Access and accepted values | Default/reset value |
| --- | --- | --- |
| macExtendedAddress | Upper-layer read-only; construction requires an individual extended address | Supplied device identity, retained across reset |
| macCoordExtendedAddress | Writable individual extended address | Native NONE denotes unspecified coordinator context; this local sentinel is not an address writable through SET |
| macShortAddress, macCoordShortAddress, macPanId | Writable integers 0–65535, including sentinel values | 65535 |
| macDsn | Writable integer 0–255 | Owner-supplied random draw at construction and each default reset |
| macBsn | Writable integer 0–255 | Separately injected random draw when the managed capability is enabled; deterministic helper injection otherwise |
| macAutoRequest, macBeaconAutoRespond | Writable Boolean | true |
| macNotifyAllBeacons, macAssociatedPanCoord, macAssociationPermit | Writable Boolean | false |
| macResponseWaitTime | Writable integer 2–64, base-superframe periods | 32 |
| macTransactionPersistenceTime | Writable integer 0–65535; BO=15 unit is 960 O-QPSK symbols | 500 (7.68 s) |
| macBeaconPayload | Writable opaque byte vector; transmission validates resulting MPDU length | Empty |
| macMinBe, macMaxBe | Writable integers 0–current maximum and 3–8; both must remain consistent | 3, 5 |
| macMaxCsmaBackoffs, macMaxFrameRetries | Writable integers 0–5 and 0–7 | 4, 3 |
| macSifsPeriod, macLifsPeriod | Upper-layer read-only, in symbols | 12, 40 for selected O-QPSK |
| macUnitBackoffPeriod | Writable positive integer symbols, locally representable through UINT32_MAX | 12 + ceil(CCA duration in microseconds / 16) |
| macRxOnWhenIdle, macImplicitBroadcast, macGroupRxMode, macPromiscuousMode | Writable Boolean | false |
| macSecurityEnabled | Writable, selected profile accepts false only | false |
| macBeaconOrder | Writable, selected profile accepts 15 only | 15 |
| macTimestampSupported | Upper-layer read-only | false |
| macSyncSymbolOffset | Upper-layer read-only, symbols | Value 0: beacon descriptor timestamps use the first symbol past SFD; DATA timestamps remain absent |
| Table 8-37 capability flags | Upper-layer read-only | false for this implementation |
| Table 8-37 enabled flags, macLeHsEnabled, macTrleRelayingMode | Writable, selected profile accepts false only | false |

GET of unspecified coordinator context returns SUCCESS with native NONE as a local API
representation, not an IEEE-defined extended-address value or sentinel.

The source leaves several defaults unspecified. NONE, zero sync offset, and false capability
values above are declared implementation choices, not IEEE defaults. The backoff upper bound is
a local representation limit, not a range supplied by Table 8-36. SIFS/LIFS values follow clause
11.1.4, physical PDF page 593, `ieee802154-2024@2363122:2364053`.

Unselected base attributes, ranging state and security tables
are absent from this store. Their requests return UNSUPPORTED_ATTRIBUTE. The service facade
additionally represents typed PHY durations, powers, channel/mode and peer tables; the PHY owns
their validation and readonly precedence, rather than duplicating those rules in the MAC store.
This does not resolve their deferred device-profile obligations or claim them inapplicable.

## Initialization and reset

Construction receives the immutable extended identity, separately injected DSN/BSN defaults, and the selected
PHY's CCA duration in integer microseconds (1–1000000, default 128). O-QPSK uses 16 microseconds
per symbol and 12 turnaround symbols. Integer ceiling precedes the addition. CCA duration is
reset context, not a duplicate PHY PIB. After a successful delegated PHY duration write,
the management owner calls `updateCcaDuration()` before confirming the change. That refresh
validates the context and preserves the current writable backoff value. An explicit backoff
write is not silently recomputed during another MAC or PHY write.

Startup overrides use the same value/access checks as SET. They are validated as a complete
candidate, including the final BE pair, independent of override order. Duplicate names and
invalid candidates fail construction with a configuration error. NED owners read their
parameters once into these inputs; no second runtime parameter copy is authoritative.

Reset with SetDefaultPib=false retains every stored value (Table 8-12, physical PDF page 123,
`ieee802154-2024@530664:531472`). Reset with true restores the table's
standard/profile defaults, using fresh owner-supplied sequence values; it does not replay startup overrides.
Device identity and the current validated PHY context remain supplied defaults. MLME reset retains
PHY configuration under Table 8-12's implementation-dependent PHY-reset rule; default MAC reset
therefore uses its current CCA duration. The owner controls RNG stream
and draw timing; the store never draws randomness. PHY reset and aborting outstanding requests
are provider responsibilities, outside this store's reset operation.

## Mutation, failure and observability

All writable selected attributes can change at the store boundary after initialization. There
is no public batch SET. The dedicated `clearAssociation()` operation restores the five
PAN/coordinator references required by 10.21.3, preserving other values. Each SET first checks the
exact name, then read-only access, then type/domain/profile restrictions, then cross-attribute
invariants. These precedence rules for simultaneous errors are local API choices. Failure retains
all values. A successful same-value SET is still SUCCESS.

The store is synchronous and emits no signals or callbacks. The production provider admits
changes, publishes the native interface identity, and confirms after the owning MAC or PHY
has applied a mutation. Direct store tests establish validation/default behavior. Production
PHY-delegation and lifecycle fixtures separately establish busy handling, actual radio timing
effects and reset/stop barriers; their results must not be inferred from the unit case.

Validation and independent review are recorded in [results.md](results.md#step-1b-selected-mac-pib).
