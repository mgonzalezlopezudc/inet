# Testing Patterns

**Analysis Date:** 2026-06-16

## Test Framework

**Always set up the environment before running any test:**
   Make sure the OMNeT++ environment is sourced and `PYTHONPATH` includes the OMNeT++ python tools:
   ```bash
   source setenv
   export PYTHONPATH=/home/user/omnetpp-6.4.0/python:$PYTHONPATH
   ```

**Runner:**
- **OMNeT++ opp_test:** Runs unit tests by reading `.test` files, compiling their contents into small transient simulation executables, running them, and validating output.
- **Python test suite:** Orchestrates executing unit, smoke, speed, statistical, and fingerprint tests.
- **Fingerprint Test Runner:** Runs configurations against recorded checksums to verify execution reproducibility.

**Assertion Library:**
- OMNeT++ `%contains: stdout` format - matches the text printed during execution.
- Standard C++ `assert()` and `ASSERT()` macros.

**Run Commands:**
```bash
./bin/inet_run_unit_tests          # Run all unit tests under tests/unit/
./bin/inet_fingerprinttest         # Run simulation fingerprint tests
./bin/inet_run_all_tests           # Run the entire test suite
```

## Running a Single Unit Test

If you need to compile and execute a single unit test (e.g., `tests/unit/Ieee80211HeMode_1.test`), follow these steps:

1. **Set up the environment:**
   Make sure the OMNeT++ environment is sourced and `PYTHONPATH` includes the OMNeT++ python tools:
   ```bash
   source setenv
   export PYTHONPATH=/home/user/omnetpp-6.4.0/python:$PYTHONPATH
   ```

2. **Generate the test sources:**
   Generate the C++ source files from the `.test` specification:
   ```bash
   cd tests/unit
   opp_test gen -v <TestName>.test
   ```
   This creates a directory `work/<TestName>/` containing `test.cc` and `test.ned`.

3. **Generate the Makefile:**
   Navigate into the generated directory and run `opp_makemake` to link against the INET libraries:
   ```bash
   cd work/<TestName>
   opp_makemake -f --deep -lINET_dbg -L../../../../src -ltest_dbg -L../../lib -I../../../../src -I../../lib
   ```
   *(Note: Remove `-ltest_dbg -L../../lib -I../../lib` if the test does not depend on auxiliary unit test libraries).*

4. **Compile the test binary:**
   Build the executable:
   ```bash
   make MODE=debug -j$(nproc)
   ```

5. **Run the test case:**
   Return to `tests/unit/` and execute the test via `opp_test run`, pointing to the built binary and specifying the correct NED path:
   ```bash
   cd ../..
   opp_test run -v -p ../../../out/clang-debug/tests/unit/work/<TestName>/inet_dbg <TestName>.test -a "--check-signals=false -lINET -n ../../../../src:.:../../lib"
   ```

## Test File Organization

**Unit Tests (`tests/unit/`):**
- Placed under `tests/unit/` with `.test` extension.
- File naming: PascalCase matching target component, followed by a sequence number (e.g., `MACAddress_1.test`).
- **IEEE 802.11ax DL OFDMA Unit Tests:**
  - `Ieee80211HeMode_1.test` - Verifies HE MCS tables and modes.
  - `Ieee80211HeRu_1.test` - Verifies physical Resource Unit configurations and sub-channel indices.
  - `HeDlScheduler_1.test` - Verifies equal-sized Resource Unit scheduling logic.
  - `Ieee80211HeMuPhyHeaderSerializer_1.test` - Verifies serialization and dissector parsing of the HE MU PHY header.
  - `Ieee80211HeMuRx_1.test` - Verifies multi-user reception, RU tag assignment, and station filtering.
  - `Ieee80211HeMuSeqAck_1.test` - Verifies the sequential BlockAck request/response sequence after a DL MU transmission.

**Structure of a `.test` file:**
```
%description:
[Short text explaining what the test verifies]

%includes:
[C++ header file includes]

%global:
[Helper definitions or global classes]

%activity:
[C++ logic executed inside the test module]

%contains: stdout
[Expected console output lines]
```

**Fingerprint Tests (`tests/fingerprint/`):**
- Grouped in CSV files by category (e.g., `examples.csv`, `showcases.csv`, `tutorials.csv`).
- Each row specifies the config name, working directory, and the recorded fingerprint hash representing event execution path.

## Best Practices

- **Excluding GUI:** When running fingerprint tests, use the `-F tyf` option to exclude graphical (tyf) fingerprints, as visual/gui updates frequently change.
- **No Segfaults:** Make sure to use `check_and_cast` inside unit tests to verify pointers without risking unhandled segment faults.
- **Deterministic:** Avoid unseeded random variables or system time functions in tests to ensure results are fully deterministic.

---

*Testing audit: 2026-06-16*
*Update after adding new test frameworks*
