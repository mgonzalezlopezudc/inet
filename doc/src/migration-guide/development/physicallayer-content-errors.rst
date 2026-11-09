Content Errors in the Physical Layer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The SNIR of a Time Interval
^^^^^^^^^^^^^^^^^^^^^^^^^^^

:cpp:`ISnir` has three new pure virtual methods. They return the SNIR of a
time interval inside the reception:

.. code-block:: c++

   virtual double getMin(simtime_t startTime, simtime_t endTime) const = 0;
   virtual double getMax(simtime_t startTime, simtime_t endTime) const = 0;
   virtual double getMean(simtime_t startTime, simtime_t endTime) const = 0;

An :cpp:`ISnir` implementation outside INET no longer compiles until it
overrides these methods. The methods without arguments keep their meaning:
they return the values of the whole reception. In :cpp:`ScalarSnir` and
:cpp:`DimensionalSnir`, the protected functions :cpp:`computeMin()`,
:cpp:`computeMax()`, and :cpp:`computeMean()` now take the start and the end
of the interval.

Error Rates of Signal Regions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:cpp:`IErrorModel` has a new pure virtual method:

.. code-block:: c++

   virtual double computePacketErrorRate(const ISnir *snir, IRadioSignal::SignalPart part, b length, simtime_t startTime, simtime_t endTime) const = 0;

The method gives the error rate of a region of the header part or the data
part. The region has the given length, and the transmitter sends it from
``startTime`` to ``endTime``. Use the SNIR of that interval, for example with
:cpp:`ErrorModelBase::getScalarSnir(snir, startTime, endTime)`. An
:cpp:`IErrorModel` implementation outside INET no longer compiles until it
overrides this method.

A model that derives from :cpp:`Ieee80211ErrorModelBase` and overrides
:cpp:`getDataSuccessRate()` gets the exact number of bits to count. For an OFDM
mode, :cpp:`computePacketErrorRate()` now adds the SERVICE bits before the call,
and :cpp:`getDataSuccessRate()` no longer adds them.

Corruption of Failed Regions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The enum :cpp:`CorruptionMode` moved from :cpp:`ErrorModelBase` to
:cpp:`IErrorModel`; the name :cpp:`ErrorModelBase::CorruptionMode` still refers
to it. :cpp:`IErrorModel` has two new pure virtual methods:

.. code-block:: c++

   virtual CorruptionMode getCorruptionMode() const = 0;
   virtual Packet *computeCorruptedPacket(const Packet *packet, const std::vector<ErrorRegion>& regions) const = 0;

The second method gives the received packet with the failed regions that the
error decisions of the receiver found. A model that derives from
:cpp:`ErrorModelBase` inherits both methods. It can change the corrupted form of
a region with the protected :cpp:`computeCorruptedChunk()`, and
:cpp:`corruptUnits()` gives the units with errors of a failed region. An
:cpp:`IErrorModel` implementation outside INET no longer compiles until it
overrides the two methods.
