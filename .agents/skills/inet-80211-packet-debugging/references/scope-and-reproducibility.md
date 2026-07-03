## Contents

* 1. Required Inputs
* 2. Establish the Model’s Actual 802.11 Scope
* 2.1 Identify the installed versions
* 2.2 Inspect the instantiated interface
* 2.3 Build a feature-support matrix
* 3. Preserve Reproducibility

---

# 1. Required Inputs

Reuse these skills: 
- `inet-simulation-run` to validate the simulation environment, locate the INET library, and run the simulation with Cmdenv.
- `inet-pcap-tshark-analysis` to extract and analyze PCAPng captures with TShark.

Determine or discover:

* OMNeT++ version
* INET version and commit
* Simulation executable, usually `opp_run_dbg`
* INET debug library path
* NED paths
* `omnetpp.ini` path
* Configuration name
* Run number and repetition
* Random seed or seed-set
* Sender and receiver module paths
* AP and intermediate-node paths
* Wireless-interface indices
* SSID and BSSID
* STA and AP MAC addresses
* Radio-medium module path
* Radio type and mode set
* Center frequency and channel bandwidth
* Transmit power
* Receiver sensitivity and energy-detection threshold
* Background-noise and interference models
* Propagation, path-loss, fading, obstacle-loss, and antenna models
* MAC mode: DCF, QoS/EDCA, ad hoc, infrastructure, mesh, or another mode
* Data rate, MCS, or rate-control algorithm
* RTS threshold
* Fragmentation threshold
* ACK and Block Ack policies
* Retry limits
* Aggregation policies
* Traffic class or TID
* Simulation-time interval containing the failure
* Packet name, protocol, address tuple, or sequence number identifying the flow

---

# 2. Establish the Model’s Actual 802.11 Scope

## 2.1 Identify the installed versions

Inspect the INET repository:

```sh
git -C "$INET_ROOT" describe --always --dirty
git -C "$INET_ROOT" status --short
git -C "$INET_ROOT" log -1 --oneline
```

Check release metadata:

```sh
find "$INET_ROOT" -maxdepth 2 \
  \( -iname '*version*' -o -iname '*changelog*' -o -iname '*whatsnew*' \) \
  -print
```

Record the exact version and commit in the diagnosis.

## 2.2 Inspect the instantiated interface

Locate the interface type:

```sh
rg -n 'Ieee80211Interface|Ieee80211Nic|WirelessHost|AccessPoint|AdhocHost' \
  /path/to/project "$INET_ROOT/src"
```

Inspect the NED definitions and configuration overrides:

```sh
rg -n 'wlan|radioMedium|qosStation|mgmt|agent|radio|mac' \
  /path/to/project/*.ini \
  /path/to/project \
  "$INET_ROOT/src/inet/linklayer/ieee80211"
```

Determine:

* MAC module type
* Management module type
* Agent module type
* Radio module type
* Radio-medium type
* QoS mode
* Mode set
* Queue type
* Classifier type
* Rate-selection and rate-control modules
* ACK, RTS, aggregation, fragmentation, and Block Ack policies

## 2.3 Build a feature-support matrix

Before diagnosing standard behavior, create a table like:

| Feature                     | Required by scenario | Present in standard | Implemented by this INET version | Enabled in configuration |
| --------------------------- | -------------------: | ------------------: | -------------------------------: | -----------------------: |
| DCF                         |                  Yes |                 Yes |                              Yes |                      Yes |
| EDCA                        |                   No |                 Yes |                              Yes |                       No |
| RTS/CTS                     |                  Yes |                 Yes |                              Yes |                      Yes |
| Block Ack                   |                  Yes |                 Yes |                              Yes |                      Yes |
| A-MSDU                      |                  Yes |                 Yes |                   Inspect source |                  Inspect |
| A-MPDU                      |                  Yes |                 Yes |                   Inspect source |                  Inspect |
| HT/VHT/HE/EHT PHY           |   Scenario-dependent |                 Yes |                   Inspect source |                  Inspect |
| OFDMA                       |   Scenario-dependent |              HE/EHT |                   Inspect source |                  Inspect |
| Multi-link operation        |   Scenario-dependent |                 EHT |                   Inspect source |                  Inspect |
| Power save                  |   Scenario-dependent |                 Yes |                   Inspect source |                  Inspect |
| Protected management frames |   Scenario-dependent |                 Yes |                   Inspect source |                  Inspect |

Treat “not implemented” differently from “implemented incorrectly.”

Never diagnose a missing frame exchange as a protocol defect until the required feature has been confirmed in the installed model.

---

# 3. Preserve Reproducibility

Create a dedicated debug configuration:

```ini
[Config WifiDebug]
extends = ExistingConfiguration
```

Do not alter the normal experiment configuration unless unavoidable.

Record:

* Configuration inheritance
* Run number
* Seed set
* Repetition number
* Command line
* Environment variables
* Source commit
* Modified files
* Generated captures
* Result files
* Log files

Run deterministic repetitions while debugging:

```sh
"$OPP_RUN" \
  -u Cmdenv \
  -f "$INI_FILE" \
  "--ned-path=$NED_PATH" \
  -l "$INET_LIBRARY" \
  -c "$CONFIG" \
  -r "$RUN"
```

Do not compare runs with different random streams unless testing sensitivity to randomness.

---

