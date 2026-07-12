# Target Wake Time plots

The radio-state raster should expose long, repeated low-power intervals for TWT
stations and a continuously available AP. The companion bars divide recorded
energy consumption by delivered application bits. A useful TWT configuration
reduces joules per delivered bit without unacceptable loss or delay.

Zero energy bars mean the selected run did not instantiate an energy consumer;
they must not be interpreted as zero physical consumption. Use
`BaselineEnergy` and `TwtEnergySaving` for the energy comparison.
