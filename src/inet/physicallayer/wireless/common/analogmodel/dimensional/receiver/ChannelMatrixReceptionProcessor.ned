//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.physicallayer.wireless.common.analogmodel.dimensional.receiver;

import inet.physicallayer.wireless.common.contract.packetlevel.IChannelMatrixReceptionProcessor;

//
// Stateless receiver-local composition of antenna selection, combining, and
// spatial-stream detection.
//
simple ChannelMatrixReceptionProcessor like IChannelMatrixReceptionProcessor
{
    parameters:
        string antennaSelection = default("all") @enum("all","fixed","optimal");
        int activeReceiveAntennaCount = default(-1); // -1 selects all physical receive chains
        string fixedReceiveAntennaIndices = default("");
        string oneStreamCombiner = default("mrc") @enum("mrc","selection","maximumSinr","mmse");
        string spatialStreamDetector = default("mmse") @enum("zf","mmse","mmseSic");
        @class(ChannelMatrixReceptionProcessor);
        @display("i=block/process");
}
