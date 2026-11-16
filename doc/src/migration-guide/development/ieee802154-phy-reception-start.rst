IEEE 802.15.4 PHY Reception Start
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``IIeee802154PhyServiceClient`` has a new pure virtual callback. Custom PHY clients must implement
the following method and rebuild their model:

.. code-block:: c++

   void handleReceiveStarted(const Ieee802154PhyReceiveStart& start) override;

A client that does not use reception-start information may implement it as a no-op. The record is
borrowed for the call; copy any values needed later.

Custom PHY providers used with ``enableSuperframe`` must notify the client at the actual first
arrival symbol and include the same copied transmission ID in the matching receive indication.
The receiver samples its local clock at that event. Do not retain pointers to the borrowed record
or reconstruct a historical local-clock sample after reception.
