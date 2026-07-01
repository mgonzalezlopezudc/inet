//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/EhtDlMuTxOpFs.h"

namespace inet {
namespace ieee80211 {

EhtDlMuTxOpFs::EhtDlMuTxOpFs(IIeee80211HeDlScheduler *dlScheduler,
                             const IIeee80211HeDlScheduler::ScheduleContext& scheduleContext,
                             physicallayer::Ieee80211ModeSet *modeSet,
                             queueing::IPacketQueue *pendingQueue,
                             IAckHandler *ackHandler,
                             IFrameSequenceHandler::ICallback *callback,
                             int maxAmpduMpduCount,
                             int maxHeMuPsduLength,
                             simtime_t maxHeMuPpduDuration,
                             AckMethod ackMethod,
                             bool ehtEnabled)
    : HeDlMuTxOpFs(dlScheduler, scheduleContext, modeSet, pendingQueue, ackHandler, callback, maxAmpduMpduCount, maxHeMuPsduLength, maxHeMuPpduDuration, ackMethod)
{
    this->ehtEnabled = ehtEnabled;
}

EhtDlMuTxOpFs::~EhtDlMuTxOpFs()
{
}

} // namespace ieee80211
} // namespace inet
