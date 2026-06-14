# 802.11ax DL OFDMA Stack Research

## Core Technologies

- **Simulation Platform**: OMNeT++ 6.3.0.
- **Framework**: INET Framework (L2 MAC/L1 PHY model).
- **Language**: C++17 (for simulation logic), NED (for topology), and MSG (for frame structures).
- **Build System**: GNU Make with `opp_makemake`.

## Key Dependencies and Libraries

- **OMNeT++ Core Library (`sim_std`)**: Offers event-loop execution, simple modules, packet management (`cPacket`), and statistics tracking.
- **INET Common Utilities**: Handles physical signal representations, spectrum analysis, path loss, and noise calculation.
- **C++ Standard Template Library (STL)**: Utilized for scheduler queues, user mappings, and mapping functions.

## Rationale for Tech Stack Choices

- **C++17**: Ensures high performance during discrete-event simulations, allowing fine-grained calculations of transmission timings and signal-to-noise ratios.
- **NED Module System**: Provides clean module decoupling, permitting the DL OFDMA scheduler to be plugged in or replaced dynamically without recompiling the core MAC.
- **OMNeT++ Message Compiler (`opp_msgc`)**: Compiles `.msg` packet formats directly to C++ headers and source files, ensuring serialization safety and introspection support.

## Confidence Levels
- **OMNeT++ 6.3.0 + INET Compatibility**: High. The framework has stable interfaces for MAC and PHY layer models.
- **Abstract RU modeling via independent sub-channels**: High. Fits naturally into INET's spectrum and sub-channel abstractions.
