//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"

namespace inet {
namespace ieee80211 {

void HeFrameSequenceHandler::handleStartRxTimeout()
{
    auto lastStep = context->getLastStep();
    switch (lastStep->getType()) {
        case IFrameSequenceStep::Type::RECEIVE: {
            auto receiveStep = check_and_cast<IReceiveStep *>(lastStep);
            if (!receiveStep->completesOnReception()) {
                EV_INFO << "HE FS handler: receive collection deadline reached.\n";
                finishFrameSequenceStep();
                if (isSequenceRunning())
                    startFrameSequenceStep();
            }
            else if (dynamic_cast<HeDlMuTxOpFs *>(frameSequence) != nullptr) {
                EV_INFO << "HE FS handler: sequential BlockAck timeout, continuing sequence.\n";
                lastStep->setCompletion(IFrameSequenceStep::Completion::REJECTED);
                finishFrameSequenceStep();
                if (isSequenceRunning())
                    startFrameSequenceStep();
            }
            else {
                abortFrameSequence();
            }
            break;
        }
        case IFrameSequenceStep::Type::TRANSMIT:
            throw cRuntimeError("Received timeout while in transmit step");
        default:
            throw cRuntimeError("Unknown step type");
    }
}

} // namespace ieee80211
} // namespace inet
