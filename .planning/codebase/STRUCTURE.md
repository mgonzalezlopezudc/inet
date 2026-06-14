# Codebase Structure

**Analysis Date:** 2026-06-14

## Directory Layout

```
inet/
├── bin/                 # Script wrappers to run simulations and tests
├── doc/                 # Doxygen configuration and developer documentation
├── examples/            # Simulation config directories (omnetpp.ini, NED configurations)
├── python/              # Python test execution modules and results processors
├── showcases/           # Advanced tutorials demonstrating specific protocol setups
├── src/
│   └── inet/            # Core INET Framework source code
│       ├── applications/# Traffic generators and service modules
│       ├── common/      # Shared headers, socket mappings, lifecycle components
│       ├── linklayer/   # L2 Ethernet, PPP, and 802.11 MAC implementations
│       ├── mobility/    # Node movement models
│       ├── networklayer/# L3 IPv4, IPv6, ARP, ICMP, and routing databases
│       ├── node/        # Pre-assembled network node definitions (StandardHost, Router)
│       ├── routing/     # Dinamic routing protocols (AODV, OSPF, BGP)
│       └── transportlayer/ # L4 TCP, UDP, and SCTP protocols
├── tests/               # Validation, unit, and fingerprint tests
└── tutorials/           # Learning modules and tutorial documentation
```

## Directory Purposes

**`src/inet/applications`:**
- Purpose: Simulated applications generating network traffic.
- Contains: C++ classes, NED modules, and MSG definitions.
- Key files: `PingApp.cc`, `PingApp.ned`.

**`src/inet/transportlayer`:**
- Purpose: Implementations of Transport Layer protocols.
- Contains: TCP (multiple variants like TCP-Tahoe, TCP-Reno), UDP, and SCTP.
- Key files: `tcp/Tcp.cc`, `udp/Udp.cc`.

**`src/inet/networklayer`:**
- Purpose: Network addressing, forwarding, and route resolution.
- Contains: IPv4 and IPv6 network protocol layers and tables.
- Key files: `ipv4/Ipv4.cc`, `common/L3Address.cc`, `common/InterfaceTable.cc`.

**`src/inet/linklayer`:**
- Purpose: Data link layer configurations and framing.
- Contains: Ethernet, wireless (802.11), and point-to-point (PPP) MAC protocols.
- Key files: `ethernet/common/Ethernet.cc`, `ieee80211/mac/Ieee80211Mac.cc`.

**`src/inet/node`:**
- Purpose: Assembled hosts, routers, and network devices.
- Contains: Compound NED modules linking network stacks together.
- Key files: `inet/StandardHost.ned`, `inet/Router.ned`, `ethernet/EthernetSwitch.ned`.

**`examples/`:**
- Purpose: Practical simulation testbeds.
- Contains: `omnetpp.ini` simulation setups, custom local NED topologies.
- Key files: `examples/inet/pingapp/omnetpp.ini`.

**`tests/`:**
- Purpose: Regression and correctness testing suite.
- Contains: Unit `.test` files, regression fingerprint CSV lists, and Python scripts.
- Subdirectories: `tests/unit/`, `tests/fingerprint/`, `tests/smoke/`.

## Key File Locations

**Entry Points:**
- `src/inet/common/InitStages.h` - Stage counts for initialization phases of simulation nodes.
- `bin/inet` - Main CLI wrapper script for executing INET simulations.

**Build Configurations:**
- `Makefile` - GNU Make compilation configuration.
- `.oppbuildspec` - IDE parameter mappings for generating secondary Makefiles.
- `.oppfeatures` - Defines compile-time optional features.

---

*Structure analysis: 2026-06-14*
*Update when directories are restructured*
