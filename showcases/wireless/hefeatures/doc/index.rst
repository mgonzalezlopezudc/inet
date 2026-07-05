Advanced 802.11ax HE Features
=============================

Goals
-----

This showcase demonstrates several HE-specific PHY and MAC metadata paths that
are easy to miss when looking only at packet counts:

- HE LDPC coding support and per-peer capability negotiation;
- HE packet extension duration;
- HE preamble puncturing on an 80 MHz channel;
- puncture-aware RU allocation.

The scenarios all use the same downlink MU-OFDMA topology. The AP sends traffic
to four stations on an 80 MHz channel so that scheduler decisions and
preamble-puncturing constraints are visible.

| Verified with INET version: ``4.6``
| Source files location: `inet/showcases/wireless/hefeatures <https://github.com/inet-framework/inet/tree/master/showcases/wireless/hefeatures>`__

The Model
---------

The :ned:`HeFeaturesShowcase` network contains a wired server, one AP, and four
stations. The 80 MHz channel is intentional: HE preamble puncturing is defined
for 80 MHz and 160 MHz operation, because the channel is made of multiple
20 MHz subchannels.

The common configuration enables HE mode, QoS, :ned:`HeHcf`, downlink
MU-OFDMA scheduling, aggregation, Block Ack, and HE signal detail display:

.. literalinclude:: ../omnetpp.ini
   :start-at: # 802.11ax HE base configuration
   :end-at: **.heMuSignalLabelMaxUsers = 4
   :language: ini

Baseline
--------

``BccBaseline`` is the reference case: BCC coding, no packet extension, and no
preamble puncturing.

.. literalinclude:: ../omnetpp.ini
   :start-at: BccBaseline
   :end-before: HeLdpc
   :language: ini

Inspect the AP radio transmitter watches ``lastHeTransmissionSummary`` and
``lastHeUserPhyParameters``. The HE MU header should show BCC coding, zero
packet extension, and no punctured subchannel mask.

LDPC
----

``HeLdpc`` enables HE LDPC capability on the AP and all stations:

.. literalinclude:: ../omnetpp.ini
   :start-at: HeLdpc
   :end-before: PacketExtension
   :language: ini

The model uses LDPC for packet-level timing/accounting and applies the modeled
PER benefit in the error model. In Qtenv, look for ``coding = LDPC`` in the HE
signal details and compare PPDU duration/user PHY parameters with the baseline.

``MixedLdpcSupport`` leaves one station without HE LDPC support:

.. literalinclude:: ../omnetpp.ini
   :start-at: MixedLdpcSupport
   :end-before: CombinedHeFeatures
   :language: ini

This demonstrates capability negotiation. A MU frame that includes a station
without negotiated LDPC support must fall back to BCC. The useful observation is
that capability is per peer, but the selected coding mode applies to the whole
scheduled MU frame.

Packet Extension
----------------

``PacketExtension`` configures an 8 us HE packet extension:

.. literalinclude:: ../omnetpp.ini
   :start-at: PacketExtension
   :end-before: PreamblePuncturing
   :language: ini

The HE MU PHY header should carry ``packetExtensionDurationUs = 8`` and the PPDU
duration should include the extension. This is mostly a metadata and timing
showcase: packet counts alone are not the right primary observation.

Preamble Puncturing
-------------------

``PreamblePuncturing`` disables one 20 MHz subchannel inside the 80 MHz channel:

.. literalinclude:: ../omnetpp.ini
   :start-at: PreamblePuncturing
   :end-before: MixedLdpcSupport
   :language: ini

The AP validates that puncturing is only used on a suitable channel width and
that the primary 20 MHz subchannel remains active. The scheduler must then avoid
placing RUs on the disabled subchannel. In Qtenv, inspect
``lastRuAllocations`` on the AP scheduler and ``puncturedSubchannelMask`` in the
HE MU signal details. Large RUs may be downgraded when they would overlap the
punctured subchannel.

Combined Features
-----------------

``CombinedHeFeatures`` enables LDPC, 8 us packet extension, and preamble
puncturing together:

.. literalinclude:: ../omnetpp.ini
   :start-at: CombinedHeFeatures
   :language: ini

Use this run to confirm that the metadata is carried together through the HE MU
header and that the RU layout still avoids the punctured subchannel.

What To Observe
---------------

For this showcase, Qtenv watches and HE signal labels are more educational than
aggregate throughput. In particular, inspect:

- AP ``wlan[0].mac.hcf.dlScheduler``: ``lastScheduleSummary`` and
  ``lastRuAllocations``;
- AP radio transmitter: ``lastHeTransmissionSummary`` and
  ``lastHeUserPhyParameters``;
- AP MIB: negotiated HE peer capability summaries.

The central lesson is that HE features interact. Coding capability comes from
association-time negotiation, packet extension changes PPDU timing, and
preamble puncturing constrains which RUs are legal in a scheduled multi-user
frame.

Sources: :download:`omnetpp.ini <../omnetpp.ini>`,
:download:`HeFeaturesShowcase.ned <../HeFeaturesShowcase.ned>`

Try It Yourself
---------------

Open ``inet/showcases/wireless/hefeatures`` in the IDE and run the individual
feature configurations first, then ``CombinedHeFeatures``. To use ``opp_env``:

.. code-block:: bash

    $ opp_env run inet-4.6 --init -w inet-workspace --install --build-modes=release --chdir \
       -c 'cd inet-4.6.*/showcases/wireless/hefeatures && inet'

Discussion
----------

Use the INET issue tracker for commenting on this showcase.
