# Managed native IEEE 802.15.4 PAN

After setting up the INET environment, run these commands from this directory:

```sh
inet -u Cmdenv -r 0,1             # Non-beacon association and polling
inet -u Cmdenv -c Beacon -r 0,1   # Sleeping BO3/SO1 beacon PAN
inet -u Cmdenv -c Gts -r 0,1      # Bidirectional GTS DATA
inet -u Cmdenv -c Enhanced -r 0,1 # Unacknowledged version-2 DATA
inet -u Cmdenv -c EnhancedDiscovery -r 0,1 # Enhanced active discovery
inet -u Cmdenv -c EnhancedGts -r 0,1 # Enhanced periodic beacon and GTS
```

Each pair uses typed frames and raw bytes respectively. Both peers explicitly
instantiate `Ieee802154StandardInterface` with full EUI-64 identities over the
scalar O-QPSK medium. The default configuration runs for 300 ms; Beacon and
Gts each run for four simulated seconds.

The coordinator starts PAN 0x1234 on channel 20. The device resets, actively
scans channels 11 and 20, chooses that PAN and associates. It sends acknowledged
DATA; the coordinator queues an indirect reply while the device sleeps. The
device polls after 5 ms, receives the reply, leaves, and repeats the cycle with
a new short-address allocation. The example app supplies PAN selection and
allocation policy through native services. It has no IP, ARP or 6LoWPAN adapter.

After 300 ms, the device's `completedCycles`, `receivedMsdus` and `confirmedMsdus`
scalars should each be 2. The coordinator should report two received and two
confirmed MSDUs. These counts demonstrate this configured exchange; protocol
edge cases and failures have separate fixtures under `tests/protocol/ieee802154`.
In the optional beacon-mode policy, the coordinator uses BO3/SO1; the device
passively scans, synchronizes to an actual beacon, and associates before its
DATA, poll, leave and rejoin cycles.
This configuration explicitly enables the superframe composition on the same
`Ieee802154StandardInterface`. Its two runs each completed two device cycles
with two received replies and two confirmed device MSDUs in the diagnostic
campaign. Security transforms and waveform fidelity are outside this selected
legacy unsecured profile.

The optional Enhanced configuration explicitly enables version-2 DATA at both
interfaces. The application sends it without requesting an acknowledgement;
association, polling and departure continue to use their existing procedures.
This is an unsecured DATA example; it does not request an enhanced ACK or use
enhanced management frames.
Its device sends with a short source address, so enabling
`enhancedPanIdSuppressed` is not supported by this example's address layout;
the PAN-omitted native profile requires both extended addresses and an
established peer context. The default Enhanced setting keeps PAN suppression
off. Both the default and sequence-suppressed Enhanced configurations completed
two device cycles and two DATA deliveries and confirmations per role in each
300 ms typed/raw E1-04 acceptance run.

`EnhancedDiscovery` selects an enhanced active scan at the device and enables
enhanced frames at both interfaces. Since an enhanced discovery beacon does not
itself supply the legacy PAN-coordinator flag, the app's
`enhancedCoordinatorIsPanCoordinator` parameter supplies that role as an
explicit example policy. The remainder of the association and exchange uses
the existing native procedures. The request carries the device's extended
address, and the automatic response addresses that device with the coordinator
PAN in the Destination PAN field. This example does not enable periodic
enhanced superframes.

`EnhancedGts` starts with legacy BO3/SO1 beacons for association and its pending
response. After the association response succeeds, the coordinator switches to
enhanced periodic beacons; the already tracking device waits for an actual
matching enhanced schedule before requesting GTS. This example retains the
legacy DATA request format; the protocol fixtures separately exercise
version-2 scheduled DATA and ACKs. The enhanced beacon does not carry the
legacy pending-address list, so this example does not claim association while
already tracking enhanced periodic beacons.
This example policy supports exactly one cycle.

The default-off bidirectional GTS policy waits for transmit and receive
allocation confirmations, then exchanges acknowledged DATA with the coordinator
in those reservations.
Each run completes one device cycle and one DATA delivery and confirmation per
role. The coordinator's policy timer attempts to reopen permit after each
configured grant batch; a refused attempt waits for a later batch. These
application scalars show the completed exchange; the protocol fixture also
checks the actual radio transmission times against the advertised slots.
