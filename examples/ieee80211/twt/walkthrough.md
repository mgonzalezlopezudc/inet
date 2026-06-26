# 802.11ax HE Target Wake Time (TWT) Simulation

This example illustrates the Target Wake Time (TWT) power-saving mechanism introduced in the IEEE 802.11ax (Wi-Fi 6) standard. It demonstrates how TWT agreements are negotiated between an Access Point (AP) and stations (STAs), and how individual and broadcast TWT configurations allow stations to sleep, reducing power consumption.

## Background: HE Target Wake Time (TWT)

In legacy 802.11 power-saving modes (like PS-Poll or APSD), stations wake up periodically to receive Beacons and check if the AP has buffered downlink traffic. In high-density networks, this causes all sleeping stations to wake up simultaneously, leading to high collision rates and increased energy waste.

802.11ax addresses this with **Target Wake Time (TWT)**:
1. **Scheduled Wakeup**: Instead of waking up for every Beacon, the AP and each STA negotiate specific, customized time windows called **TWT sessions** (defined by `wakeInterval` and `wakeDuration`). The STA remains in a deep sleep state outside of these sessions.
2. **Individual TWT**:
   - Negotiated dynamically via TWT Setup frames.
   - **Unannounced TWT**: The STA wakes up at the TWT start time and can immediately transmit uplink frames or receive downlink frames without waiting for a poll frame.
   - **Announced TWT**: The STA wakes up but must remain quiet (listening) until the AP polls it (e.g., with a trigger frame or downlink data frame) to confirm it is ready.
3. **Broadcast TWT**:
   - The AP defines and broadcasts a shared TWT schedule (in Beacons or Association responses).
   - Multiple stations join this shared broadcast schedule, allowing the AP to coordinate groups of STAs together (highly useful for Downlink/Uplink MU-OFDMA).

---

## Network Topology

The network [TwtRegression.ned](TwtRegression.ned) consists of:
- **`ap`**: An Access Point located at `(300, 180)`.
- **`sta[0..1]`**: Two wireless stations located at `(170, 130)` and `(170, 230)` (approx. 140 meters from the AP).
- **`server`**: A wired server connected to the AP.
- **Traffic**: The `server` generates downlink UDP traffic destined for `sta[0]` and `sta[1]` (200B packets sent every 5ms, starting at `0.5s`).

```
       [sta[0]] (Color 1)
          |
          | (140m wireless)
          v
       [ ap ] <==== (100M Ethernet) ====> [server]
          ^
          | (140m wireless)
          |
       [sta[1]] (Color 2)
```

---

## Configurations in `omnetpp.ini`

The [omnetpp.ini](omnetpp.ini) file defines four TWT scenarios:

### 1. `Baseline`
- TWT is disabled: `**.wlan[*].twt.enabled = false`.
- Stations do not sleep and remain awake for the entire duration of the simulation.

### 2. `IndividualUnannounced`
- TWT is enabled: `**.wlan[*].twt.enabled = true`.
- Stations negotiate an **Individual, Unannounced** TWT schedule:
  - `*.sta[*].wlan[*].agent.requestBroadcast = false`
  - `*.sta[*].wlan[*].agent.announced = false`
  - Wake interval is set to 100ms (`wakeInterval = 100ms`) and wake duration is 20ms (`wakeDuration = 20ms`).
- **Result**: Stations enter a low-power sleep state outside their wake windows.

### 3. `IndividualAnnounced`
- Extends the `IndividualUnannounced` configuration but sets `announced = true`.
- **Result**: The stations must wait for the AP to poll them after waking up before exchanging data.

### 4. `Broadcast`
- Stations join a **Broadcast** TWT schedule created by the AP:
  - `*.ap.wlan[*].mgmt.createBroadcastSchedule = true` and `*.ap.wlan[*].mgmt.broadcastId = 1`
  - `*.sta[*].wlan[*].agent.requestBroadcast = true` and `*.sta[*].wlan[*].agent.broadcastId = 1`

---

## Running the Simulation

Ensure your environment is set up, then run the configurations.

### Running with Qtenv (GUI)
```sh
source $HOME/omnetpp-6.4.0aipre2/setenv && source $HOME/omnetpp_ws/inet/setenv
opp_run -u Qtenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c IndividualUnannounced examples/ieee80211/twt/omnetpp.ini
```

### Running with Cmdenv (Command Line)
```sh
source $HOME/omnetpp-6.4.0aipre2/setenv && source $HOME/omnetpp_ws/inet/setenv

opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c Baseline examples/ieee80211/twt/omnetpp.ini
opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c IndividualUnannounced examples/ieee80211/twt/omnetpp.ini
opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c IndividualAnnounced examples/ieee80211/twt/omnetpp.ini
opp_run -u Cmdenv --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples -l $HOME/omnetpp_ws/inet/src/libINET.so -c Broadcast examples/ieee80211/twt/omnetpp.ini
```

---

## Verifying Results

After running the simulations, use `opp_scavetool` to extract TWT agreement counts and sleep time statistics for each station.

```sh
# Query TWT agreements, schedules, awake time, and sleep time
opp_scavetool query -l -f 'name =~ "twtAgreementCount" or name =~ "twtBroadcastScheduleCount" or name =~ "twtAwakeTime" or name =~ "twtSleepTime"' examples/ieee80211/twt/results/*.sca
```

### Expected Output Summary

```
Baseline-#0.sca:
scalar  TwtRegression.ap.wlan[0].twt      twtAgreementCount          0
scalar  TwtRegression.sta[0].wlan[0].twt  twtSleepTime               0

IndividualUnannounced-#0.sca:
scalar  TwtRegression.ap.wlan[0].twt      twtAgreementCount          2
scalar  TwtRegression.sta[0].wlan[0].twt  twtAgreementCount          1
scalar  TwtRegression.sta[0].wlan[0].twt  twtSleepTime               0.0098 s

IndividualAnnounced-#0.sca:
scalar  TwtRegression.ap.wlan[0].twt      twtAgreementCount          2
scalar  TwtRegression.sta[0].wlan[0].twt  twtAgreementCount          1
scalar  TwtRegression.sta[0].wlan[0].twt  twtSleepTime               0.0098 s

Broadcast-#0.sca:
scalar  TwtRegression.ap.wlan[0].twt      twtBroadcastScheduleCount  1
scalar  TwtRegression.sta[0].wlan[0].twt  twtBroadcastScheduleCount  1
scalar  TwtRegression.sta[0].wlan[0].twt  twtSleepTime               0
```

### Interpretation of Results

1. **Agreement Counts**:
   - In `Baseline`, no TWT agreements are formed.
   - In `IndividualUnannounced` and `IndividualAnnounced`, the AP negotiates **2 agreements** (1 with each STA), and both STAs report `twtAgreementCount = 1`.
   - In `Broadcast`, the AP and STAs register **1 Broadcast Schedule** (`twtBroadcastScheduleCount = 1`).

2. **Sleep Duration**:
   - In `Baseline` and `Broadcast`, the stations sleep for **0 seconds**.
   - In both `Individual` configs, the stations sleep for **0.0098 seconds** (approx. 9.8 ms).
   - *Why is the sleep time so short?* The UDP downlink application generates a packet every 5ms, which is much faster than the 100ms TWT wake interval. During the active traffic phase (from 0.5s to 2s), there is always downlink data queued at the AP, forcing the stations to remain awake to clear the queue backlog. The 9.8 ms sleep time represents the brief period before traffic started at 0.5s when the stations entered sleep mode immediately after completing association and TWT setup.
   - To maximize energy savings in real-world deployments, TWT is best matched with low-duty-cycle traffic (e.g., IoT sensors reporting data every few minutes) rather than continuous high-speed streaming.
