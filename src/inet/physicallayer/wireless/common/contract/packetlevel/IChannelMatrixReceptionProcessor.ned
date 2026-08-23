//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.common.contract.packetlevel;

//
// Stateless receiver-local matrix/covariance processing policy.
//
moduleinterface IChannelMatrixReceptionProcessor
{
    parameters:
        @display("i=block/process");
}
