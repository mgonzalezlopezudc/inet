# Testing Patterns

**Analysis Date:** 2026-06-14

## Test Framework

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

## Test File Organization

**Unit Tests (`tests/unit/`):**
- Placed under `tests/unit/` with `.test` extension.
- File naming: PascalCase matching target component, followed by a sequence number (e.g., `MACAddress_1.test`).

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

*Testing audit: 2026-06-14*
*Update after adding new test frameworks*
