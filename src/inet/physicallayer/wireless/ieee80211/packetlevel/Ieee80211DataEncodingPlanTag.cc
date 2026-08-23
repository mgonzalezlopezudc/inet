//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211DataEncodingPlanTag.h"

namespace inet {
namespace physicallayer {

Register_Class(Ieee80211DataEncodingPlanTag);

const Ieee80211DataEncodingPlan& Ieee80211DataEncodingPlanTag::getPlan() const
{
    if (plan == nullptr)
        throw cRuntimeError("IEEE 802.11 data encoding plan tag has no plan");
    return *plan;
}

std::ostream& Ieee80211DataEncodingPlanTag::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "Ieee80211DataEncodingPlanTag";
    if (plan != nullptr)
        stream << ", plan = " << *plan;
    else
        stream << ", plan = null";
    return stream;
}

} // namespace physicallayer
} // namespace inet
