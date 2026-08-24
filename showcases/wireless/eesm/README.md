# TGn + HT EESM showcase

This showcase wires the packet-level BCC SISO HT EESM model to the
TGn per-carrier channel implementation. `omnetpp.ini` defines four locally
runnable configs when the ignored local artifacts are present: `D20`, `D40`,
`E20`, and `E40`. The suffix is the HT bandwidth in MHz;
D/E are TGn profile labels, not distances. All configs use occupied
subcarriers, one transmit/receive antenna, and a static channel response for
the Data interval. Temporal, vehicle, fluorescent, and large-scale shadowing
effects are disabled. Model E enables the calibration-only whole-profile
ensemble normalization option.

When the ignored local inputs are present, the four configurations are locally
runnable. `local/` is intentionally gitignored and is not a distributable
artifact directory. It must contain the exact user-supplied
`user-supplied-bcc-awgn-per-v1.csv` (SHA-256
`364c87c3e129876f7782b312fc7a7c5b69b23c96e542ecadf113be23129cd608`), the
four D20/D40/E20/E40 manifests, and
`patidar2017-bcc-beta-v1.csv` (SHA-256
`ecf940a8e6439975de39a33cf60ee56447d473311e13b162e30962035fc8707d`) and
its `patidar2017-bcc-beta-v1.json` metadata sidecar.
Each manifest binds those checksums. The AWGN artifact is the exact BCC table
supplied directly by the project user. HT40 is an exact duplicate of HT20 by
explicit user assumption. Raw packet and decode counts, SNR normalization,
provider/generator provenance, and a redistribution grant are unavailable or
not granted; the manifests make those limitations and the
`userAuthorizedLocal` scope explicit. The beta artifact is a locally reviewed
publication transcription, approved here only for local evaluation;
independent clean-room verification and redistribution rights are not claimed.

The paper describes Model D at 10 m as NLOS, while the TGn inclusive breakpoint
rule makes exactly 10 m LOS for Model D. These configs keep D explicitly NLOS
and record that boundary ambiguity rather than silently changing the campaign
meaning. Model E is configured LOS.

EESM samples Data centers only: 52 carriers for HT20 and 108 for HT40, while
transmit power is allocated over 56 and 114 total transmit carriers. Packet
outcomes produced by INET are Bernoulli samples from the same EESM lookup and
are therefore circular for independent beta/Table-4 validation. This is an
integration showcase, not an independent reproduction of the paper's Table 4.

## Mutual-information alternatives

The receiver also provides three opt-in mutual-information effective-SNR
policies through the same `errorModel.typename` slot:

- `Ieee80211MiesmErrorModel` uses normalized symbol-constrained mutual
  information.
- `Ieee80211RbirErrorModel` is the public RBIR name for that same mapping. For
  fixed-modulation SISO HT, normalizing symbol mutual information by the bits
  per symbol cancels during inversion, so MIESM and RBIR are algebraically
  identical here.
- `Ieee80211MmibErrorModel` averages the exact log-MAP mutual information of
  the authoritative IEEE Gray-labelled bit channels.

All three consume linear per-carrier Es/N0 and apply
`gammaEffective = beta * F^-1(mean(F(gammaCarrier / beta)))`. They then use
the existing MCS- and PSDU-length-specific AWGN PER table. The HT mode remains
authoritative for the BPSK, QPSK, 16-QAM, or 64-QAM constellation and bit
labelling; the error models do not duplicate an MCS-to-modulation table.

The MI `beta` is an explicit, dimensionless, linear-domain calibration
parameter. It has no usable default: selecting an MI policy without a finite,
positive value fails during initialization. Existing EESM beta artifacts must
not be reused. `beta = 1` selects an uncalibrated reference mapping suitable
for numerical and integration tests, not a production calibration.

To use an MI policy, replace the complete EESM error-model parameter block
(including removal of `betaTableFile` and `calibrationSet`) with, for example:

```ini
*.destinationHost.wlan[0].radio.receiver.errorModel.typename = "Ieee80211MmibErrorModel"
*.destinationHost.wlan[0].radio.receiver.errorModel.beta = 1  # uncalibrated example
*.destinationHost.wlan[0].radio.receiver.errorModel.artifactAcceptanceMode = "userAuthorizedLocal"
*.destinationHost.wlan[0].radio.receiver.errorModel.perTableFile = "local/user-supplied-bcc-awgn-per-v1.csv"
*.destinationHost.wlan[0].radio.receiver.errorModel.perTableManifest = readJSON("local/user-supplied-bcc-awgn-per-v1.d20.manifest.json")
```

The supported receiver scope remains BCC SISO HT MCS 0-7 at 20 or 40 MHz,
with 52 or 108 static Data-carrier SNIR values and WHOLE-packet corruption.
MIESM, RBIR, and MMIB are link-abstraction policies, not normative IEEE 802.11
procedures. The checked numerical oracles establish the mapping implementation;
packet delivery in this showcase would establish wiring only. Mapping-specific
beta calibration and independent decoder validation remain future work.

Run from this directory with matching debug artifacts:

```sh
../../../bin/inet --debug -u Cmdenv -f omnetpp.ini -c D20 --cmdenv-express-mode=true --cmdenv-log-level=OFF
../../../bin/inet --debug -u Cmdenv -f omnetpp.ini -c D40 --cmdenv-express-mode=true --cmdenv-log-level=OFF
../../../bin/inet --debug -u Cmdenv -f omnetpp.ini -c E20 --cmdenv-express-mode=true --cmdenv-log-level=OFF
../../../bin/inet --debug -u Cmdenv -f omnetpp.ini -c E40 --cmdenv-express-mode=true --cmdenv-log-level=OFF
```

Each configuration uses `seed-set = 11` and writes an ignored scalar file to
`results/<config>-#0.sca`. Packet delivery and radio PER statistics in those
files demonstrate runtime wiring only; they must not be interpreted as
independent decoder accuracy.
