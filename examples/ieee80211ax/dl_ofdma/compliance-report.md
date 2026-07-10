# MU-BAR Response Timing and Recovery

The AP waits for the solicited HE TB Block Ack responses after transmitting an
HE MU-BAR Trigger. It may start another HE MU PPDU only after either collecting
the responses or reaching the response deadline and contending for the channel
again.

The response duration and deadline are derived from the exact triggered RU,
guard interval, and coding parameters. The MU-BAR Trigger carries those common
PHY parameters to the stations, and each station copies them into its HE TB
Block Ack request. This is important for 484-tone and wider responses, where
BCC is illegal and the response must use LDPC.

The current deterministic verification runs exercise both successful response
collection and timeout recovery:

- `EqualSizedRUs_fBW` contains two modeled PHY losses of the MU-BAR Trigger at
  `host[2]`. In both cases the AP reaches the response deadline, reports the
  missing Block Ack, requeues the MPDU, ends the TXOP, and later delivers the
  retried packet. The other four verification runs have no response timeout.
- `WideBandwidth80MHz` completes 84 eight-user response exchanges and accepts
  672 HE TB Block Acks. Its 85th HE MU PPDU reaches the simulation time limit
  before the response exchange completes.
- No runtime error or undisposed object is reported.

If a Trigger or response is genuinely lost, the recovery path still waits for
`SIFS + common response duration + slot time`, requeues unacknowledged MPDUs,
ends the TXOP, and performs EDCA contention before retrying.

Scope note: this verifies the packet-level MAC/PHY timing and recovery model. It
does not claim bit-exact HE-SIG or Trigger-frame interoperability.
