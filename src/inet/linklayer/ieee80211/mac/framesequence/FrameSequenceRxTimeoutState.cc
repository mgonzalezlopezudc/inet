//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceRxTimeoutState.h"

namespace inet {
namespace ieee80211 {

void FrameSequenceRxTimeoutState::clear()
{
    deferredStep = nullptr;
}

void FrameSequenceRxTimeoutState::deferCurrentStep(const IFrameSequenceStep *currentStep)
{
    ASSERT(currentStep != nullptr);
    deferredStep = currentStep;
}

bool FrameSequenceRxTimeoutState::takeIfCurrentStep(const IFrameSequenceStep *currentStep)
{
    auto deferredStep = this->deferredStep;
    clear();
    return deferredStep != nullptr && deferredStep == currentStep;
}

} // namespace ieee80211
} // namespace inet
