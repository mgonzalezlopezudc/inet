//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtCapabilities.h"

namespace inet {
namespace physicallayer {

Ieee80211HtCapabilities::Ieee80211HtCapabilities(bool txStbc, int rxStbc,
    int maximumSupportedMcs, int maximumSupportedSpatialStreams,
    bool ht20Supported, bool ht40Supported) :
    txStbc(txStbc), rxStbc(rxStbc), maximumSupportedMcs(maximumSupportedMcs),
    maximumSupportedSpatialStreams(maximumSupportedSpatialStreams),
    ht20Supported(ht20Supported), ht40Supported(ht40Supported)
{
    if (rxStbc < 0 || rxStbc > 3)
        throw cRuntimeError("HT Rx STBC capability must be in [0,3], got %d", rxStbc);
    if (maximumSupportedMcs < 0 || maximumSupportedMcs > 76)
        throw cRuntimeError("Maximum supported HT MCS must be in [0,76], got %d", maximumSupportedMcs);
    if (maximumSupportedSpatialStreams <= 0 || maximumSupportedSpatialStreams > 4)
        throw cRuntimeError("Maximum supported HT spatial streams must be in [1,4], got %d",
            maximumSupportedSpatialStreams);
    if (!ht20Supported && !ht40Supported)
        throw cRuntimeError("Local HT capabilities must support at least one channel width");
}

void Ieee80211HtCapabilities::validateTransmission(int mcs,
    int numberOfSpatialStreams, Hz bandwidth, bool spaceTimeCoded) const
{
    if (!supportsMcs(mcs))
        throw cRuntimeError("HT MCS %d exceeds the local maximum supported MCS %u",
            mcs, maximumSupportedMcs);
    if (numberOfSpatialStreams <= 0 ||
        numberOfSpatialStreams > maximumSupportedSpatialStreams)
        throw cRuntimeError("HT NSS %d exceeds the local maximum supported spatial-stream count %u",
            numberOfSpatialStreams, maximumSupportedSpatialStreams);
    if (!supportsChannelWidth(bandwidth))
        throw cRuntimeError("HT channel width %s is disabled by the local radio capabilities",
            bandwidth.str().c_str());
    if (spaceTimeCoded && !txStbc)
        throw cRuntimeError("HT STBC transmission requested while local htTxStbc capability is disabled");
}

} // namespace physicallayer
} // namespace inet
