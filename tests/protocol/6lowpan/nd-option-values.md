# Neighbor Discovery option byte values

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-completion.md](../../../plan/done/6lowpan-completion.md), [C++ enum value rules](https://eel.is/c++draft/dcl.enum#8)

This document accompanies the generic ICMPv6 representation prerequisite. It reuses the existing wire unit, ordinary ND receive fixture and native registration protocol fixture; it introduces no new registration policy or optional LoWPAN dependency in generic ICMPv6.

An ND option's type occupies one wire octet. The C++ `Ipv6NdOptionTypes` type must therefore retain every value from 0 through 255 even when the value has no named enumerator. The known enumerators currently span 1 through 8. Without a fixed underlying type, the C++ enum's value range is narrower than an octet; storing registration option 33 or unknown option values such as 99 and 255 has undefined behavior. Casting the value to `int` after loading the enum does not repair that invalid representation. For an enum with a fixed underlying type, the enum's value range is the underlying type's range. [C++ draft, dcl.enum paragraph 8](https://eel.is/c++draft/dcl.enum#8)

The MSG definition specifies `@baseType(uint8_t)`. Normal message compilation generates `enum Ipv6NdOptionTypes : uint8_t`; getter/setter types and named option values stay unchanged. Unknown values continue through the existing raw-option serializer branch. Known options retain their existing dynamic classes and checked casts. Representing a byte is separate from accepting its option semantics: this change does not make reserved type zero a valid option or relax length validation.

`Ipv6NdRegistrationWire.test` checks the underlying type and enum-typed getter at compile time, then stores and reads all 256 byte values. It constructs independent raw RS options 99 and 255, compares their serialized bytes, decodes their type/body and changes the checksum before reserialization to bypass the decoded-byte cache. Existing vectors retain ARO 33, native SLLAO/TLLAO, unknown option 222, and the typed six-octet Ethernet option. Known option types are not serialized through an incompatible raw class solely to fill the numeric range.

The independent C++ range probe includes the actual normally generated header and passes all 256 values under enum sanitization, ordinary optimization and strict enum optimization. It uses explicit condition/exit checks. Earlier diagnostic evidence found UBSan failures for old values 33, 99 and 255; a strict-enum probe folded the old option-33 comparison to false. The ordinary optimized old probe did not show a failure. Those diagnostics establish the representation defect and its optimization risk; they do not establish a failing old full-simulation release run.

The following selections passed after fresh matching debug and release builds:

```sh
opp_run_unit_tests --load . -p inet -m debug --no-build \
  -f 'Ipv6NdRegistrationWire\.test$'
opp_run_module_tests --load . -p inet -m debug --no-build \
  -f 'Ipv6NdUnsupportedRegistration\.test$'
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/6lowpan$' -f 'LowpanNeighborRegistration\.test$'
opp_run_protocol_tests --load . -p inet -m release --no-build \
  -w '^tests/protocol/6lowpan$' -f 'LowpanNeighborRegistration\.test$'
```

The unit and module selections each contain one fixture. The native fixture sends literal NS/ARO input through MAC and IPv6 into the production ND consumer. The debug run checks real radio reply fields, learned data routes, ownership, expiry and lifecycle, including the ordinary unicast/multicast/DAD controls.

Release compilation defines `NDEBUG`, disabling the fixture's OMNeT++ `ASSERT` checks. Its runner PASS alone is therefore not semantic evidence. Independent post-run checks in both modes require the printed counts: 21 injected frames, eleven ARO advertisements, one learned-route data packet, six malformed drops and registration-count sequence `1,0,1,0,1,0,1,0,1`. The release run met those counts, establishing the optimized option-33 consumer's observed registration actions. It does not establish the ordinary NS/DAD field assertions in release mode. The range probe's explicit checks remain active under optimization. These checks do not establish new ND registration policy, a fully sanitized library or all optional feature combinations.
