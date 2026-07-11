package inet.linklayer.ieee80211.mac.fragmentation;
import inet.linklayer.ieee80211.mac.fragmentation.BasicFragmentationPolicy;
simple HeDynamicFragmentationPolicy extends BasicFragmentationPolicy
{
    parameters:
        @class(HeDynamicFragmentationPolicy);
        int requiredLevel = default(1);
}
