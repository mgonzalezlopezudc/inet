IEEE 802.15.4 Beacon Synchronization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Custom ``IIeee802154ManagementServiceProvider`` and ``IIeee802154ManagementServiceClient``
implementations must add ``submitSync()`` and ``handleSyncConfirm()`` and rebuild. The native
confirmation reports local installation, not successful beacon reception. After installation, the
request ID is no longer cancellable; reset or a replacement SYNC retires the ongoing tracker.

Custom superframe providers and clients must implement the new synchronization wake and deadline
methods. The deadline event must run after equal-time received-packet delivery.

The fixed-rate external-clock profile requires a nominal scheduling quantum that divides the
16 µs PHY symbol, ``OscillatorBasedClock.clockGranularity = 0`` and an initial offset aligned to
that quantum. Each fixed clock rate must remain within ±40 ppm; clock and oscillator changes
while tracking are outside the profile. With no external clock, simulation time represents the
symbol exactly. The implementation rounds the uncertainty, turnaround and maximum-PPDU
allowances outward to whole symbols and retains the future physical deadline for inclusive
receive-completion checks. These requirements do not describe arbitrary ``IClock`` models.
