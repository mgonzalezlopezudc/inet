---
name: inet-ned-ini-analysis
description: Analyze INET NED and omnetpp.ini configuration behavior. Use when Codex needs to trace module types, NED inheritance, INI config inheritance, wildcard precedence, parameter overrides, effective module paths, typename selection, radio/medium pairing, recorder paths, or configuration bugs before running or debugging simulations.
---

# INET NED and INI analysis

## Core rule

Prove the instantiated module path, type, and effective parameter value before reasoning from it. Do not assume a wildcard override matched the intended module.

## Workflow

1. Identify the working directory, `omnetpp.ini`, configuration name, run number, and network type.
2. Read the relevant `[Config ...]` chain and `extends` relationships before inspecting isolated lines.
3. Search NED files for the module type, submodule name, parameter declaration, default value, and inherited base type.
4. Evaluate every matching assignment using the checked-out OMNeT++ configuration-precedence rules; show why the selected assignment wins instead of relying on a vague "later" or "more specific" heuristic.
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
* An apparently relevant assignment may lose because of configuration inheritance, entry order, or pattern matching; identify the actual winning entry.
* A `typename` may be inherited from a base config or NED default.
* Radio and radio-medium analog representations must be compatible.
* Unit suffixes matter for time, frequency, power, bitrate, and length parameters.
* A parameter name visible in one INET version may not exist in another checkout.

Report the config chain, instantiated type and module path, relevant parameter assignments, the winning precedence reasoning, and any ambiguity requiring a diagnostic run.
