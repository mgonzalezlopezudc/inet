IEEE 802.15.4 Access Window Contract
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Custom ``IIeee802154AccessProvider`` implementations must implement ``updateWindow()`` and
handle the optional copied CAP window and exchange duration in the access request. The existing
unslotted provider refuses windowed requests. Custom ``IIeee802154AccessClient`` implementations
must accept the additional optional physical start time in ``handleAccessConfirm()``. Rebuild
custom models using either role. A missing start time retains the original immediate-transmit
semantics; a supplied time is an absolute current or future simulation-time opportunity.
