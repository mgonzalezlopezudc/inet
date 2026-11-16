IEEE 802.15.4 Slotted Access Roles
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Custom ``IIeee802154AccessProvider`` implementations must implement ``deferCca()`` and
rebuild. The MAC uses this operation to return a slotted CCA that was not performed because
the radio or interframe spacing was still busy. A slotted provider must retain the attempt's
NB, BE and CW while finding another eligible grid boundary; an unslotted provider is not
asked to perform this operation.

The copied access window now carries physical CAP bounds, sender readiness and the optional
BLE and local beacon-preparation bounds, together with the actual beacon order. The access request separates the physical CCA,
turnaround and final IFS durations. Implementations must keep local-clock grid times distinct
from these physical durations.

Custom ``IIeee802154SuperframeProvider`` and ``IIeee802154SuperframeClient`` implementations
must also implement the access-window and access-search methods and rebuild. Published windows
represent actual beacons; an expired or retired window must not authorize later traffic.

External clocks must use a fixed rate within ±40 ppm, an aligned initial offset and a
clock quantum that divides the 16-microsecond symbol. With the supplied oscillator clock,
use ``clockGranularity=0`` and a compatible nominal tick; both future-event-set and
internal-event backends are supported. Do not step or change the rate during an installed
schedule. With no external clock, the simulation resolution must represent a symbol.
