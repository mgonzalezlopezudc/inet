// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan.contract;

moduleinterface ILowpanLink
{
    gates:
        input in;
        output out;
        input lowerLayerIn;
        output upperLayerOut;
}
