//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_FRAMESEQUENCERXTIMEOUTSTATE_H
#define __INET_FRAMESEQUENCERXTIMEOUTSTATE_H

#include "inet/common/INETDefs.h"

namespace inet {
namespace ieee80211 {

class IFrameSequenceStep;

/**
 * Stores the non-owning frame-sequence step token for a deferred receive timeout.
 */
class INET_API FrameSequenceRxTimeoutState
{
  protected:
    const IFrameSequenceStep *deferredStep = nullptr;

  public:
    void clear();
    void deferCurrentStep(const IFrameSequenceStep *currentStep);
    bool hasDeferredStep() const { return deferredStep != nullptr; }
    bool takeIfCurrentStep(const IFrameSequenceStep *currentStep);
};

} // namespace ieee80211
} // namespace inet

#endif
