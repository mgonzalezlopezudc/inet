//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac;

import inet.linklayer.base.MacProtocolBase;

//
// Upper MAC for Multi-Link Operation (MLO) in 802.11be (EHT).
// It manages per-AC queues, global sequence numbers, and traffic steering
// across multiple lower MACs.
//
module Ieee80211MldMac extends MacProtocolBase
{
    parameters:
        @class(Ieee80211MldMac);
        @display("i=block/layer");
        int numLinks = default(1);
        string mldMacAddress @mutable = default("auto"); // MLD MAC address

    gates:
        input lowerLinkIn[numLinks] @labels(ILinkLayerFrame/up);
        output lowerLinkOut[numLinks] @labels(ILinkLayerFrame/down);

    submodules:
        pendingQueue[4]: <default("DropTailQueue")> like inet.queueing.contract.IPacketQueue {
            parameters:
                @display("p=100,100,row,150");
        }
}
