In this simulation the AP does not wait to receive the STAs ACKs after sending the HE-MU-BAR-Trigger at event #1003 before sending the next HE-MU-PPDU at event #1425. Is this standard-compliant?

Yes, the behavior in the simulation is **fully standard-compliant**. 

The AP **did wait** for the STAs' BlockAck responses (uplink HE TB PPDUs). However, because the STAs dropped the trigger frame due to bit errors, no responses were sent. The AP waited for the full standard-compliant timeout duration, declared a transmission failure, ended the TXOP, and then performed a standard EDCA backoff contention before transmitting the next `HE-MU-PPDU`.

Here is the exact microsecond-by-microsecond breakdown of the events in [compliance-sim.log](examples/ieee80211/ofdma/compliance-sim.log):

### 1. Trigger Frame Transmission (Event #1003)
* **Start Time (`t = 0.502713836666 s`)**: The AP begins transmitting the `HE-MU-BAR-Trigger` frame.
* **End Time (`t = 0.502765836666 s`)**: Transmission finishes (duration: 52 µs).
* **Expected Response**: The trigger frame solicits immediate BlockAck responses from the STAs in an uplink HE TB PPDU. The expected duration (`commonDuration`) of this response PPDU is calculated by the AP to be **460 µs** (Event #17198).
* **Duration Field**: The AP sets the frame's Duration field to `SIFS (16 µs) + commonDuration (460 µs) = 476 µs`. Non-addressed STAs (like `host[2]`) receive the trigger and set their NAV to 476 µs (Event #1010), protecting the medium until `t = 0.503242072531 s` (Event #1143).

### 2. Waiting and Response Timeout (Event #1144)
* **Timeout Calculation**: As implemented in [HeDlMuTxOpFs.cc](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc#L400-L401), the AP schedules a `ReceiveCollectionStep` with a timeout of:
  $$\text{Timeout} = \text{SIFS} + \text{commonDuration} + \text{SlotTime} = 16\ \mu\text{s} + 460\ \mu\text{s} + 9\ \mu\text{s} = 485\ \mu\text{s}$$
* **Deadline Expiration (`t = 0.503250836666 s`)**: The receive collection timer (`startRxTimeout`) fires:
  ```
  [INFO]  HeFrameSequenceHandler: receive collection deadline reached.
  [WARN]  HeDlMuTxOpFs: MU-BAR response timeout for STA 0A-AA-00-00-00-01
  [WARN]  HeDlMuTxOpFs: MU-BAR response timeout for STA 0A-AA-00-00-00-02
  ```
  Since `host[0]` and `host[1]` dropped the trigger frame due to bit errors (Events #1011-#1014), the AP received nothing. It increments the short retry counters, re-queues the failed packets, and ends the TXOP.

### 3. Channel Contention (Event #1397 to #1422)
* **NAV Expiration (`t = 0.504521836666 s`)**: In INET, the AP's RX module sets an internal NAV timer based on the duration of its own transmitted frame sequence. Once this NAV clears (Event #1397), the AP immediately initiates a new contention procedure.
* **Backoff Duration**: The AP selects a random backoff of **7 slots** for the Voice Access Category (AC_VO). The contention wait interval is:
  $$\text{Wait Interval} = \text{AIFS} + (\text{backoff slots} \times \text{SlotTime}) = 34\ \mu\text{s} + (7 \times 9\ \mu\text{s}) = 97\ \mu\text{s}$$
* **Access Granted (`t = 0.504618836666 s`)**: At $0.504521836666\text{ s} + 97\ \mu\text{s}$, the contention backoff timer expires, and channel access is granted (Event #1422).

### 4. Next Transmission (Event #1425)
* **Start Time (`t = 0.504618836666 s`)**: The AP starts transmitting the next `HE-MU-PPDU` (Event #1425).

---

### Conclusion
The AP **did wait** for the STAs' responses. It only proceeded to transmit the next packet because the response collection timer expired without receiving any ACKs, and it successfully contended for the channel again. This behavior aligns precisely with the IEEE 802.11ax standard rules for transmission failure recovery.