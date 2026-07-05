802.11ax HE OFDMA
=================

Goals
-----

IEEE 802.11ax High Efficiency (HE) introduces multi-user OFDMA, where a
20 MHz channel can be divided into Resource Units (RUs) and assigned to several
stations in the same PPDU. This showcase demonstrates two complementary cases:

- downlink MU-OFDMA, where the AP sends one HE MU PPDU containing data for
  several stations;
- uplink MU-OFDMA, where the AP sends Trigger frames and stations answer at
  the same time on their assigned RUs.

The important thing to observe is not only that several packets are in flight at
once, but also how the scheduler's objective changes the RU layout, user
concurrency, and trigger overhead.

| Verified with INET version: ``4.6``
| Source files location: `inet/showcases/wireless/heofdma <https://github.com/inet-framework/inet/tree/master/showcases/wireless/heofdma>`__

The Model
---------

The showcase contains two small infrastructure WLANs. The downlink network,
:ned:`HeDlOfdmaShowcase`, has a wired server, one AP, and three stations. The
server creates downlink UDP traffic for all stations, so the AP can collect
multiple per-station queues and build HE MU frames.

The uplink network, :ned:`HeUlOfdmaShowcase`, uses the same AP and three-station
layout, but the stations generate UDP traffic toward the wired server. In this
direction, the AP must coordinate the stations with HE Trigger frames.

Both networks use 802.11ax mode and :ned:`HeHcf`:

.. literalinclude:: ../omnetpp.ini
   :start-at: **.opMode
   :end-at: **.isBlockAckSupported
   :language: ini

Downlink OFDMA
--------------

The downlink configurations compare three AP scheduler choices.

``DlEqualSizedRUs_fBW`` uses :ned:`HeDlSchedulerEqualSizedRUs` with the
``fBW`` objective. In a 20 MHz channel, this tends to select larger RUs and
serves fewer stations per PPDU. When you run the simulation, inspect the AP
``wlan[0].mac.hcf.dlScheduler`` watches ``lastScheduleSummary`` and
``lastRuAllocations``. You should see that the scheduler prefers higher
bandwidth occupancy over serving every queued station immediately.

.. literalinclude:: ../omnetpp.ini
   :start-at: DlEqualSizedRUs_fBW
   :end-before: DlEqualSizedRUs_fHoL
   :language: ini

``DlEqualSizedRUs_fHoL`` keeps the same scheduler but changes the objective to
``fHoL``. This favors head-of-line delay, so the AP should serve all three
stations concurrently with smaller RUs. Compare the per-host
``packetReceived:count`` scalars with ``DlEqualSizedRUs_fBW``: the bandwidth
oriented run usually has higher short-run aggregate delivery, while the HoL run
is more evenly spread among stations.

.. literalinclude:: ../omnetpp.ini
   :start-at: DlEqualSizedRUs_fHoL
   :end-before: DlBacklogBased
   :language: ini

``DlBacklogBased`` changes to :ned:`HeDlSchedulerBacklogBased` and uses
asymmetric traffic. ``host[0]`` receives the heaviest stream, ``host[1]`` a
medium stream, and ``host[2]`` a light stream. The scheduler can mix RU sizes in
the same PPDU, so the interesting observation is whether the larger backlog gets
more spectrum without completely starving the lighter stations.

.. literalinclude:: ../omnetpp.ini
   :start-at: DlBacklogBased
   :end-before: UplinkBase
   :language: ini

Uplink OFDMA
------------

In uplink OFDMA the AP first announces the transmission opportunity with a
Trigger frame. Stations transmit HE trigger-based PPDUs simultaneously, aligned
in time by the AP.

``UlScheduledOnly`` disables random-access RUs. The AP can still poll for buffer
status and schedule known stations, but stations do not contend for UORA RUs.
Observe the AP ``wlan[0].mac.hcf.ulCoordinator`` scalars
``heUlBsrpTriggerSent:count`` and ``heUlBasicTriggerSent:count``. They reveal
whether airtime was spent polling for backlog reports or starting data-bearing
triggered uplink exchanges.

.. literalinclude:: ../omnetpp.ini
   :start-at: UlScheduledOnly
   :end-before: UlMixedUora
   :language: ini

``UlMixedUora`` adds random-access RUs and limits the number of scheduled
stations. This demonstrates UORA: stations can attempt uplink access on RUs
assigned to AID 0. In a saturated three-station scenario this may reduce
delivered packets because random-access contention and extra trigger frames
consume airtime. That is the point of the configuration: UORA is useful for
many lightly active stations, but it is not automatically a throughput win in a
small saturated network.

.. literalinclude:: ../omnetpp.ini
   :start-at: UlMixedUora
   :end-before: UlEqualRus
   :language: ini

``UlEqualRus`` changes the uplink scheduler to equal-sized RUs. Compare its RU
layout with ``UlScheduledOnly`` and ``UlMixedUora`` in Qtenv, or compare the
same Trigger-frame counters in Cmdenv results.

.. literalinclude:: ../omnetpp.ini
   :start-at: UlEqualRus
   :language: ini

What To Observe
---------------

Run the downlink configurations first and watch the AP transmitter's
``lastHeTransmissionSummary`` and ``lastHeUserPhyParameters``. They show the HE
PPDU format, the number of users, and the per-user RU assignments. Then compare
the UDP sink ``packetReceived:count`` values to see the throughput/fairness
tradeoff.

For uplink runs, focus on the AP ``ulCoordinator`` counters:

.. code-block:: bash

   $ opp_scavetool query -l -f 'name =~ "heUlBsrpTriggerSent:count" or name =~ "heUlBasicTriggerSent:count" or name =~ "packetReceived:count"' results/*.sca

The lesson is that HE OFDMA gives the MAC a new scheduling dimension. The
scheduler can trade bandwidth occupancy, per-station delay, fairness, and
trigger overhead rather than only deciding who wins the whole channel.

Sources: :download:`omnetpp.ini <../omnetpp.ini>`,
:download:`HeDlOfdmaShowcase.ned <../HeDlOfdmaShowcase.ned>`,
:download:`HeUlOfdmaShowcase.ned <../HeUlOfdmaShowcase.ned>`

Try It Yourself
---------------

If you already have INET and OMNeT++ installed, start the IDE by typing
``omnetpp``, import the INET project, then navigate to the
``inet/showcases/wireless/heofdma`` folder in the Project Explorer.

With ``opp_env`` you can install INET and open this showcase interactively:

.. code-block:: bash

    $ opp_env run inet-4.6 --init -w inet-workspace --install --build-modes=release --chdir \
       -c 'cd inet-4.6.*/showcases/wireless/heofdma && inet'

Discussion
----------

Use the INET issue tracker for commenting on this showcase.
