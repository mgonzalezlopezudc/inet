# IEEE 802.11 limitation register

This register replaces the former `src/inet/linklayer/ieee80211/__TODO`
scratchpad with scoped, actionable limitations.

## Configuration and operation

- Mode-provider diagnostics should identify the missing IEEE 802.11 mode contract
  when a generic radio is paired with the detailed MAC.
- Mixed legacy/ERP response-rate behavior remains unsupported; probe responses
  require a mutually receivable operational rate path.
- HCCA is not implemented. HCF rejects an HCCA-owned exchange explicitly at the
  exchange boundary; configurations requiring HCCA must not treat the submodule
  as operational.

## Observability

- IEEE 802.11 MAC, contention, coordination, rate, TXOP, recovery, and Block Ack
  statistics remain a follow-up surface. New protocol decisions must use typed
  contracts; observational signals remain reporting-only.

## Verification boundary

- Association and reassociation ACK, retry-limit, stale-result, and cancellation
  cases are covered by the typed contract unit test, while the current module
  regressions exercise real DCF/HCF data exchanges. A full packet-level management
  simulation that deliberately loses Association Response ACKs through both DCF
  and HCF remains follow-up coverage.

## Model completeness

- Optional management information elements not represented by the message model
  remain unsupported and must not be fabricated by serializers.
- HCF TODOs for additional frame variants, rate-policy details, and non-QoS
  aggregation are retained at their owning call sites until focused behavior and
  regression coverage exist.
- Default modulation values should be changed only after confirming whether the
  mode pipeline or the NED parameter is authoritative.
