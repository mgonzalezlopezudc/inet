---
name: inet-lldb-debugging
description: Debug INET and OMNeT++ simulations at the C++ source level with LLDB. Use for runtime errors, crashes, aborts, segfaults, unresolved hangs, targeted breakpoints or watchpoints, and local-variable inspection after logs, captures, event logs, or results are insufficient; use Cmdenv by default and Qtenv only when interactive visualization is also needed.
---

# Debugging INET simulations with LLDB

## Build invariant

Use matching debug components:

```text
opp_run_dbg + libINET_dbg.so + debug project libraries
```

Do not mix release and debug runners or model libraries. `--debug-on-errors=true` triggers a debugger trap for an OMNeT++ runtime error; it does not start LLDB.

## Workflow

1. Reproduce the problem narrowly and preserve the simulation command.
2. Launch that command under LLDB with `opp_run_dbg`, debug model libraries, and `--debug-on-errors=true`.
3. Set a targeted breakpoint before running when the suspicious path is known; avoid stepping from `main()`.
4. At the first relevant stop, record the stop reason and full backtrace before continuing.
5. Select the first project or INET frame that exposes the invalid state; the trap or exception frame is usually only the consequence.
6. Inspect locals with `frame variable` before evaluating expressions.
7. Use conditional breakpoints or watchpoints to locate where the state first diverges.
8. Correlate the stop with the simulation time, event number, module path, and message or packet identity when available.
9. Explain the root cause and evidence before proposing a source change.

## Useful commands

```lldb
process status
thread backtrace all
thread list
frame select <index>
frame info
frame variable --show-types
source list

breakpoint set --file SomeFile.cc --line 123
breakpoint set --name FullyQualifiedFunctionName
breakpoint set --name handleMessage --condition 'msg != nullptr'
breakpoint list
watchpoint set variable state

continue
next
step
finish
process interrupt
```

A watchpoint on a pointer variable detects reassignment of that variable, not destruction of the pointed-to object. For a dangling-pointer investigation, break on the concrete deletion or ownership-transfer path after identifying the object address while it is valid.

Use side-effect-free expressions only when ordinary variable inspection is insufficient:

```lldb
expression -- this->getFullPath().c_str()
expression -- simTime().str().c_str()
expression -- msg->getName()
expression -- msg->getClassName()
```

## OMNeT++ and INET context

At a relevant stop, identify:

* Simulation time and event number.
* Module path and current callback.
* Message or packet name, class, and ownership.
* Gate, sender, receiver, timer, queue, or protocol metadata relevant to the failure.
* The first frame where the invalid value was created or accepted.

For packet metadata, use `inet-packet-tag-debugging`. For IEEE 802.11 paths, use the breakpoint guidance in `inet-80211-packet-debugging`.

## Qtenv and MCP

Use Cmdenv under LLDB by default. When interactive topology, animation, or Qtenv state inspection is necessary, launch the repository's Qtenv `lldb-dap` configuration and drive the same LLDB session through LLDB MCP. Use the Qtenv control surface to advance simulation events and LLDB for C++ state; if an LLDB stop interrupts a pending Qtenv request, inspect the debugger stop before treating the request failure as causal.

## Safety

* Do not continue immediately from a `debug-on-errors` trap before preserving the stack and locals.
* Do not treat the trap function or faulting line alone as the root cause.
* Do not mutate packets, messages, parameters, timers, iterators, or simulation state from debugger expressions unless the user explicitly requests an experiment.
* Do not patch source before the evidence identifies a concrete defect.

Report the stop reason, relevant backtrace and source frame, inspected variables, breakpoints or watchpoints, simulation context, and whether the conclusion is direct debugger evidence or inference.
