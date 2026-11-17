IEEE 802.15.4 Dual-CAP Window Origin
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``Ieee802154AccessWindow`` now includes an ``origin`` identifying an incoming
parent CAP or an outgoing coordinator CAP. Custom superframe providers should
preserve this field when storing and returning a published window. Its default
is ``UNKNOWN`` for callers that do not supply it; a linked coordinator requires
the appropriate incoming or outgoing origin to use a window. Rebuild custom
modules using this value type.

Custom ``IIeee802154SuperframeProvider`` implementations must also supply
``getForecastUncertainty(elapsed, maxPropagationDelay)``. Return zero for a simulation-time clock;
for a supported external clock, return a conservative physical uncertainty for
the declared fixed-rate and anchor-quantization profile. The MAC uses the bound
when comparing a predicted parent occurrence with an outgoing beacon. Rebuild
custom providers against the updated role.
