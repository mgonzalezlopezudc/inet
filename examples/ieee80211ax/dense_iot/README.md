# Dense IoT: 802.11ax OFDMA and TWT versus 802.11ac

This example compares a Wi-Fi 6 treatment with a matched Wi-Fi 5 baseline in
one dense infrastructure BSS. A wired server communicates through one AP with
128, 256, or 512 stationary IoT stations.

The three workloads are:

- `UL`: every STA sends one 100-byte UDP payload per second.
- `DL`: the server sends one 100-byte UDP payload per second to every STA.
- `Mixed`: every STA sends once per second and receives once per 10 seconds.

Each source receives an independently randomized phase within its period. The
STA agents also start across a common randomized 0–15 s window so association
does not become a synchronized 512-device burst. Traffic starts during warm-up,
so association, Block Ack setup, and initial rate selection do not become part
of the 20–120 s performance interval.

## Compared technologies

The `Ax*` configurations use 802.11ax, HE Minstrel rate control, downlink and
trigger-based uplink OFDMA, and negotiated individual implicit unannounced TWT.
The configured TWT interval is 100 ms and the nominal awake duration is 5 ms.
The AP-selected first wake occurs 20 ms after each independently timed setup
exchange, so randomized association also distributes the individual service-
period phases. The TWT frame's 256 microsecond duration unit quantizes the
negotiated duration to 5.12 ms. DL MU transmissions use sequential BAR/Block
Ack responses, keeping the one-antenna comparison within its configured
spatial-stream limit. A lost setup exchange is retried every 100 ms, up to 50
attempts, because a one-shot management request is not reliable in a dense BSS.

The AP considers an uplink Trigger exchange every millisecond, while a 10 ms
minimum interval between transmitted Trigger frames prevents empty buffer-status
polls from monopolizing the channel when most unannounced-TWT STAs are asleep.

The `Ac*` configurations use 802.11ac, single-user EDCA, one-stream AARF rate
control, and no TWT. Both technologies use the same placement, application
load, one-antenna radios, 5 GHz 20 MHz channel, transmit power, receiver
thresholds, and radio power-consumption parameters.

Separate random-number streams isolate placement, association timing, and
application phase selection. Thus an AX/AC pair with the same repetition uses
the same station coordinates and offered-load phases even though the MACs
consume different random numbers internally.

Station positions are uniformly distributed in a 360 m square centered on the
AP. Consequently, links span a broad path-loss/SNR range. The recorded HE MCS
vectors provide direct evidence of the selected MCS range; coordinates alone
must not be treated as proof of a transmitted MCS.

## Configurations and runs

| Configuration | Technology | Workload |
|---|---|---|
| `AxUl` / `AcUl` | AX / AC | uplink |
| `AxDl` / `AcDl` | AX / AC | downlink |
| `AxMixed` / `AcMixed` | AX / AC | mixed uplink and downlink |

Every configuration contains three station-count iterations and five
independent repetitions, or 15 runs. The full comparison is therefore 90 runs.
Run it from the repository root with:

```sh
python3 examples/ieee80211ax/dense_iot/run_campaign.py
```

Dense 512-STA runs are memory intensive, so the runner is serial by default.
Use `-j N` only when the machine has enough memory. A single condition can be
selected, for example:

```sh
python3 examples/ieee80211ax/dense_iot/run_campaign.py --config AxUl --run 0
```

## Metrics and analysis

Generate the CSV summaries and dashboard with the native OMNeT++ result API:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211ax/dense_iot/analyze.py
```

The analysis uses these definitions:

- Goodput is delivered application payload bits divided by the 100-second
  measurement interval.
- Delay is computed from application `endToEndDelay` samples. The dashboard
  shows the mean across runs of each run's 95th percentile, with a 95%
  Student-t confidence interval across the five independent runs.
- Energy is `1000 J - final residual energy`, averaged over all STAs within a
  run. Reduction is paired by workload, station count, and repetition as
  `1 - AX / AC` before computing its confidence interval.
- Mixed UL and DL observations remain separate in `per_run_performance.csv`;
  the dashboard's Mixed row aggregates delivered bytes and pools delivered
  packet delays within each run.

Always inspect delivery ratio together with energy reduction. Sleeping through
traffic or failing to associate is not an energy-efficiency improvement.
