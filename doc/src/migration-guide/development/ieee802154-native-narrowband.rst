Native IEEE 802.15.4 Narrowband Interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``Ieee802154NarrowbandInterface`` now selects the native legacy-frame O-QPSK
model, also available as ``Ieee802154StandardInterface``. This breaking cutover
is targeted at INET 5.0. To preserve a pre-cutover model, select
``Ieee802154LegacyNarrowbandInterface`` explicitly. It retains the old parameters,
48-bit upper-layer contract, queue and legacy wire representation. That type is
deprecated for 5.0 and will not be removed before 6.0, after consumer migration
and regression validation. The separate legacy UWB interface is retained.

Preserving Existing IPv4 Models
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Change the interface selection explicitly:

.. code-block:: ini

   **.wlan[*].typename = "Ieee802154LegacyNarrowbandInterface"

The IEEE 802.15.4 and coexistence IPv4 showcases already make this selection.
The sensor-network showcase selects other wireless MACs and is unaffected.
Stored legacy captures are still decoded as their original format; they are
never reinterpreted as standard MPDUs.

Moving to Native Services
^^^^^^^^^^^^^^^^^^^^^^^^

Each interface defaults to a locally administered EUI-64 derived from its simulator
component ID. For identities stable across topology edits, configure a full EUI-64, for example
``extendedAddress = "extended:02-01-12-34-56-78-9A-BC"``. Configure ``panId`` and
``shortAddress`` at the interface or obtain them through management. The native
interface has no generic ``MacAddress``/ARP adaptation and does not truncate,
widen or silently map addresses. Existing IP applications therefore require an
explicit upper-layer adaptation before selecting the native interface.

Submit opaque MSDUs through ``IIeee802154MacServiceProvider`` or attach an
``Ieee802154MacDataReq`` tag before sending through the upper gate. The adapter
returns explicit native admission/confirmation/indication tags. Set receiver-side
``upperProtocol`` only when a single upper protocol is known; there is no wire
``networkProtocol`` field. No sender protocol tag is trusted on reception.

The complete MPDU, including native addressing and FCS, must fit 127 bytes.
There is no fixed legacy header-size subtraction or automatic fragmentation;
the permissible MSDU length depends on addressing and selected fields. The
native MAC replaces old generic queue/backoff/ACK settings with typed service
requests and PIB attributes such as ``macMinBe``, ``macMaxCsmaBackoffs`` and
``macMaxFrameRetries``. Sleep is the default idle receiver policy; use
``rxOnWhenIdle`` or the management receive-window service deliberately.

Enable ``enableManagement`` and bind ``managementClientModule`` for START, scans,
association, indirect DATA/POLL/PURGE, disassociation, orphan recovery, PAN
conflict and scheduled receiver control. PAN choice and address allocation are
higher-layer policy. ``examples/wireless/ieee802154managed`` demonstrates two
complete discover/join/DATA/sleep/POLL/leave/rejoin cycles with native clients.

The default model is a packet-level selected unsecured legacy profile, not
whole-standard or waveform support. It does not add periodic beacons, slotted
access, GTS, enhanced frames/IEs, security transforms, TSCH, DSME or modern UWB.
Native frames and old legacy frames are wire-incompatible. New native timing,
real MPDU/FCS lengths, channel access and real ACK exchanges intentionally change
trajectories; legacy configurations retain their original behavior through the
explicit legacy type.
