//
// Copyright (C) 2026
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtPpduDescription.h"

namespace inet {
namespace physicallayer {

Ieee80211HtPpduDescription::Ieee80211HtPpduDescription(const Ieee80211HtPpduContext& context,
    const Ieee80211HtSignalField& signalField, int numberOfSpatialStreams,
    int numberOfSpaceTimeStreams, int numberOfDataLongTrainingFields,
    int numberOfExtensionLongTrainingFields, int numberOfLongTrainingFields) :
    context(context),
    signalField(signalField),
    numberOfSpatialStreams(numberOfSpatialStreams),
    numberOfSpaceTimeStreams(numberOfSpaceTimeStreams),
    numberOfDataLongTrainingFields(numberOfDataLongTrainingFields),
    numberOfExtensionLongTrainingFields(numberOfExtensionLongTrainingFields),
    numberOfLongTrainingFields(numberOfLongTrainingFields)
{
}

} // namespace physicallayer
} // namespace inet
