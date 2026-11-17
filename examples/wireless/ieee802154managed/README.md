# Managed native IEEE 802.15.4 PAN

Run `inet -u Cmdenv -r 0,1` from this directory. The two runs use typed frames
and raw bytes respectively. Both explicitly instantiate Ieee802154StandardInterface
with explicit full EUI-64 identities over the scalar O-QPSK medium.

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
Periodic beaconing, security transforms, enhanced frames and waveform fidelity
are outside this selected legacy unsecured profile.
