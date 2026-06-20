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
 * It extends the generic receive-timeout handling so that an HE MU exchange
 * is cleaned up consistently when its expected response does not arrive.
 */
class INET_API HeFrameSequenceHandler : public FrameSequenceHandler
{
  public:
    virtual void handleStartRxTimeout() override;
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HEFRAMESEQUENCEHANDLER_H
