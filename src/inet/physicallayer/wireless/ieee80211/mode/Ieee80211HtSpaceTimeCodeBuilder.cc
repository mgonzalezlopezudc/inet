//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtSpaceTimeCodeBuilder.h"

#include <cmath>

namespace inet {
namespace physicallayer {

SpaceTimeCodeDescriptor Ieee80211HtSpaceTimeCodeBuilder::build(int numberOfSpatialStreams,
    int numberOfSpaceTimeStreams, int stbc)
{
    if (numberOfSpatialStreams != 1 || numberOfSpaceTimeStreams != 2 || stbc != 1)
        throw cRuntimeError("HT Alamouti STBC is defined only for NSS=1, NSTS=2, STBC=1");

    const ComplexMatrix direct0(2, 2, {{1, 0}, {0, 0}, {0, 0}, {0, 0}});
    const ComplexMatrix conjugate0(2, 2, {{0, 0}, {0, 0}, {0, 0}, {-1, 0}});
    const ComplexMatrix direct1(2, 2, {{0, 0}, {1, 0}, {0, 0}, {0, 0}});
    const ComplexMatrix conjugate1(2, 2, {{0, 0}, {0, 0}, {1, 0}, {0, 0}});
    std::vector<SpaceTimeCodeDescriptor::Slot> slots;
    slots.emplace_back(direct0, conjugate0, false);
    slots.emplace_back(direct1, conjugate1, true);
    return SpaceTimeCodeDescriptor(numberOfSpatialStreams, 2, numberOfSpaceTimeStreams,
        1 / std::sqrt(2.0), slots, {false, true});
}

} // namespace physicallayer
} // namespace inet
