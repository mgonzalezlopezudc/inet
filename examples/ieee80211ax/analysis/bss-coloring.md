# BSS coloring plots

The dashboard compares aggregate goodput, per-receiver Jain fairness, and the
fraction of sampled time in which both AP radios transmit concurrently.
Enabling distinct BSS colors should increase useful concurrency and aggregate
capacity. An aggressive OBSS/PD threshold can raise concurrency while reducing
fairness or delivery, whereas a color collision should lose much of the gain.

Concurrency is derived from recorded AP transmission-state vectors on a common
time grid; it is not inferred from packet counts.
