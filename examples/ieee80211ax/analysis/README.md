# IEEE 802.11ax result plots

`plot_results.py` creates didactical figures directly from OMNeT++ scalar and
vector files using `omnetpp.scave.results`. It does not parse result files or
use CSV intermediates.

Run it from the INET repository root in an OMNeT++-configured shell:

```sh
python examples/ieee80211ax/analysis/plot_results.py \
    examples/ieee80211ax/he_rate_adaptation/results \
    --suite rate \
    --output-dir examples/ieee80211ax/he_rate_adaptation/analysis/figures
```

Available suites are `common`, `rate`, `bsr`, `twt`, and `auto`. The common
suite produces a packet-delay ECDF and event-driven queue occupancy plot when
the corresponding vectors exist. `auto` attempts every plot and reports
missing inputs as `SKIP` messages.

Use `--inspect` to print the actual modules, result names, units, and result
types before plotting. Every plotting query is validated for nonempty vectors,
equal time/value array lengths, and monotonic simulation time.

The script intentionally limits dense figures to the eight vectors with the
most recorded samples. The matched vectors are printed so this selection is
auditable. Packet-delay ECDFs pool no modules: every displayed curve belongs
to one recorded module and one run. Queue and radio-mode vectors are drawn as
steps because they are piecewise-constant event-driven quantities.

Checked-in result artifacts in this tree may appear below a repeated
`examples/ieee80211ax/.../results` path depending on the working directory
used for the simulation. Pass that actual directory to the script; generated
figures should go in the example's `analysis/figures` directory.

## Complete first tranche

`first_tranche.py` implements the ten cross-configuration plot groups. Supply
one or more labeled inputs; labels become figure categories:

```sh
python examples/ieee80211ax/analysis/first_tranche.py width \
    --input 20MHz=/path/to/Width20MHz-results \
    --input 80MHz=/path/to/Width80MHz-results \
    --output-dir examples/ieee80211ax/analysis/figures/channel-width
```

The group names are `rate`, `bsr`, `bss`, `width`, `twt`, `dl`, `uora`,
`puncturing`, `fragmentation`, and `mimo`. Each has a same-directory Markdown
guide explaining the expected observation and identifying the result signals
that supply each panel.
