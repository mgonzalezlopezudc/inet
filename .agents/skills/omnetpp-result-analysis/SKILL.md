---
name: omnetpp-result-analysis
description: Inspect, filter, query, and export OMNeT++ scalar and vector result files using opp_scavetool. Use after a simulation has generated .sca or .vec files, or when asked to find, compare, or extract recorded simulation statistics.
---

# Analyzing OMNeT++ results

Use `opp_scavetool` after simulations have produced `.sca` and `.vec` files.

## Locate result files

Start by locating available results:

```sh
find . -type f \( -name '*.sca' -o -name '*.vec' \) -print
```

Do not assume that every result file belongs to the configuration or run under investigation. Check filenames and run metadata.

## Inspect a result-file summary

```sh
opp_scavetool query <filename>
```

Because `query` is the default command, this is equivalent:

```sh
opp_scavetool <filename>
```

For several files:

```sh
opp_scavetool query results/*.sca results/*.vec
```

## List result items

Use:

```sh
opp_scavetool query -l <filename>
```

To filter the listed results:

```sh
opp_scavetool query -l -f '<filter>' <filename>
```

For multiple scalar and vector files:

```sh
opp_scavetool query \
  -l \
  -f '<filter>' \
  results/*.sca \
  results/*.vec
```

The filter must follow the result-selection expression syntax supported by the installed OMNeT++ version.

Consult the built-in help before constructing a complex expression:

```sh
opp_scavetool help query
opp_scavetool help patterns
opp_scavetool help filters
```

Also inspect the general command help when needed:

```sh
opp_scavetool --help
```

## Investigation procedure

1. Identify the relevant configuration and run.
2. Locate its `.sca` and `.vec` files.
3. Run an unfiltered summary query.
4. List the available module and result names.
5. Construct the narrowest useful filter.
6. Query scalar and vector files as appropriate.
7. Verify units, module paths, result names, and run attributes.
8. Report all matching results instead of silently selecting one ambiguous match.

## Export matching results

When further analysis requires CSV:

```sh
opp_scavetool export \
  -f '<filter>' \
  -F CSV-R \
  -o results.csv \
  <input-files>
```

Do not overwrite an existing analysis file unless requested.

When exporting vectors, consult the installed command help for options that restrict the vector time interval:

```sh
opp_scavetool help export
```

## Vector indexes

`opp_scavetool` normally creates or updates vector index files automatically when needed.

If explicit indexing is required:

```sh
opp_scavetool index <file.vec>
```

Do not rebuild indexes unnecessarily.

## Reporting

Include:

* Input `.sca` and `.vec` files.
* Exact `opp_scavetool` command.
* Filter expression.
* Configuration and run identifiers.
* Matching module paths and result names.
* Result type: scalar, vector, statistic, or histogram.
* Values and units.
* Exported file path, if any.
* Whether no result matched or several ambiguous results matched.

Do not compare values across runs without confirming that their configurations, iteration variables, units, warm-up periods, and recording settings are compatible.
