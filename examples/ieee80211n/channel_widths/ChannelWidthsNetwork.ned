package inet.examples.ieee80211n.channel_widths;

import inet.examples.ieee80211.common.SingleBssNetwork;
import inet.node.inet.WirelessHost;

network ChannelWidthsNetwork extends SingleBssNetwork
{
    parameters:
        bool hasInterferer = default(false);
    submodules:
        interferer: WirelessHost if hasInterferer {
            @display("p=350,200");
        }
}
