# Stack Research

**Domain:** Wireless Simulation / IEEE 802.11ax
**Researched:** 2026-06-16
**Confidence:** HIGH

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| OMNeT++ | 6.3.0 | Discrete event simulation engine | Native framework, provides event loop, scheduling, and standard libraries. |
| C++ | C++17 | Protocol logic and mathematical models | High performance, strict type safety, standard in OMNeT++ 6.x. |
| NED | - | Network description topology | Declarative syntax for connecting modules and setting parameters. |
| MSG | - | Message/packet definitions | Automatically generates C++ serialization/deserialization classes. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| INET Framework | 4.x | Networking protocols library | Base library for L2 MAC and PHY layer wireless simulations. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| opp_makemake | Makefile generation | Generates platform-specific Makefiles. |
| opp_run / inet_dbg | Simulation execution | Native execution binary with debugging capabilities. |
| opp_test | Unit/Integration testing | Compiles and executes `.test` files in unit testing. |

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| OMNeT++ 6.3.0 | ns-3 | If simulating larger-scale non-INET protocol suites or when C++ scripting is preferred over NED. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| raw dynamic_cast | Can crash simulation with segmentation fault on check failure | `check_and_cast<T*>()` for safe casting with runtime errors |
| std::cout | Bypasses OMNeT++ logging filters and Qtenv GUI log view | `EV` logging streams (e.g. `EV_INFO`, `EV_WARN`) |

## Stack Patterns by Variant

**If debugging protocol sequence:**
- Use Qtenv (graphical interface)
- Because it allows step-by-step event debugging and inspection of packet fields.

**If executing test suites/regression run:**
- Use Cmdenv (command-line execution)
- Because it has no graphical overhead and runs significantly faster.

## Version Compatibility

| Package A | Compatible With | Notes |
|-----------|-----------------|-------|
| OMNeT++ 6.3.0 | gcc/clang with C++17 support | Required for building the INET shared library. |

## Sources

- [IEEE 802.11ax-2020 Standard] — Standard specifications for HE timing, MU PPDU structure, and BlockAck procedures.
- [INET Framework Developer Guide] — Guidelines on module design, packet tagging, and event flow.

---
*Stack research for: 802.11ax DL MU OFDMA correctness*
*Researched: 2026-06-16*
