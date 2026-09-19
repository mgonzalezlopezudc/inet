// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan.contract;

moduleinterface ILowpanLayer
{
    gates:
        input upperLayerIn;
        output upperLayerOut;
        input lowerLayerIn;
        output lowerLayerOut;
}
