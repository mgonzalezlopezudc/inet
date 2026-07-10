---
name: inet-packet-tag-debugging
description: Debug INET Packet, Chunk, and tag behavior. Use when Codex needs to inspect packet ownership, encapsulation/decapsulation, protocol dispatch, request/indication tags, region tags, header chunks, metadata propagation, duplication, popping/peeking, or why packet metadata is missing or changed across INET modules, including MAC/PHY and IEEE 802.11 paths.
---

# INET packet, chunk, and tag debugging

## Core rule

Distinguish packet data from packet metadata. A packet may carry the expected bytes while missing the tag, protocol marker, region tag, or header representation needed by the next module.

## Workflow

1. Identify the packet name/class and the module path where metadata is first correct and first incorrect.
2. Inspect the source code that adds, removes, copies, peeks, pops, inserts, trims, encapsulates, or decapsulates the relevant chunk/tag.
3. Search for the exact tag or chunk type in the checked-out source; do not rely on memory of another INET version.
4. Use targeted Cmdenv logs or LLDB breakpoints around the first module that changes the packet.
5. Verify whether the packet was duplicated or shared before modification.
6. Check whether code expects a front header, region tag, protocol tag, request tag, indication tag, or packet protocol field.
7. For protocol-visible consequences, confirm with `inet-pcap-tshark-analysis`; for 802.11 paths, use `inet-80211-packet-debugging` as well.

## Useful searches

```sh
rg -n 'addTag|addTagIfAbsent|findTag|hasTag|getTag|removeTag|clearTags' src
rg -n 'peek|pop|insertAtFront|insertAtBack|trim|encapsulate|decapsulate|dup\(' src
rg -n 'ProtocolTag|DispatchProtocolReq|PacketProtocolTag|InterfaceReq|MacAddressReq|SignalPower|Snir|ErrorRate' src
rg -n 'class .*Tag|class .*Header|Register_Class|FieldsChunk|Chunk' src/inet
```

Use narrower paths after locating the relevant protocol or module.

## INET-specific traps

* `peek`-style operations should not consume data; `pop`/`trim` operations change packet contents.
* Verify tag and region-tag preservation across duplication, encapsulation, decapsulation, fragmentation, aggregation, and protocol conversion; do not assume all transformations preserve the same metadata.
* Request tags and indication tags usually mean different directions of information flow.
* Region tags can apply to byte ranges; checking only packet-level tags may miss them.
* Shared chunks or packets can make ownership/lifetime assumptions wrong.
* A packet captured in PCAP may not include all internal tags that drove module behavior.
* Debugger expressions that call packet methods may execute code; prefer `frame variable` first and inspect methods cautiously.

Report the packet identity, first module and source location where data or metadata diverged, inspected tag/chunk fields, supporting evidence, and the resulting failure category.
