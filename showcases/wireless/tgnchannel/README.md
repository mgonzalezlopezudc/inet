# TGn channel showcase

This showcase selects the IEEE 802.11 TGn A–F wideband channel, with time
variation enabled in the base configuration. The configurations are:

- `ModelA`–`ModelF`: TGn Appendix C profiles A through F, using the default 2×1
  selected-column/MRC packet path.
- `Siso`: one transmit and one receive antenna for a scalar channel response.
- `Matrix2x2`: two transmit and two receive antennas for the matrix path.
- `StaticDiagnostic`: deterministic, time-invariant response generation only.

The propagation model comes from the nonnormative IEEE 802.11-03/940r4,
*TGn Channel Models*, May 10, 2004. The Rx-row/Tx-column baseband matrix
convention follows IEEE Std 802.11-2024, Clause 19.3.12.1; compiling this model
does not select it or alter legacy operation modes.

The channel snapshot is a complete receive-row/transmit-column complex matrix.
The current receiver policy sends one stream through selected transmit column
0 and applies ideal MRC across the receive rows. It is not spatial
multiplexing, beamforming, or an MRC packet-error calibration.

The configured `Ieee80211NistErrorModel` remains INET's ordinary scalar packet
error policy. Packet results demonstrate integration only; they are not a TGn
PER oracle. Inspect matrix responses directly, or use the statistical unit
tests, for channel validation.

Run a fixed-seed channel example from this directory with:

```sh
../../../bin/inet --debug -u Cmdenv -c ModelD -f omnetpp.ini
```

Generate the diagnostic fixed-time frequency response, absolute-time response,
and 4x4 Table III capacity CDF with:

```sh
./generate-artifacts
```

The command writes `results/tgn-frequency-response.csv`,
`results/tgn-time-evolution.csv`, and `results/tgn-capacity-cdf.csv`. If
`gnuplot` is installed, it also writes the corresponding PNG plots. The first
two files exercise the deterministic wideband/absolute-time evaluator; the CDF
uses Models A--F, 20 fixed-seed batches of 2000 realizations, 10 dB SNR, and the
same acceptance oracle as `TgnMimoChannelStatistics_1.test`. Observation and
plot generation evaluate already-created values and consume no channel RNG.
