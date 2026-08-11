# IEEE 802.11 HE planning performance baseline

This harness measures deterministic HE scheduling and transmission workloads in
fresh sequential processes. It is separate from correctness tests because CPU
time and memory limits are machine-dependent.

The matrix has 36 stable cases: station counts 1, 4, 9, and 37; widths 20, 80,
and 160 MHz; and single-user (SU), downlink OFDMA (DL), and scheduled uplink
OFDMA (UL) workload families. Case names use the form
`<family>-<stations>sta-<width>mhz`, such as `dl-37sta-160mhz`. List the exact
case-to-configuration mapping with:

```sh
python3 tests/speed/ieee80211heplanning/run.py --list-cases
```

The matrix currently defines and initializes all 36 cases, but only the SU
family is ready for full one-second baseline collection. Sustained DL traffic
currently reaches a peer-capability/mode-legality failure, and sustained UL
traffic exposes an A-MSDU fragmentation/BSR parsing defect. These are tracked
as production correctness blockers; short initialization or smoke runs must
not be reported as performance samples for the affected families.

Each case has a fixed one-second simulation, seed 1, and stationary near-AP
placement. DL and SU establish one ADDBA agreement per station at 200 ms, then
run one active 1000-byte UDP flow per station from 300 ms onward; UL uses one
station-to-server flow per station from 20 ms onward. The SU case deliberately
disables MU selection while retaining the DL queue shape.

Run a profile baseline from the repository root:

```sh
MPLCONFIGDIR=/tmp/inet-matplotlib \
python3 tests/speed/ieee80211heplanning/run.py \
  --mode profile --case dl-4sta-20mhz --run 0 --seed 1 \
  --warmups 3 --samples 11 \
  --result-dir /tmp/inet-he-planning-perf \
  --write-baseline /tmp/inet-he-planning-perf/baseline.json
```

Compare a later run by replacing `--write-baseline` with
`--baseline /tmp/inet-he-planning-perf/baseline.json`. The comparison fails on
the relative CPU/RSS limits in `budgets.json`. A reported improvement must be at
least 10 percent and more than twice the measured baseline noise.

CPU time and peak RSS are recorded for every sample. Instruction and cycle
counts are recorded when the host permits access to hardware performance
counters; otherwise they are explicitly marked unavailable. Canonical CPU time
always comes from the same process-resource measurement, so baselines remain
comparable across hardware-counter permission changes.

Baseline creation refuses to replace an existing file. Use
`--replace-baseline` together with `--write-baseline` only when replacement is
intentional; comparison and baseline-writing modes are mutually exclusive.
The planning-operation counter schema is also recorded, but its values remain
explicitly unavailable until counters are added to their owning MAC/PHY
components in later refactoring tranches. The relative budgets intentionally do
not define absolute machine timings; write and compare a baseline only on a
like-for-like release/profile environment.
