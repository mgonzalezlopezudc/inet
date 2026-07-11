# HE channel-width comparison

Run `Width20MHz`, `Width40MHz`, `Width80MHz`, and `Width160MHz` with the same
seed and offered load. Compare receiver `packetReceived` counts, application
throughput, and the AP scheduler's RU allocation. The configurations exercise
the supported contiguous HE widths and the 160 MHz 1992-tone RU path.

This model deliberately does not label 160 MHz as 80+80 MHz: INET's radio
channel representation is contiguous, so non-contiguous 80+80 operation is
not demonstrated here.
