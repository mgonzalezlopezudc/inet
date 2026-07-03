---
name: inet-ned-ini-analysis
description: Analyze INET NED and omnetpp.ini configuration behavior. Use when Codex needs to trace module types, NED inheritance, INI config inheritance, wildcard precedence, parameter overrides, effective module paths, typename selection, radio/medium pairing, recorder paths, or configuration bugs before running or debugging simulations.
---

# INET NED and INI analysis

Use this skill when a suspected issue may be caused by effective configuration rather than C++ code: wrong module path, inherited override, wildcard precedence, missing `typename`, unexpected default, incompatible radio/medium type, or recorder/logging override that matches nothing.

## Core rule

Prove the instantiated module path, type, and effective parameter value before reasoning from it. Do not assume a wildcard override matched the intended module.

## Workflow

1. Identify the working directory, `omnetpp.ini`, configuration name, run number, and network type.
2. Read the relevant `[Config ...]` chain and `extends` relationships before inspecting isolated lines.
3. Search NED files for the module type, submodule name, parameter declaration, default value, and inherited base type.
4. Check wildcard specificity and order for every relevant INI override.
5. Verify `typename` assignments for compound/simple modules, radios, MACs, queues, management modules, recorders, and radio media.
6. Confirm module paths using the actual network hierarchy, not guessed paths from examples.
7. Prefer command-line overrides for temporary diagnostics; do not edit `omnetpp.ini` solely to enable logging, PCAP, event logs, or result inspection.
8. When the effective configuration is still uncertain, run one short Cmdenv initialization or diagnostic simulation and inspect startup output/logs.

## Useful searches

```sh
rg -n 'extends|network|typename|wlan|radioMedium|mac|mgmt|agent|pcapRecorder|numPcapRecorders' omnetpp.ini .
rg -n 'module .*|simple .*|compound .*|parameters:|submodules:' path/to/*.ned src/inet
rg -n 'parameterName|submoduleName|TypeName' src examples showcases tutorials
```

Adapt paths to the project. Use `rg --files -g '*.ned'` and `rg --files -g '*.ini'` when the model location is unknown.

## INET-specific traps

* `moduleNamePatterns` for `PcapRecorder` is relative to the node containing the recorder, not a full network path.
* `**` wildcard overrides can unintentionally match several modules or none.
* Later or more-specific INI assignments may override earlier assumptions.
* A `typename` may be inherited from a base config or NED default.
* Radio and radio-medium analog representations must be compatible.
* Unit suffixes matter for time, frequency, power, bitrate, and length parameters.
* A parameter name visible in one INET version may not exist in another checkout.

## Reporting

Include the exact config chain, network type, module path, NED type and source file, parameter names and values, matching INI lines, wildcard reasoning, command-line overrides, and any ambiguity that requires a diagnostic run.
