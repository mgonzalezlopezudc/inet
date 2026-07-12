# Fragmentation and acknowledgment plots

The first panel compares the ECDF of transmitted MAC packet sizes. Dynamic and
static fragmentation should shift probability mass toward smaller frames
relative to an unfragmented baseline. The second panel is an acknowledgment
overhead proxy combining recorded Block Ack agreement and completed frame
sequence counts.

The current result signals do not identify every BAR and Block Ack frame or
record their airtime. The proxy is useful for regression comparison but is not
a literal count of acknowledgment frames; a future dedicated signal or PCAP
analysis can replace it without changing the surrounding dashboard.
