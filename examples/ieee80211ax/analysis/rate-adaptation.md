# Rate adaptation plots

The timeline aligns selected HE MCS, NSS, PHY data rate, and delivered
throughput. A healthy controller should move toward higher-rate modes after
sustained successes and retreat after losses. In the mobile scenario, changes
should follow changing link quality rather than oscillating continuously.

Some runs declare MCS/NSS vectors without emitting samples. In that case the
figure omits those panels and retains the populated data-rate and throughput
signals; it never invents MCS values from bit rates.
