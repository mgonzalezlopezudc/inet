# Channel-width plots

The dashboard shows aggregate application goodput, goodput normalized by
configured channel width, and the packet-delay ECDF. Wider channels should
increase raw capacity, but goodput per Hz reveals whether that bandwidth is
used efficiently. At the cell edge, wider bandwidth may reduce delivery due to
the noise-integration and sensitivity penalty.

Widths are read from the dataset labels (`20`, `40`, `80`, or `160` MHz), while
goodput and delay come from native OMNeT++ results.
