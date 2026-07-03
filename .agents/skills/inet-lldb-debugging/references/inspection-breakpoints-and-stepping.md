## Contents

* Inspect a runtime-error stop
* Evaluate expressions cautiously
* Set a breakpoint by source location
* Set a breakpoint by function name
* Set a conditional breakpoint
* Set and use watchpoints
* Step through the source

---

## Inspect a runtime-error stop

When the simulation stops, begin with:

```text
(lldb) process status
(lldb) thread list
(lldb) thread backtrace all
```

For the selected thread:

```text
(lldb) bt
```

Inspect the current frame:

```text
(lldb) frame select
(lldb) source list
(lldb) frame variable
```

Navigate the stack:

```text
(lldb) up
(lldb) down
(lldb) frame select <frame-number>
```

The top frame may be the OMNeT++ debugger-trap or error-handling implementation.
Move upward until reaching the first frame in:

* The project source tree.
* `$INET_ROOT/src`.
* A relevant OMNeT++ API call made by project or INET code.

Do not treat the trap function itself as the root cause.

For the relevant frame, record:

```text
(lldb) frame variable
(lldb) source list
```

Inspect one variable without evaluating arbitrary code:

```text
(lldb) frame variable <variable-name>
```

Examples:

```text
(lldb) frame variable packet
(lldb) frame variable msg
(lldb) frame variable state
(lldb) frame variable retryCount
(lldb) frame variable this
```

Prefer `frame variable` over `expression` when merely reading variables,
because it does not execute code in the debugged process.

---

## Evaluate expressions cautiously

Use:

```text
(lldb) expression -- <C++ expression>
```

or its aliases:

```text
(lldb) expr -- <C++ expression>
(lldb) p <C++ expression>
```

Examples:

```text
(lldb) expression -- retryCount
(lldb) expression -- packet == nullptr
(lldb) expression -- queue.getLength()
```

Expression evaluation may call functions or otherwise execute code in the
stopped process. Avoid expressions that:

* Mutate simulation state.
* Remove or insert packets.
* Advance iterators with side effects.
* Schedule or cancel messages.
* Emit signals.
* Change module parameters.
* Continue simulation execution indirectly.

Do not “fix” a variable inside LLDB and then treat the resulting run as valid
simulation evidence unless the user explicitly requests an experimental
state modification.

---

## Set a breakpoint by source location

Set a breakpoint before running:

```text
(lldb) breakpoint set --file <source-file.cc> --line <line-number>
```

Example:

```text
(lldb) breakpoint set --file Ieee80211Mac.cc --line 420
```

Use the source filename rather than an absolute path when debug information
contains paths from a different build directory.

List breakpoints and verify that they resolved:

```text
(lldb) breakpoint list
```

A breakpoint with zero locations is pending or unresolved.

Pending breakpoints may resolve when `libINET_dbg.so` or a project library is
loaded. Check again after simulation startup:

```text
(lldb) breakpoint list
```

If the breakpoint remains unresolved after the relevant library has loaded,
check:

* Source filename.
* Line number.
* Build mode.
* Loaded library.
* Debug information.
* Whether the source was optimized or inlined.
* Whether the executing code comes from a different library copy.

---

## Set a breakpoint by function name

Use a fully qualified C++ function name when known:

```text
(lldb) breakpoint set --name 'inet::SomeClass::someMethod'
```

Example form:

```text
(lldb) breakpoint set \
  --name 'inet::physicallayer::SomeReceiver::computeReceptionResult'
```

The exact class and namespace must come from the source code. Do not guess a
symbol name when it can be found with source search.

Search loaded symbols:

```text
(lldb) image lookup --name 'inet::SomeClass::someMethod'
```

List loaded executable images and shared libraries:

```text
(lldb) image list
```

Use a function-name regular expression only when several related methods are
intentionally being investigated:

```text
(lldb) breakpoint set --func-regex 'Ieee80211.*::.*'
```

Broad regular-expression breakpoints may create many locations and severely
slow the simulation.

---

## Set a conditional breakpoint

Use a condition when the relevant function is called frequently:

```text
(lldb) breakpoint set \
  --file <source-file.cc> \
  --line <line-number> \
  --condition '<C++ boolean expression>'
```

Example:

```text
(lldb) breakpoint set \
  --file SomeScheduler.cc \
  --line 250 \
  --condition 'retryCount > 3'
```

List the resulting breakpoint:

```text
(lldb) breakpoint list
```

Conditions are evaluated each time the breakpoint location is reached and can
slow a frequently executed simulation path.

Keep conditions:

* Side-effect free.
* Narrow.
* Based on variables available in that frame.
* Independent of unstable temporary objects.

---

## Set and use watchpoints

A watchpoint stops when a selected memory value changes.

First stop in a frame where the variable is visible, then run:

```text
(lldb) watchpoint set variable <variable-name>
```

Example:

```text
(lldb) watchpoint set variable state
```

List watchpoints:

```text
(lldb) watchpoint list
```

Continue:

```text
(lldb) continue
```

When the watchpoint triggers:

```text
(lldb) process status
(lldb) bt
(lldb) frame variable
```

Watchpoints are limited hardware resources and may significantly slow
execution. Use them only for a small number of variables.

A watchpoint on a local variable becomes invalid when that variable’s stack
frame ends. For long-lived state, prefer a field of a long-lived module object.

---

## Step through the source

After stopping at a relevant breakpoint:

```text
(lldb) next
```

steps over the current source line.

```text
(lldb) step
```

steps into a called function.

```text
(lldb) finish
```

runs until the current function returns.

```text
(lldb) continue
```

continues until the next breakpoint, trap, signal, or process exit.

Avoid instruction-level stepping unless source-level stepping is unavailable:

```text
(lldb) stepi
(lldb) nexti
```

Stepping into heavily templated C++ or standard-library code may obscure the
model logic. Use `finish` to return to the relevant INET or project frame.

---

