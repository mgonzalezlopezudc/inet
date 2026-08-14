//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/EhtDlMuTxOpFs.h"

namespace inet {
namespace ieee80211 {

namespace {

IFrameSequenceHandler::ICallback *requireLegacyCallback(
        IFrameSequenceHandler::ICallback *callback)
{
    if (callback == nullptr)
        throw cRuntimeError("Legacy EHT DL MU construction requires an explicit callback owner");
    return callback;
}

} // namespace

EhtDlMuTxOpFs::EhtDlMuTxOpFs(const HeDlMuPlan& dlPlan,
                             physicallayer::Ieee80211ModeSet *modeSet,
                             queueing::IPacketQueue *pendingQueue,
                             IAckHandler *ackHandler,
                             IFrameSequenceHandler::ICallback *callback,
                             IHeDlMuExecutionServices *heServices,
                             IHeDlMuExchangeEvents *heEvents,
                             HeDlMuExchangeId transactionToken,
                             int maxAmpduMpduCount,
                             int maxHeMuPsduLength,
                             simtime_t maxHeMuPpduDuration,
                             AckMethod ackMethod,
                             bool ehtEnabled)
    : HeDlMuTxOpFs(dlPlan, modeSet, pendingQueue, ackHandler, callback,
            heServices, heEvents, transactionToken, maxAmpduMpduCount,
            maxHeMuPsduLength, maxHeMuPpduDuration, ackMethod)
{
    this->ehtEnabled = ehtEnabled;
}

EhtDlMuTxOpFs::EhtDlMuTxOpFs(const HeDlMuPlan& dlPlan,
                             physicallayer::Ieee80211ModeSet *modeSet,
                             queueing::IPacketQueue *pendingQueue,
                             IAckHandler *ackHandler,
                             IFrameSequenceHandler::ICallback *callback,
                             IFrameSequenceHandler::ICallback *legacyCallback,
                             IQosRateSelection *legacyRateSelection,
                             int maxAmpduMpduCount,
                             int maxHeMuPsduLength,
                             simtime_t maxHeMuPpduDuration,
                             AckMethod ackMethod,
                             bool ehtEnabled)
    : HeDlMuTxOpFs(dlPlan, modeSet, pendingQueue, ackHandler, callback,
            requireLegacyCallback(legacyCallback), legacyRateSelection,
            maxAmpduMpduCount, maxHeMuPsduLength,
            maxHeMuPpduDuration, ackMethod)
{
    this->ehtEnabled = ehtEnabled;
}

EhtDlMuTxOpFs::~EhtDlMuTxOpFs()
{
}

} // namespace ieee80211
} // namespace inet
