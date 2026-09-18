# IEEE 802.15.4 — survey coverage

> **Kind:** ledger · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [conformance.md](conformance.md), [derive-tests-from-a-standard.md](../../../guide/derive-tests-from-a-standard.md)

Level reached: **1 (Survey)** for the model families named in the claim survey. No Level 2
or higher check has been authored or run in this pass. No protocol conformance verdict is
recorded; successful corpus retrieval validates tooling, not the simulation model.

| Area | Survey evidence | Next evidence needed |
| --- | --- | --- |
| Edition and family | Pinned 2024 base; later amendments explicitly excluded | Older 2006/2007 texts for exact comparison when a change relies on an older claim |
| MAC access and ACK/retry behavior | Claim sources mapped to 6.3 and 6.6 | Specification-derived catalog, English checks, then production-path exchanges |
| Frame representation | Serializer limitation identified; mapped to clause 7 | Independent header/serialization checks and captured frame evidence |
| Narrowband PHY | Claim sources mapped to 11–13; sensitivity approximation identified | Parameter applicability, duration checks, and appropriately scoped reception/error tests |
| UWB PHY and interface | Historical PHY claim mapped to 16; distinct MAC composition identified | Mode-specific comparison and PHY evidence; separate MAC claim selection |

Statement IDs, `owed` rows, test paths, and run verdicts enter this ledger when the next pass
extracts the catalog and selects checks. A simulation test is not replaced by this survey.
