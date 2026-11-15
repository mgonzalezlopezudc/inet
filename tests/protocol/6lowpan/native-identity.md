# Native reassembly identity isolation

> **Kind:** report · **Status:** snapshot 2026-11-15 · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-implementation-plan.md](../../../plan/pending/6lowpan-implementation-plan.md), [data-plane.md](../../../doc/project/evidence/protocol/6lowpan/checks/data-plane.md)

The P7 identity row needs evidence that identical tag/size values cannot mix native datagrams from different senders, receiving interfaces/PANs or destinations. `LowpanNativeIdentityIsolation.test` adds that coverage to the existing production implementation. Its planned commit is `tests: add: native 6LoWPAN reassembly identity isolation`. The parent already has the correct behavior; this is a control, not a bug repair.

Five stationary native interfaces occupy two dedicated scalar radio media. PANs `0x1234` and `0x5678` deliberately reuse the same sender and receiver EUI-64 values. A third peer in the first PAN supplies a distinct source. The driver constructs literal native data frames and injects them at the receiving MAC gate. Real MAC decoding creates the indications consumed by the native boundary and adaptation module. No TX tags or sender chunk structure accompany the input. IPv6 output is captured at the receiving interface rather than passed through a mock decompressor.

Each scenario interleaves two 80-byte datagrams with tag `0x1234`, beginning with their final ranges. The IPv6 headers agree but the payload bytes differ. The first pair uses distinct receiving interfaces/PANs with identical EUI values; the second pair uses distinct sources on one interface; the third uses extended-unicast versus short-broadcast destinations on one interface. Fresh first ranges must complete the correct payload at 3/4, 12/13 and 22/23 ms respectively. The output gate index identifies the receiving interface. Mixed ranges cause byte, time, drop or completion-count assertions to fail.

The run uses seed 0 and ends at 50 ms. It requires all 12 injected fragments to reach adaptation, exactly six completed and delivered datagrams, no drop and no expiry. The paired PAN/interface case proves external isolation across per-interface owners; it does not pretend to distinguish the contribution of each internal key field in an already separate table. Injection bypasses propagation and channel access; the native sender boundary sweep supplies separate radio-path evidence.

```sh
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/6lowpan$' -f 'LowpanNativeIdentityIsolation\.test$'
```

Run from the INET root with a matching debug library. Exact source/fixture hashes and results are in the [closure record](../../../doc/project/evidence/model/6lowpan/p7-closure.json), with the combined campaign explained in [release-validation.md](../../../doc/project/evidence/model/6lowpan/release-validation.md). Initial setup attempts bound the second PAN's interfaces to the first domain: the NED assignment was fixed rather than a default, so the INI override could not take effect. Making the default overridable and using explicit node assignments fixes the fixture; `identity-domain-setup.err` preserves the setup error. No production change or expected-failure declaration is used.
