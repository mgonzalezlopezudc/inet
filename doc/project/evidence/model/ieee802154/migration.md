# Native narrowband cutover

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [integration.md](integration.md), [external-validation.md](external-validation.md)

The public Ieee802154NarrowbandInterface becomes the native standard composition.
Ieee802154StandardInterface remains its explicit equivalent. The old module body
is retained unchanged under Ieee802154LegacyNarrowbandInterface. The legacy UWB
composition and old standalone MAC/radio types retain their behavior.

The native model supplies a full locally administered EUI-64 from the interface
component ID; explicit canonical identities remain available. It requires native
MCPS tags or typed service calls and cannot consume generic 48-bit MacAddress/ARP traffic.
There is no silent conversion, automatic MTU fragmentation or protocol guessing.
Existing IPv4 showcases deliberately select the explicit legacy type pending
their own upper-layer adaptation. SensorNode keeps its named narrowband default;
the sensor-network showcases already override it with WirelessInterface.

The consumer inventory is SensorNode's default, the IEEE 802.15.4 IPv4 showcase,
the coexistence showcase's wpanHost interfaces, and sensor-network NED instances
whose INI files already select other MACs. The generic wireless NIC examples use
the separately retained old standalone MAC; they do not instantiate the changed
narrowband interface. Stored fingerprints and SelfDoc are evidence, not source
configuration to rewrite. Local user configurations must follow the migration
guide or explicitly select legacy operation.

The cutover is a breaking change targeted at INET 5.0. The explicit legacy
narrowband composition is retained and deprecated in that release. Removal is
no earlier than INET 6.0, after inventoried consumers migrate and regression gates
pass. This sets a concrete schedule without renumbering this development branch.

The example client owns PAN selection, address allocation and application
orchestration. Each node binds to its local native MAC; the coordinator starts a
PAN, admits a device and queues replies. The device resets, scans, selects a PAN,
associates, sends DATA, sleeps, polls, leaves and repeats. A reusable message
owns only higher-layer delayed actions; protocol timing remains in the MAC.
The example app belongs to the Ieee802154 feature, so disabling it removes the
app and protocol together. Proposed validation runs this example through the
public default interface and checks two completed cycles plus actual replies.

Pre-write checks covered the current interface bodies, native tag/role consumers,
feature graph and source protection. Migration preserves old example packet
trajectories by explicit selection; fingerprint expectations are not refreshed.
