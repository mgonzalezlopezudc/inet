# TWT regression scenarios

`Baseline`, `IndividualUnannounced`, `IndividualUnannounced1ms`,
`IndividualUnannounced5ms`, `IndividualUnannounced50ms`,
`IndividualAnnounced`, and `Broadcast` use identical offered traffic and seed.
The two periodic sources are phased 5 ms apart and use a 2.011 s interval, so
packets are neither released simultaneously nor locked to one phase of the
100 ms individual or Broadcast TWT schedules.
Compare the STA `twtAwakeTime` and `twtSleepTime` scalars with delivered-packet
scalars and the energy storage's residual-capacity scalar under the same seven
configurations.
