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
