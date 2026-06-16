# Technology Stack

**Analysis Date:** 2026-06-16

## Languages

**Primary:**
- C++17 - Main simulation models, protocol logic, packet serializers, and application implementation.

**Secondary:**
- NED (Network Description) - Network topology configurations, module structures, parameter definitions, and gates connectivity.
- MSG (OMNeT++ Message definitions) - Packet structures, header definitions, and event messages (generates C++ source/headers using `opp_msgc`).
- Python 3 - Testing framework runners (`python/inet/test/`), analysis scripts, and build helper scripts.
- XML - Network configuration, routing table structures, and ScenarioManager script files.

## Runtime

**Environment:**
- OMNeT++ 6.3.0 Simulation Engine (runs simulations via `opp_run` or `inet_dbg` / `inet` binaries).
- Python 3.x for scripting and running test suites.

**Package Manager:**
- None (native OMNeT++ build system and submodules). Build output is a shared library (`libINET.so`) or standalone executable.

## Frameworks

**Core:**
- OMNeT++ Simulation Library (`sim_std` including `cSimpleModule`, `cMessage`, `cPacket`, `cSimulation`).

**Testing:**
- `opp_test` - Standard OMNeT++ unit testing tool for compiling and executing `.test` files.
- INET Test Framework - Custom Python test runner suite (`python/inet/test/`) executing smoketests, validation tests, fingerprint tests, and unit tests.

**Build/Dev:**
- GNU Make - Build management.
- `opp_makemake` - OMNeT++ utility to generate Makefiles matching project configuration files.
- `opp_featuretool` - Command-line utility to manage optional INET features (`.oppfeatures`, `.oppfeaturestate`).

## Key Dependencies

**Critical:**
- OMNeT++ simulation libraries (core classes, RNGs, logging, result manager).
- LibXML2 - For parsing XML network configuration and scenario scripts.

**Infrastructure:**
- Python standard library for script execution and result parsing.

## Configuration

**Environment:**
- Sourced configuration scripts (`setenv` in repository root). Sets environment variables: `INET_ROOT`, `INET_NED_PATH`, and paths to OMNeT++ binaries.

**Build:**
- `.oppbuildspec` - IDE/CLI build options for `opp_makemake`.
- `.oppfeatures` / `.oppfeaturestate` - Lists available features and their enablement states.
- `Makefile` - Project-level Makefile.

**Simulation:**
- `omnetpp.ini` - Simulation runtime settings, parameter overrides, module typenames, seeds, and execution limits.

## Platform Requirements

**Development:**
- Linux (Debian/Ubuntu/CentOS), macOS, or Windows (with MSYS2 toolchain supplied by OMNeT++).
- OMNeT++ 6.3.0 SDK must be installed and sourced.

**Production:**
- Built as shared library `libINET.so` (Linux) / `libINET.dylib` (macOS) / `INET.dll` (Windows) for linking with simulation targets.

---

*Stack analysis: 2026-06-16*
*Update after major dependency changes*
