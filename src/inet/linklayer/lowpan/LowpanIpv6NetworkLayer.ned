// SPDX-License-Identifier: LGPL-3.0-or-later
package inet.linklayer.lowpan;

import inet.common.MessageDispatcher;
import inet.networklayer.configurator.contract.IIpv6NodeConfigurator;
import inet.networklayer.contract.INetworkLayer;
import inet.networklayer.icmpv6.Icmpv6;
import inet.networklayer.ipv6.Ipv6RoutingTable;

// Static LoWPAN profile. Mobility, IPsec and MLD are outside this composition.
module LowpanIpv6NetworkLayer like INetworkLayer
{
    parameters:
        bool forwarding = default(false);
        bool multicastForwarding = default(false);
        string interfaceTableModule = default("");
        *.forwarding = this.forwarding;
        *.multicastForwarding = this.multicastForwarding;
        *.interfaceTableModule = default(absPath(this.interfaceTableModule));
        *.routingTableModule = default(absPath(".routingTable"));
        *.ipv6NeighbourDiscoveryModule = default(absPath(".neighbourDiscovery"));
        *.icmpv6Module = default(absPath(".icmpv6"));
        *.mipv6Module = "";
        *.mldModule = "";
        @display("i=block/fork");
    gates:
        input ifIn;
        output ifOut;
        input transportIn;
        output transportOut;
    submodules:
        configurator: <default("Ipv6NodeConfigurator")> like IIpv6NodeConfigurator if typename != "" {
            @display("p=100,50;is=s");
        }
        routingTable: Ipv6RoutingTable {
            @display("p=100,100;is=s");
        }
        up: MessageDispatcher {
            @display("p=400,100;b=500,5");
        }
        icmpv6: Icmpv6 {
            @display("p=550,200");
        }
        lp: MessageDispatcher {
            @display("p=400,300;b=500,5");
        }
        neighbourDiscovery: LowpanIpv6NeighbourDiscovery {
            @display("p=250,400");
        }
        ipv6: LowpanIpv6 {
            @display("p=400,400");
        }
    connections:
        transportIn --> up.in++;
        transportOut <-- up.out++;
        up.out++ --> lp.in++;
        up.in++ <-- lp.out++;
        up.out++ --> icmpv6.transportIn;
        up.in++ <-- icmpv6.transportOut;
        icmpv6.ipv6Out --> lp.in++;
        icmpv6.ipv6In <-- lp.out++;
        lp.out++ --> ipv6.transportIn;
        lp.in++ <-- ipv6.transportOut;
        neighbourDiscovery.ipv6Out --> ipv6.ndIn;
        neighbourDiscovery.ipv6In <-- ipv6.ndOut;
        ipv6.queueOut --> ifOut;
        ipv6.queueIn <-- ifIn;
}
