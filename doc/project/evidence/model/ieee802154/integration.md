# IEEE 802.15.4 — native interface integration

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [phy-services.md](phy-services.md), [coverage.md](coverage.md)

Packages 1d/1e add a native service adapter and selectable replacement composition. The
adapter owns only gate-to-service admission and receiver-side payload tagging. The MAC owns
identity and operational PIB state; `Ieee802154InterfaceData` publishes that identity through
`NetworkInterface`'s existing protocol-data extension. No 48-bit surrogate is assigned.

| Consumer | M1 disposition | Extension point / evidence |
| --- | --- | --- |
| Interface identity | Supported | `Ieee802154InterfaceData` on the real `NetworkInterface`; compare complete EUI-64 values |
| Upper destination request | Supported | Native `Ieee802154MacDataReq` packet tag copied into the typed MCPS call |
| Lower source indication | Supported | Native `Ieee802154MacDataInd` packet tag from the MCPS callback |
| Queue classification | Adapted | Ordinary packet queues preserve tags; classifiers must explicitly inspect the native tag |
| Neighbor/forwarding lookup | Outside M1 | No implicit mapping into generic `MacAddress`/ARP tables |
| Receive filtering | Supported in step 6 | MAC compares native identities and PAN context before service delivery |
| Display/configuration | Supported | Canonical native address parameter and interface-data `str()` |
| Generic `MacAddress` consumers | Outside M1 | No truncation, address widening or silent conversion; use a future explicit adaptation |
| Opaque payload | Supported | Native service tag, no upper protocol guessed from payload bytes or sender tags |
| Configured upper protocol | Adapted | Receiver's `upperProtocol` chooses the sole delivered protocol; mixed upper protocols are unsupported |

Pre-write contract: introduce `Ieee802154MacServiceAdapter`, native request/indication/confirm
tags and interface data under `linklayer/ieee802154`, with paired provider binding. Accepted
MSDUs transfer to the provider; refusals are consumed by the adapter with an explicit admission
tagged result to its upper gate. Inline confirmations are safe because the adapter holds no
borrowed packet or pending request after calling the provider. Request IDs never wrap or reset.
Indications take ownership immediately, discard request and sender protocol metadata, and set
the receiver's configured protocol only when present. Promiscuous and communication-status
callbacks use separate tags and never masquerade as normal data indications. Lifecycle/MCPS
completion remains owned by the provider; the adapter schedules no timers.

Proposed direct fixture: `tests/module/Ieee802154NativeInterface_1.test`. It will instantiate
real `NetworkInterface` and production adapters, use a test-local provider only for the service
boundary, and submit two addresses with equal low 48 bits through actual gates. Assertions
observe distinct attached identity, typed request and receive tags, retained bytes, inline
confirmation/refusal, and opaque versus receiver-configured dispatch. Fixed General run/seed 0.
Before the addition the native gate path and interface-data type do not exist. This package
establishes local integration, not radio delivery; steps 4–6 supply production peer exchanges.

Package 1e's explicit selection is `Ieee802154StandardInterface`, containing a substitutable
`Ieee802154StandardMac` provider and the native adapter. The existing
`Ieee802154NarrowbandInterface` remains the default until step 10's cutover gate;
`Ieee802154LegacyNarrowbandInterface` gives it an explicit migration name. UWB stays separate.
Before radio integration the replacement initializes identity and the authoritative PIB but
refuses DATA as `UNSUPPORTED_OPERATION`; this is never reported as delivered traffic.
The provider's synchronous GET/SET/RESET entry points use the existing store. Its own admission
state precedes lifecycle callbacks, IDs survive restart, and default reset draws a fresh DSN
from its configured simulation RNG. Planned fixture `Ieee802154Composition_1.test` checks this
production wiring and local outcomes. Radio and channel-access substitutions arrive in steps 4/5.
