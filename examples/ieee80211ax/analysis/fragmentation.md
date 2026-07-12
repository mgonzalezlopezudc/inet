# Fragmentation and acknowledgment plots

The first panel compares the measured ECDF of transmitted MAC frame sizes. Dynamic and
static fragmentation should shift probability mass toward smaller frames
relative to an unfragmented baseline. The second panel sums measured PHY
airtime by acknowledgment frame-type code. ACK, BAR, and Block Ack
transmissions export synchronized `acknowledgmentFrameType` and
`acknowledgmentAirtime` vectors at the radio.
