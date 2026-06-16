<!-- GSD:project-start source:PROJECT.md -->

## Project

**802.11ax DL MU OFDMA Correctness**

This project focuses on verifying and ensuring the functional and physical correctness of the Downlink Multi-User Orthogonal Frequency Division Multiple Access (DL MU OFDMA) implementation in the INET Framework. It validates that standard-compliant IEEE 802.11ax frame sequences, block acknowledgment timings, and physical layer Resource Unit (RU) signal reception/noise calculations are accurately executed and collision-free.

**Core Value:** Ensure high-fidelity, standard-compliant packet-level simulation of 802.11ax DL MU OFDMA scheduling, transmission, and reception by verifying both protocol state machines and physical sub-channel behavior.

### Constraints

- **Language**: C++17, NED, and MSG message definitions.
- **Environment**: OMNeT++ 6.3.0 simulation engine.
- **Safety**: Safe casting must use `check_and_cast<T*>()`.

<!-- GSD:project-end -->

<!-- GSD:stack-start source:codebase/STACK.md -->

## Technology Stack

## Languages

- C++17 - Main simulation models, protocol logic, packet serializers, and application implementation.
- NED (Network Description) - Network topology configurations, module structures, parameter definitions, and gates connectivity.
- MSG (OMNeT++ Message definitions) - Packet structures, header definitions, and event messages (generates C++ source/headers using `opp_msgc`).
- Python 3 - Testing framework runners (`python/inet/test/`), analysis scripts, and build helper scripts.
- XML - Network configuration, routing table structures, and ScenarioManager script files.

## Runtime

- OMNeT++ 6.3.0 Simulation Engine (runs simulations via `opp_run` or `inet_dbg` / `inet` binaries).
- Python 3.x for scripting and running test suites.
- None (native OMNeT++ build system and submodules). Build output is a shared library (`libINET.so`) or standalone executable.

## Frameworks

- OMNeT++ Simulation Library (`sim_std` including `cSimpleModule`, `cMessage`, `cPacket`, `cSimulation`).
- `opp_test` - Standard OMNeT++ unit testing tool for compiling and executing `.test` files.
- INET Test Framework - Custom Python test runner suite (`python/inet/test/`) executing smoketests, validation tests, fingerprint tests, and unit tests.
- GNU Make - Build management.
- `opp_makemake` - OMNeT++ utility to generate Makefiles matching project configuration files.
- `opp_featuretool` - Command-line utility to manage optional INET features (`.oppfeatures`, `.oppfeaturestate`).

## Key Dependencies

- OMNeT++ simulation libraries (core classes, RNGs, logging, result manager).
- LibXML2 - For parsing XML network configuration and scenario scripts.
- Python standard library for script execution and result parsing.

## Configuration

- Sourced configuration scripts (`setenv` in repository root). Sets environment variables: `INET_ROOT`, `INET_NED_PATH`, and paths to OMNeT++ binaries.
- `.oppbuildspec` - IDE/CLI build options for `opp_makemake`.
- `.oppfeatures` / `.oppfeaturestate` - Lists available features and their enablement states.
- `Makefile` - Project-level Makefile.
- `omnetpp.ini` - Simulation runtime settings, parameter overrides, module typenames, seeds, and execution limits.

## Platform Requirements

- Linux (Debian/Ubuntu/CentOS), macOS, or Windows (with MSYS2 toolchain supplied by OMNeT++).
- OMNeT++ 6.3.0 SDK must be installed and sourced.
- Built as shared library `libINET.so` (Linux) / `libINET.dylib` (macOS) / `INET.dll` (Windows) for linking with simulation targets.

<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->

## Conventions

## Naming Conventions

- Namespace: All classes, structures, and functions must be enclosed within the `inet` namespace.
- Class Names: PascalCase (e.g., `PingApp`, `Ipv4Header`).
- Method Names: camelCase (e.g., `processPingResponse()`, `socketDataArrived()`).
- Fields and Variables: camelCase (e.g., `sentCount`, `destAddr`).
- Constants/Macros: UPPER_CASE (e.g., `PING_HISTORY_SIZE`).
- File Names: PascalCase matching the name of the main module defined inside (e.g., `PingApp.ned`).
- Module Names: PascalCase (e.g., `simple PingApp`, `module StandardHost`).
- Interface Names: PascalCase prefixed with `I` (e.g., `IApp`, `IMacProtocol`).
- Parameters: camelCase (e.g., `sendInterval`, `checksumMode`).
- Messages and classes defined in `.msg` files use PascalCase (e.g., `class PingReply extends FieldsChunk`).

## C++ Coding Style

- NEVER use raw `dynamic_cast` or C-style casts when cast-checking pointers to messages or submodules.
- ALWAYS use `check_and_cast<T*>(pointer)` to automatically verify that the runtime type is correct. If the cast fails, `check_and_cast` throws a clean `cRuntimeError` simulation exception instead of causing a segmentation fault.
- Header guards must be formatted as `__INET_FILENAME_H` (e.g., `#ifndef __INET_PINGAPP_H`).
- Do not use `std::cout` or `printf` for simulation logs.
- Use OMNeT++ logging streams with the `EV` macros, using appropriate stream categories:

## NED Coding Rules

- Always add the `@unit(...)` metadata to double/int physical parameters.
- Example: `double sendInterval @unit(s) = default(1s);`
- When configuring in `.ini` files, always append units to values (e.g. `1s`, `100ms`, `10Mbps`).
- In simple modules, define `@class` linking the NED module to its C++ class.
- Example: `@class(PingApp);`
- Gate declarations should use `@labels` to enforce connection protocol compatibility.
- Example: `input socketIn @labels(ITransportPacket/up);`

## INI File Styling

- **Sections:** Config sections use `[Config ConfigName]` in PascalCase.
- **Abstract bases:** Shared configs must be abstract configurations (`abstract = true`) and child configurations inherit via `extends = BaseName`.
- **Comments:** Use `#` for section headers and inline documentation.

<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->

## Architecture

## Pattern Overview

- **Modular Composition:** Compound modules (like `StandardHost` and `Router`) are assembled from interchangeable submodules implementing individual layer protocols.
- **Discrete Event Simulation:** Driven by an event loop. Events are messages (`cMessage`) scheduled for execution at a specific simulation time.
- **Message-Passing & Flow:** Submodules interact by exchanging packet objects (`inet::Packet`) via connected gates (`gates`) or direct method calls using interfaces.
- **Lifecycle Support:** Nodes and submodules inherit lifecycle behavior, supporting crash, stop, start, and reboot states at runtime.

## Layers

- Purpose: Traffic generators and network services.
- Contains: Ping app, UDP/TCP traffic generators, DHCP clients/servers, and visualizer modules.
- Depends on: Transport Layer interfaces (`IApp` contracts).
- Used by: Node boundary definitions.
- Purpose: End-to-end communication services.
- Contains: TCP, UDP, and SCTP protocol implementations.
- Depends on: Network Layer interfaces.
- Used by: Application layer modules via transport gates.
- Purpose: Packet routing and interface address configuration.
- Contains: IPv4, IPv6, ARP, ICMP, and routing tables.
- Depends on: Link Layer interfaces.
- Used by: Transport layer protocols.
- Purpose: Link transmission and media access control.
- Contains: Ethernet, PPP, and IEEE 802.11 MAC/LLC implementations. Integrates the 802.11ax DL OFDMA scheduler (`IIeee80211HeDlScheduler`) at the AP and the multi-user TXOP frame sequence handler (`HeFrameSequenceHandler`) for managing downlink multi-user frame transmissions and subsequent sequential BlockAck sequences.
- Depends on: Physical layer interface.
- Used by: Network layer protocols.
- Purpose: Physical signal transmission, antenna propagation, and reception.
- Contains: Transmitter, receiver, propagation model, and radio medium. Integrates the HE mode representation (`Ieee80211HeMode`), HE MU PHY header serialization, and the Resource Unit (`Ieee80211HeRu`) model where the radio medium (`Ieee80211RadioMedium`) treats independent RUs as parallel, interference-isolated sub-channels.
- Depends on: Common utility classes.
- Used by: Link layer protocols.

## Data Flow

- Each module maintains its own local C++ state (variables, timers, statistics objects).
- Global network state (routing tables, address mappings, physical environment) is stored in global configuration modules (e.g. `Ipv4NetworkConfigurator` and `InterfaceTable`).

## Key Abstractions

- Purpose: Base class for all C++ simulation code in OMNeT++.
- Examples: `PingApp`, `Ipv4`, `Udp`.
- Pattern: Hook Methods (`initialize()`, `handleMessage()`, `finish()`).
- Purpose: Container for simulated data payloads and headers.
- Examples: Extends `cPacket` using a dynamic chunk system (`inet::FieldsChunk`).
- Purpose: Triggers state changes like Start, Stop, or Crash across submodules.
- Examples: `LifecycleOperation` handled in `handleStartOperation` and `handleStopOperation`.
- Purpose: Represents a Resource Unit partition in 802.11ax (HE), defining sub-channel bandwidth, center frequency, and subcarrier range.
- Purpose: Interface for Downlink OFDMA MAC schedulers at the AP.

## Entry Points

- Location: Native OMNeT++ runner invokes `initialize(int stage)` and `handleMessage(cMessage *msg)` on each simple module.
- Triggers: Start of simulation, packet arrival at gates, or expiration of scheduled self-messages (timers).

## Error Handling

- Call `throw cRuntimeError("description")` to instantly halt the simulation with a stack trace and print the error location.
- Assert validation via `ASSERT` macros.

<!-- GSD:architecture-end -->

<!-- GSD:skills-start source:skills/ -->

## Project Skills

No project skills found. Add skills to any of: `.agent/skills/`, `.agents/skills/`, `.cursor/skills/`, `.github/skills/`, or `.codex/skills/` with a `SKILL.md` index file.
<!-- GSD:skills-end -->

<!-- GSD:workflow-start source:GSD defaults -->

## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:

- `/gsd-quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd-debug` for investigation and bug fixing
- `/gsd-execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->

<!-- GSD:profile-start -->

## Developer Profile

> Profile not yet configured. Run `/gsd-profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
