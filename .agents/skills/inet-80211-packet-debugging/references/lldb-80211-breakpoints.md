## Contents

* 27. LLDB Debugging
* 27.1 Start under LLDB
* 27.2 Useful commands
* 27.3 Breakpoint targets
* 27.4 Conditional breakpoints
* 27.5 Runtime errors

---

# 27. LLDB Debugging

Use LLDB after identifying the suspicious module, packet, event, or transition.

## 27.1 Start under LLDB

```sh
lldb -- \
  "$OPP_RUN" \
  -u Cmdenv \
  -f "$INI_FILE" \
  "--ned-path=$NED_PATH" \
  --debug-on-errors=true \
  -l "$INET_LIBRARY" \
  -c "$CONFIG" \
  -r "$RUN"
```

Then:

```text
(lldb) run
```

## 27.2 Useful commands

```text
(lldb) breakpoint set --file SourceFile.cc --line 245
(lldb) breakpoint set --name FullyQualifiedFunctionName
(lldb) breakpoint list
(lldb) breakpoint delete <id>
(lldb) continue
(lldb) next
(lldb) step
(lldb) finish
(lldb) bt
(lldb) frame variable
(lldb) frame select <index>
(lldb) expression <expression>
(lldb) thread list
```

## 27.3 Breakpoint targets

Place breakpoints at the installed implementation’s equivalents of:

* Frame enqueue
* Traffic classification
* Channel-access request
* Channel-access grant
* Backoff start, freeze, and expiration
* NAV update
* RTS decision
* Frame-sequence selection
* Transmission construction
* Reception attempt
* Reception success decision
* ACK and CTS timeout
* Retry-counter update
* Retry-limit drop
* Duplicate removal
* Fragmentation and defragmentation
* Aggregation and deaggregation
* Block Ack negotiation
* Reordering-window update
* Beacon processing
* Scan result processing
* Authentication state transition
* Association state transition
* AP forwarding

Search first:

```sh
rg -n \
  'startBackoff|channelAccess|updateNav|retry|drop|computeReception|isReceptionSuccessful|associate|authenticate|BlockAck|aggregate|fragment' \
  "$INET_ROOT/src/inet"
```

## 27.4 Conditional breakpoints

Use conditions based on simple stable values, such as:

* Sequence number
* MAC address
* Retry count
* Event number
* Simulation time
* Module path
* Packet name

Example:

```text
(lldb) breakpoint set --file RecoveryProcedure.cc --line 210
(lldb) breakpoint modify --condition 'retryCount == 3' 1
```

Avoid invoking complex mutating methods from debugger expressions.

## 27.5 Runtime errors

With:

```text
--debug-on-errors=true
```

OMNeT++ can trap into the debugger on runtime errors.

When stopped:

```text
(lldb) bt
(lldb) frame variable
(lldb) thread list
```

Find the earliest project or INET frame where state first became invalid.

The exception site may only expose the consequence.

---

