# HE channel-width comparison

This example isolates the effect of contiguous HE channel width. The topology,
four downlink flows, MCS-equivalent configured rate, seed, and one-second run
are held constant while the channel changes from 20 to 40, 80, and 160 MHz.

## What changes with width

A wider channel contains more HE OFDM tones and therefore permits either more
simultaneous RUs or larger RUs. The full-bandwidth HE RU sizes are 242 tones at
20 MHz, 484 at 40 MHz, 996 at 80 MHz, and 2x996 at 160 MHz. The scheduler is
not required to use one full-bandwidth RU: in this example it partitions the
channel among up to four stations with `HeDlSchedulerEqualSizedRUs`.

The configured bitrates scale with width (14.625, 29.25, 61.25, and 122.5
Mbit/s). Wider bandwidth also integrates more noise and does not automatically
improve range. This close, controlled topology is intended to demonstrate RU
capacity, not a coverage comparison.

## Run and inspect

```sh
bin/inet -u Cmdenv -c Width20MHz examples/ieee80211ax/he_channel_widths/omnetpp.ini
bin/inet -u Cmdenv -c Width40MHz examples/ieee80211ax/he_channel_widths/omnetpp.ini
bin/inet -u Cmdenv -c Width80MHz examples/ieee80211ax/he_channel_widths/omnetpp.ini
bin/inet -u Cmdenv -c Width160MHz examples/ieee80211ax/he_channel_widths/omnetpp.ini
```

Compare application goodput and per-station delivery, then inspect the AP
HCF scheduler's `lastScheduleSummary` and `lastRuAllocations` watches in
Qtenv. Equal packet counts can simply mean that the offered load is below the
capacity of every width; RU layout and airtime remain the direct evidence that
the intended width-specific path ran.

This model deliberately does not label 160 MHz as 80+80 MHz. INET's radio
channel representation here is contiguous, so non-contiguous 80+80 operation
is outside the experiment.
