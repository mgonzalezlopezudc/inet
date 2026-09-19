// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

import inet.queueing.base.PacketPusherBase;
import inet.linklayer.lowpan.contract.ILowpanLink;

// Reversible legacy alias boundary. The dedicated domain owns PAN isolation.
simple LowpanCompatibilityBoundary extends PacketPusherBase like ILowpanLink
{
    parameters:
        string linkDomainModule = default(""); // Required shared compatibility domain.
        string macModule = default("^.mac");
        @class(LowpanCompatibilityBoundary);
        @display("i=block/ifcard");
        @signal[packetDropped](type=inet::Packet);
        @statistic[packetDropped](source=packetDropped; record=count; interpolationmode=none);
    gates:
        input lowerLayerIn;
        output upperLayerOut;
}
