// SPDX-License-Identifier: LGPL-3.0-or-later
//
package inet.physicallayer.wireless.ieee802154.packetlevel;

// Selected packet-level O-QPSK profile. The service provider owns turnaround/CCA timing.
module Ieee802154StandardRadio extends Ieee802154NarrowbandRadio
{
    parameters:
        transmitter.typename = "Ieee802154StandardTransmitter";
        initialRadioMode = "off";
        switchingTimes = "ms 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0";
        signalAnalogRepresentation = "scalar";
        separateTransmissionParts = false;
        separateReceptionParts = false;
        transmitter.power = 100mW; // Cache bound for the PHY's 20-dBm hardware maximum; actual power uses SignalPowerReq.
        // CCA uses independent energy observation, rather than generic radio-state detection.
        receiver.energyDetection = 1 W;
        @class(Ieee802154StandardRadio);
}
