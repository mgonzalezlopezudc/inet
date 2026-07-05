802.11ax Target Wake Time
=========================

Goals
-----

Target Wake Time (TWT) is an IEEE 802.11ax HE power-saving mechanism. Instead
of keeping a station awake for every beacon opportunity, the AP and station
agree on service periods. The station can sleep outside those periods and wake
up when the agreement says useful traffic exchange may occur.

This showcase compares a no-TWT baseline with individual unannounced,
individual announced, and broadcast TWT schedules.

| Verified with INET version: ``4.6``
| Source files location: `inet/showcases/wireless/twt <https://github.com/inet-framework/inet/tree/master/showcases/wireless/twt>`__

The Model
---------

The :ned:`TwtShowcase` network contains one AP, two stations, and a wired
server. The server sends low-duty-cycle UDP traffic to both stations. This is a
good TWT use case: packets are infrequent, so the stations can sleep for most of
the simulation without losing much delivery opportunity.

The WLAN interfaces use the TWT manager and advertise requester, responder, and
broadcast TWT support:

.. literalinclude:: ../omnetpp.ini
   :start-at: **.wlan[*].mib.heTwtRequester
   :end-at: **.isBlockAckSupported
   :language: ini

Baseline
--------

``Baseline`` disables TWT and uses the ordinary station agent:

.. literalinclude:: ../omnetpp.ini
   :start-at: Baseline
   :end-before: IndividualUnannounced
   :language: ini

The stations remain awake. Use this run as the reference for packet delivery and
sleep time. The ``twtAgreementCount`` should be zero and ``twtSleepTime`` should
stay at zero.

Individual TWT
--------------

``IndividualUnannounced`` asks each station to negotiate its own unannounced
TWT agreement with a 100 ms wake interval and a 20 ms service period:

.. literalinclude:: ../omnetpp.ini
   :start-at: IndividualUnannounced
   :end-before: IndividualAnnounced
   :language: ini

``IndividualAnnounced`` changes the agreement type to announced. The station
wakes for the service period but waits for AP indication before exchanging
traffic:

.. literalinclude:: ../omnetpp.ini
   :start-at: IndividualAnnounced
   :end-before: Broadcast
   :language: ini

For both individual modes, inspect the TWT manager scalars
``twtAgreementCount``, ``twtAwakeTime``, and ``twtSleepTime``. The AP should
have one agreement per station, and each station should spend most of the
100-second run asleep.

Broadcast TWT
-------------

``Broadcast`` lets the AP create one broadcast schedule and lets both stations
join it:

.. literalinclude:: ../omnetpp.ini
   :start-at: Broadcast
   :language: ini

The interesting contrast with individual TWT is that multiple stations share
one schedule. This is useful when an AP wants to coordinate a group, for example
before MU-OFDMA service. Compare ``twtBroadcastScheduleCount`` and the awake
time with the individual configurations.

What To Observe
---------------

After running all four configurations, query the TWT and delivery scalars:

.. code-block:: bash

   $ opp_scavetool query -l -f 'name =~ "twtAgreementCount" or name =~ "twtBroadcastScheduleCount" or name =~ "twtAwakeTime" or name =~ "twtSleepTime" or name =~ "packetReceived:count"' results/*.sca

The expected qualitative result is simple and instructive: packet delivery
should remain comparable for this sparse traffic, while TWT-enabled stations
accumulate large sleep times. If a radio energy consumer is added in a derived
experiment, the same configurations can be used to compare consumed energy.

Sources: :download:`omnetpp.ini <../omnetpp.ini>`,
:download:`TwtShowcase.ned <../TwtShowcase.ned>`

Try It Yourself
---------------

Open ``inet/showcases/wireless/twt`` in the IDE and run the four configurations.
To use ``opp_env``:

.. code-block:: bash

    $ opp_env run inet-4.6 --init -w inet-workspace --install --build-modes=release --chdir \
       -c 'cd inet-4.6.*/showcases/wireless/twt && inet'

Discussion
----------

Use the INET issue tracker for commenting on this showcase.
