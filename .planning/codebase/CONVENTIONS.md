# Coding Conventions

**Analysis Date:** 2026-06-14

## Naming Conventions

**C++ Coding:**
- Namespace: All classes, structures, and functions must be enclosed within the `inet` namespace.
- Class Names: PascalCase (e.g., `PingApp`, `Ipv4Header`).
- Method Names: camelCase (e.g., `processPingResponse()`, `socketDataArrived()`).
- Fields and Variables: camelCase (e.g., `sentCount`, `destAddr`).
- Constants/Macros: UPPER_CASE (e.g., `PING_HISTORY_SIZE`).

**NED Modules:**
- File Names: PascalCase matching the name of the main module defined inside (e.g., `PingApp.ned`).
- Module Names: PascalCase (e.g., `simple PingApp`, `module StandardHost`).
- Interface Names: PascalCase prefixed with `I` (e.g., `IApp`, `IMacProtocol`).
- Parameters: camelCase (e.g., `sendInterval`, `checksumMode`).

**Message/Packet Files (`.msg`):**
- Messages and classes defined in `.msg` files use PascalCase (e.g., `class PingReply extends FieldsChunk`).

## C++ Coding Style

**Pointer Casting (OMNeT++ Safe Cast):**
- NEVER use raw `dynamic_cast` or C-style casts when cast-checking pointers to messages or submodules.
- ALWAYS use `check_and_cast<T*>(pointer)` to automatically verify that the runtime type is correct. If the cast fails, `check_and_cast` throws a clean `cRuntimeError` simulation exception instead of causing a segmentation fault.

**Header Guards:**
- Header guards must be formatted as `__INET_FILENAME_H` (e.g., `#ifndef __INET_PINGAPP_H`).

**Logging:**
- Do not use `std::cout` or `printf` for simulation logs.
- Use OMNeT++ logging streams with the `EV` macros, using appropriate stream categories:
  - `EV_INFO` - Standard protocol events (e.g. "Packet sent").
  - `EV_WARN` - Non-fatal warnings/retransmissions.
  - `EV_DETAIL` - Detailed state traces for step-by-step debugging.

## NED Coding Rules

**Physical Units:**
- Always add the `@unit(...)` metadata to double/int physical parameters.
- Example: `double sendInterval @unit(s) = default(1s);`
- When configuring in `.ini` files, always append units to values (e.g. `1s`, `100ms`, `10Mbps`).

**C++ Linkage:**
- In simple modules, define `@class` linking the NED module to its C++ class.
- Example: `@class(PingApp);`

**Gate Labels:**
- Gate declarations should use `@labels` to enforce connection protocol compatibility.
- Example: `input socketIn @labels(ITransportPacket/up);`

## INI File Styling

- **Sections:** Config sections use `[Config ConfigName]` in PascalCase.
- **Abstract bases:** Shared configs must be abstract configurations (`abstract = true`) and child configurations inherit via `extends = BaseName`.
- **Comments:** Use `#` for section headers and inline documentation.

---

*Convention audit: 2026-06-14*
*Update as coding rules evolve*
