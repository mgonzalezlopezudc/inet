802.11ax BSS Coloring and Spatial Reuse
=======================================

Goals
-----

In dense deployments, nearby WLANs often hear each other's transmissions even
when they belong to different basic service sets (BSSs). Traditional carrier
sensing makes them defer to each other, so several neighboring BSSs can behave
like one large contention domain. IEEE 802.11ax HE adds BSS coloring and
OBSS/PD-based spatial reuse so a receiver can recognize an inter-BSS PPDU and,
when it is weak enough, ignore it for channel-access purposes.

This showcase compares the same two-BSS scenario with spatial reuse disabled
and enabled.

| Verified with INET version: ``4.6``
| Source files location: `inet/showcases/wireless/bsscoloring <https://github.com/inet-framework/inet/tree/master/showcases/wireless/bsscoloring>`__

The Model
---------

The :ned:`BssColoringShowcase` network contains two APs, two stations per AP,
and a wired server behind each AP. Both BSSs use the same 5 GHz 20 MHz channel.
The APs are far enough apart that the other BSS is received at a moderate power:
strong enough to be detected by ordinary carrier sensing, but below the
configured OBSS/PD threshold when spatial reuse is enabled.

The APs are assigned different HE BSS colors:

.. literalinclude:: ../omnetpp.ini
   :start-at: BssColoringDisabled
   :end-before: BssColoringEnabled
   :language: ini

Spatial Reuse Disabled
----------------------

The ``BssColoringDisabled`` configuration turns off spatial reuse:

.. literalinclude:: ../omnetpp.ini
   :start-at: **.receiver.enableSpatialReuse = false
   :end-at: *.ap2.wlan[*].mib.heBssColor = 2
   :language: ini

Run this configuration first. You should observe that the two BSSs defer to
each other when one hears the other's PPDU. The channel is shared by contention,
and short runs can be unfair: one BSS may win many more channel accesses than
the other.

Spatial Reuse Enabled
---------------------

The ``BssColoringEnabled`` configuration enables spatial reuse and raises the
OBSS/PD threshold:

.. literalinclude:: ../omnetpp.ini
   :start-at: BssColoringEnabled
   :language: ini

When a receiver sees a frame with a different BSS color and the signal is below
the OBSS/PD threshold, it treats the frame as reusable spatial overlap instead
of as a reason to keep the medium busy. In Qtenv, inspect a radio receiver's
watches: ``lastSpatialReuseBssTypeName``, ``lastSpatialReuseEligible``,
``lastSpatialReuseIgnoredPpdu``, ``lastSpatialReuseObssPdThreshold``, and
``lastSpatialReuseReason``. They explain why an inter-BSS PPDU was ignored or
not ignored.

What To Observe
---------------

Compare application delivery for both configurations:

.. code-block:: bash

   $ opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.sta*"' results/*.sca

With spatial reuse disabled, the two APs protect each other's transmissions and
serialize much of the traffic. With spatial reuse enabled, the different BSS
colors and OBSS/PD threshold allow more concurrent transmissions. The expected
qualitative result is a higher aggregate number of received packets and less
severe starvation between the two BSSs.

The important teaching point is that BSS color alone does not increase
throughput. It is the combination of color classification and an OBSS/PD
threshold decision that lets a station reuse the channel.

Sources: :download:`omnetpp.ini <../omnetpp.ini>`,
:download:`BssColoringShowcase.ned <../BssColoringShowcase.ned>`

Try It Yourself
---------------

Open the ``inet/showcases/wireless/bsscoloring`` folder in the IDE and run
``BssColoringDisabled`` followed by ``BssColoringEnabled``. To use ``opp_env``:

.. code-block:: bash

    $ opp_env run inet-4.6 --init -w inet-workspace --install --build-modes=release --chdir \
       -c 'cd inet-4.6.*/showcases/wireless/bsscoloring && inet'

Discussion
----------

Use the INET issue tracker for commenting on this showcase.
