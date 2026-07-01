//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.llc;

import inet.common.MessageDispatcher;

module OmittedIeee80211Llc like IIeee80211Llc
{
    parameters:
        @display("i=block/layer");
    gates:
        input upperLayerIn;
        output upperLayerOut;
        input lowerLayerIn;
        output lowerLayerOut;
    submodules:
        dispatcher: MessageDispatcher {
            parameters:
                interfaceTableModule = default(absPath("^.^.^.interfaceTable"));
                forwardServiceRegistration = false;
                forwardProtocolRegistration = false;
                @display("p=100,100");
        }
    connections:
        upperLayerIn --> dispatcher.in++;
        dispatcher.out++ --> lowerLayerOut;
        lowerLayerIn --> dispatcher.in++;
        dispatcher.out++ --> upperLayerOut;
}
