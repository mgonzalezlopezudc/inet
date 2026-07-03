# Plan: Check Whether HE/EHT Work Regressed Pre-HE 802.11

## Summary
- Use `f833fd12c70276485178fd50754f1a3c4c6eabbf` as the trusted pre-HE baseline.
- Compare baseline vs current on pre-HE 802.11 scenarios only.
- Decide regression from before/after behavior and protocol evidence, not from current checked-in fingerprints alone.
- For the current checkout, do **not** source `setenv`; the VSCode/Codex environment already has `INET_ROOT=/home/user/omnetpp_ws/inet`.

## Test Matrix
- Current tree: `/home/user/omnetpp_ws/inet` at current `master`.
- Baseline tree: create `/tmp/inet-prehe-f833` at `f833fd12c70276485178fd50754f1a3c4c6eabbf`.
- Run these pre-HE slices in both trees:
  - `tests/fingerprint/wireless-combo.csv` with `-m Ieee80211Radio`.
  - `tests/fingerprint/examples.csv` with `-m "ieee80211|lan80211|layered80211|Ieee80211"`.
  - `tests/fingerprint/showcases.csv` with `-m "ieee80211|Ieee80211"`.
- First pass excludes graphical fingerprints with `-F tyf`.

## Execution
- Current tree command shape:
  ```bash
  rtk proxy bash -lc 'cd tests/fingerprint && ./fingerprinttest -t 1 -F tyf -m Ieee80211Radio wireless-combo.csv'
  ```
- Baseline tree command shape must override `INET_ROOT`, because inherited `INET_ROOT` points to the current checkout:
  ```bash
  rtk proxy bash -lc 'cd /tmp/inet-prehe-f833/tests/fingerprint && INET_ROOT=/tmp/inet-prehe-f833 PATH=/tmp/inet-prehe-f833/bin:$PATH PYTHONPATH=/tmp/inet-prehe-f833/python:$PYTHONPATH ./fingerprinttest -t 1 -F tyf -m Ieee80211Radio wireless-combo.csv'
  ```
- Store logs and generated `.UPDATED`, `.FAILED`, `.ERROR`, and result directories separately for baseline and current.

## Regression Decision Rules
- Probable regression: current errors where baseline passes.
- Strong regression: current changes `tplx` or `~tND` and packet/result evidence shows altered pre-HE behavior.
- Likely harmless or expected: fingerprint changes are explained by intentional non-HE fixes and protocol-visible invariants still match.
- Not enough evidence: fingerprints differ but no packet/result comparison has been done.

## Follow-Up Diagnostics
- Prioritize the current runtime errors in `examples/wireless/ieee80211levelofdetail`, `Layered`, distance `100`, bitrates `6` and `54`.
- For `wireless-combo.csv` `~tND` changes, compare frame contents, ACK/retry behavior, sequence numbers, packet lengths, checksums/FCS, and drop/reception decisions.
- Use PCAPng, scalar/vector results, Cmdenv logs, or event logs only for mismatching cases.

## Acceptance Criteria
- Produce a table with scenario, baseline result, current result, changed fingerprint ingredients, and verdict.
- Classify every runtime error as regression, baseline issue, or environment/build issue.
- Explain every accepted fingerprint update with protocol-level evidence.
- Do not update checked-in fingerprints until changed trajectories are understood and accepted.

## Assumptions
- `f833fd12c70276485178fd50754f1a3c4c6eabbf` is the correct pre-HE baseline.
- Release-mode runs are the default; debug/LLDB is only needed for runtime errors.
- Current-tree commands rely on the already-sourced VSCode environment; only alternate worktrees need explicit `INET_ROOT`.
