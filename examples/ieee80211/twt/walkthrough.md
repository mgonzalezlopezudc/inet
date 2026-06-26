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
- **Traffic**: The `server` generates downlink UDP traffic destined for `sta[0]` and `sta[1]` (200B packets sent every 2s, starting at `0.5s`).

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
scalar  TwtRegression.sta[0].wlan[0].twt  twtSleepTime               89.2467 s
scalar  TwtRegression.sta[0].wlan[0].twt  twtAwakeTime               10.7533 s

IndividualAnnounced-#0.sca:
scalar  TwtRegression.ap.wlan[0].twt      twtAgreementCount          2
scalar  TwtRegression.sta[0].wlan[0].twt  twtAgreementCount          1
scalar  TwtRegression.sta[0].wlan[0].twt  twtSleepTime               89.2467 s
scalar  TwtRegression.sta[0].wlan[0].twt  twtAwakeTime               10.7533 s

Broadcast-#0.sca:
scalar  TwtRegression.ap.wlan[0].twt      twtBroadcastScheduleCount  1
scalar  TwtRegression.sta[0].wlan[0].twt  twtBroadcastScheduleCount  1
scalar  TwtRegression.sta[0].wlan[0].twt  twtSleepTime               85.6826 s
scalar  TwtRegression.sta[0].wlan[0].twt  twtAwakeTime               14.3174 s
```

### Interpretation of Results

1. **Agreement Counts**:
   - In `Baseline`, no TWT agreements are formed.
   - In `IndividualUnannounced` and `IndividualAnnounced`, the AP negotiates **2 agreements** (1 with each STA), and both STAs report `twtAgreementCount = 1`.
   - In `Broadcast`, the AP and STAs register **1 Broadcast Schedule** (`twtBroadcastScheduleCount = 1`).

2. **Sleep Duration**:
   - In `Baseline`, the stations sleep for **0 seconds** because TWT is disabled.
   - In both `Individual` configs, the stations sleep for **~89.25 seconds** out of the 100-second simulation (spending only about 10.75s awake).
   - In the `Broadcast` config, the stations sleep for **~85.68 seconds** (spending about 14.32s awake).
   - *Why do they sleep so much?* The UDP downlink application generates a packet every 2 seconds (`sendInterval = 2000ms`), which is much slower than the TWT wake interval (100ms). This allows the stations to remain in a low-power sleep state for the vast majority of the time, waking up only during their negotiated TWT service periods (SPs) to receive packets.
   - *Why is Broadcast sleep time slightly shorter than Individual?* In Broadcast TWT, stations wake up at the start of the broadcast SP. If they miss Beacons due to sleeping, the broadcast schedule can expire and they must wake up, listen for a Beacon to resynchronize, and then return to sleep. This synchronization overhead results in slightly higher awake times (~14.3s vs ~10.7s in Individual configs).
   - This simulation clearly demonstrates the massive energy-saving benefits of TWT, especially in low-duty-cycle traffic scenarios (like IoT sensors or background updates) where stations can sleep for most of the time.
