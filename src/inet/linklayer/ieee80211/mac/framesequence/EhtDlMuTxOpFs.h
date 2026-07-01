//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_EHTDLMUTXOPFS_H
#define __INET_EHTDLMUTXOPFS_H

#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"

namespace inet {
namespace ieee80211 {

/**
 * Frame sequence for IEEE 802.11be Downlink MU TXOP.
 * Inherits from HeDlMuTxOpFs and extends it for EHT support (MRUs, EHT PHY mode, etc).
 */
class INET_API EhtDlMuTxOpFs : public HeDlMuTxOpFs
{
  public:
    EhtDlMuTxOpFs(IIeee80211HeDlScheduler *dlScheduler,
                  const IIeee80211HeDlScheduler::ScheduleContext& scheduleContext,
                  physicallayer::Ieee80211ModeSet *modeSet,
                  queueing::IPacketQueue *pendingQueue,
                  IAckHandler *ackHandler,
                  IFrameSequenceHandler::ICallback *callback,
                  int maxAmpduMpduCount = 16,
                  int maxHeMuPsduLength = 6500631,
                  simtime_t maxHeMuPpduDuration = SimTime(5.484, SIMTIME_MS),
                  AckMethod ackMethod = AckMethod::EXPLICIT_SEQUENTIAL_BAR,
                  bool ehtEnabled = true);

    virtual ~EhtDlMuTxOpFs() override;
    
  protected:
    bool ehtEnabled = true;
};

} // namespace ieee80211
} // namespace inet

#endif
