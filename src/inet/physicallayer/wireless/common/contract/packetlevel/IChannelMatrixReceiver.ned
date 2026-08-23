//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.common.contract.packetlevel;

//
// Receiver with an optional receiver-local channel-matrix processor.
//
moduleinterface IChannelMatrixReceiver extends IReceiver
{
    parameters:
        @display("i=block/wrx");
}
