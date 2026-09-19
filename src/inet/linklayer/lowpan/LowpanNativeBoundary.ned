// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

import inet.queueing.base.PacketPusherBase;
import inet.linklayer.lowpan.contract.ILowpanLink;

// Native receive metadata boundary and bounded queue admission.
simple LowpanNativeBoundary extends PacketPusherBase like ILowpanLink
{
    parameters:
        string linkDomainModule = default(""); // Required shared native domain.
        string macModule = default("^.mac");
        @class(LowpanNativeBoundary);
        @display("i=block/ifcard");
        @signal[packetDropped](type=inet::Packet);
        @statistic[packetDropped](source=packetDropped; record=count; interpolationmode=none);
    gates:
        input lowerLayerIn;
        output upperLayerOut;
}
