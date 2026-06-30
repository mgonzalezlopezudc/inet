//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEFRAMESEQUENCEHANDLER_H
#define __INET_HEFRAMESEQUENCEHANDLER_H

#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceHandler.h"

namespace inet {
namespace ieee80211 {

/**
 * HE-specific frame-sequence handler.
 *
 * HE receive-timeout behavior is expressed by the active IReceiveStep.
 */
class INET_API HeFrameSequenceHandler : public FrameSequenceHandler
{
  public:
    virtual void handleStartRxTimeout() override;
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HEFRAMESEQUENCEHANDLER_H
