# IEEE 802.15.4 legacy receive metadata contract

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [coverage.md](coverage.md)

- Invariant and owner: `Ieee802154Mac::decapsulate()` reports both addresses from the
  removed legacy MAC header, preserving current interface and protocol indications.
- Entry/control path: lower gate → `handleLowerPacket()` filtering → receive FSM →
  `decapsulate()` → `sendUp()`. Both unicast and broadcast traverse this method.
- Changed production artifact: `src/inet/linklayer/ieee802154/Ieee802154Mac.cc` only.
  Consumer artifact: `tests/module/Ieee802154ReceiveMetadata.test`. No generated inputs,
  core packet files or sealed source paths change.
- Terminal/sibling paths: erroneous frames, other destinations, ACKs and duplicate
  suppression do not change. The same packet remains MAC-owned until `sendUp()` transfers
  ownership; adding a destination field allocates no independent packet/timer state.
- Boundaries: legacy 48-bit `MacAddress`, including broadcast; no native EUI-64 claim.
  Initialization stage count, lifecycle, timing, frame bytes and length do not change.
- Direct verification: `make MODE=debug -j8`, then
  `inet_run_module_tests -m debug -f 'Ieee802154ReceiveMetadata\.test$'`.
  The fixture injects tag-free frames at the real lower gate and checks both upper
  indications, interface, protocol and unchanged payload; no mock decapsulation.
- Regression selection: existing `tests/fingerprint/examples.csv` configurations
  `CSMAWithUnitDiskRadio`, `CSMAWithApskScalarRadio`, `CSMAWithApskDimensionalRadio`.

Pre-write self-validation: owner, both receive branches and `MacProtocolBase` gates
inspected. No IEEE 802.15.4-specific exception ledger entry or additional domain rule
was found. The test runner and selected fixture exist. Runtime verdict remains open.
