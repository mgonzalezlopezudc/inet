# Codebase Concerns

**Analysis Date:** 2026-06-16

## Tech Debt

**Fragile Fingerprints:**
- Issue: Fingerprint tests check exact event execution sequences. Even minor changes in unrelated NED defaults or parameter spacing can shift timing slightly and break multiple fingerprints.
- Files: `tests/fingerprint/` (multiple `.csv` configs).
- Impact: Developers face frequent test failures requiring rebuilding fingerprints after minor updates, slowing down pull requests.
- Fix approach: Prioritize using regex/subset comparison on outputs where possible, or document clean fingerprint regenerate instructions.

**Large Serializer Switch Blocks:**
- Issue: Protocol parsing classes mapping network layers to chunk decoders often contain large switch/if statements.
- Files: `src/inet/common/packet/serializer/` and `src/inet/networklayer/` handlers.
- Why: High performance requirements prevent deep abstractions, but lead to high complexity when adding or extending headers.
- Fix approach: Refactor into packet parser registry pattern.

## Memory and State Risks

**cMessage/cPacket Ownership (Dangling pointers and memory leaks):**
- Risk: OMNeT++ manages C++ object ownership. If a packet or chunk is sent or encapsulated, ownership transfers. If a module fails to properly delete or release ownership, memory leaks occur.
- Current mitigation: Valgrind sanitizer runs (`inet_valgrind` script) are used to detect leaks in CI.
- Recommendation: Enforce standard smart pointers (`std::unique_ptr`) or follow strict allocation conventions in protocol handlers.

## Fragile Areas

**Nested NED Parameter Refinement:**
- File: Deeply nested configurations inside `src/inet/node/inet/StandardHost.ned` and related routing compound modules.
- Why fragile: A parameter overridden in a child module (e.g. `host.wlan[0].mac.parameter`) can conflict with general overrides (`**.mac.parameter`) in `omnetpp.ini`. Tracing which parameter takes priority is difficult.
- Common failures: Parameter is set in config but ignored by host due to a default override at a lower level.
- Safe modification: Check parameter values at simulation startup using the `cPar::isExpression()` and default indicators.

---

*Concerns audit: 2026-06-16*
*Update as issues are fixed or new ones discovered*
