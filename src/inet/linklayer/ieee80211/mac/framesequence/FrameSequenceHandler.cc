//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceHandler.h"

#include "inet/common/INETUtils.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"

namespace inet {
namespace ieee80211 {

void FrameSequenceHandler::handleStartRxTimeout()
{
    auto lastStep = context->getLastStep();
    switch (lastStep->getType()) {
        case IFrameSequenceStep::Type::RECEIVE: {
            auto receiveStep = check_and_cast<IReceiveStep *>(lastStep);
            lastStep->setCompletion(receiveStep->getTimeoutCompletion());
            switch (receiveStep->getTimeoutHandling()) {
                case IReceiveStep::TimeoutHandling::COMPLETE_STEP:
                    finishFrameSequenceStep();
                    if (isSequenceRunning())
                        startFrameSequenceStep();
                    break;
                case IReceiveStep::TimeoutHandling::ABORT_SEQUENCE:
                    abortFrameSequence();
                    break;
                default:
                    throw cRuntimeError("Unknown receive timeout handling");
            }
            break;
        }
        case IFrameSequenceStep::Type::TRANSMIT:
            // The receive timeout belongs to the preceding collection step.
            // If that step completed from a response at the same simulation
            // time, its timer may already be the event being dispatched and
            // cannot be cancelled retroactively. The active transmit step has
            // no receive timeout to fail, so ignore this stale event.
            EV_DEBUG << "Ignoring stale receive timeout during transmit step\n";
            break;
        default:
            throw cRuntimeError("Unknown step type");
    }
}

void FrameSequenceHandler::processResponse(Packet *frame)
{
    ASSERT(callback != nullptr);
    auto lastStep = context->getLastStep();
    switch (lastStep->getType()) {
        case IFrameSequenceStep::Type::RECEIVE: {
            auto receiveStep = check_and_cast<IReceiveStep *>(context->getLastStep());
            if (!receiveStep->isExpectedResponse(frame, context)) {
                switch (receiveStep->getUnexpectedResponseHandling()) {
                    case IReceiveStep::UnexpectedResponseHandling::IGNORE_RESPONSE:
                        delete frame;
                        break;
                    case IReceiveStep::UnexpectedResponseHandling::REJECT_STEP:
                        delete frame;
                        lastStep->setCompletion(IFrameSequenceStep::Completion::REJECTED);
                        abortFrameSequence();
                        break;
                    default:
                        throw cRuntimeError("Unknown unexpected response handling");
                }
                break;
            }
            receiveStep->setFrameToReceive(frame);
            if (receiveStep->completesOnReception()) {
                finishFrameSequenceStep();
                if (isSequenceRunning())
                    startFrameSequenceStep();
            }
            break;
        }
        case IFrameSequenceStep::Type::TRANSMIT:
            // A half-duplex 802.11 MAC cannot process a response that overlaps
            // its own transmission. Propagation delay may still deliver such
            // a frame from the packet-level radio; treat it as an undecodable
            // overlapping reception instead of aborting the active sequence.
            EV_WARN << "Discarding received frame while current step is transmit: "
                    << frame->getName() << "\n";
            delete frame;
            break;
        default:
            throw cRuntimeError("Unknown step type");
    }
}

void FrameSequenceHandler::transmissionComplete()
{
    if (isSequenceRunning()) {
        finishFrameSequenceStep();
        if (isSequenceRunning())
            startFrameSequenceStep();
    }
}

void FrameSequenceHandler::startFrameSequence(IFrameSequence *frameSequence, FrameSequenceContext *context, IFrameSequenceHandler::ICallback *callback)
{
    EV_INFO << "Starting frame sequence.\n";
    this->callback = callback;
    if (!isSequenceRunning()) {
        this->frameSequence = frameSequence;
        this->context = context;
        frameSequence->startSequence(context, 0);
        startFrameSequenceStep();
    }
    else
        throw cRuntimeError("Channel access granted while a frame sequence is running");
}

void FrameSequenceHandler::startFrameSequenceStep()
{
    ASSERT(isSequenceRunning());
    auto nextStep = frameSequence->prepareStep(context);
    EV_INFO << "Starting next frame sequence step: history = " << frameSequence->getHistory() << "\n";
    if (nextStep == nullptr)
        finishFrameSequence();
    else {
        context->addStep(nextStep);
        switch (nextStep->getType()) {
            case IFrameSequenceStep::Type::TRANSMIT: {
                auto transmitStep = static_cast<TransmitStep *>(nextStep);
                EV_INFO << "Transmitting, frame = " << transmitStep->getFrameToTransmit() << ".\n";
                callback->transmitFrame(transmitStep->getFrameToTransmit(), transmitStep->getIfs());
                // TODO lifetime
//                if (auto dataFrame = dynamic_cast<const Ptr<const Ieee80211DataHeader>& >(transmitStep->getFrameToTransmit()))
//                    transmitLifetimeHandler->frameTransmitted(dataFrame);
                break;
            }
            case IFrameSequenceStep::Type::RECEIVE: {
                // start reception timer, break loop if timer expires before reception is over
                auto receiveStep = static_cast<IReceiveStep *>(nextStep);
                callback->scheduleStartRxTimer(receiveStep->getTimeout());
                break;
            }
            default:
                throw cRuntimeError("Unknown frame sequence step type");
        }
    }
}

void FrameSequenceHandler::finishFrameSequenceStep()
{
    ASSERT(isSequenceRunning());
    auto lastStep = context->getLastStep();
    auto stepResult = frameSequence->completeStep(context);
    EV_INFO << "Finishing last frame sequence step: history = " << frameSequence->getHistory() << "\n";
    if (!stepResult) {
        if (lastStep->getCompletion() == IFrameSequenceStep::Completion::UNDEFINED)
            lastStep->setCompletion(IFrameSequenceStep::Completion::REJECTED);
        abortFrameSequence();
    }
    else {
        if (lastStep->getCompletion() == IFrameSequenceStep::Completion::UNDEFINED)
            lastStep->setCompletion(IFrameSequenceStep::Completion::ACCEPTED);
        switch (lastStep->getType()) {
            case IFrameSequenceStep::Type::TRANSMIT: {
                auto transmitStep = static_cast<ITransmitStep *>(lastStep);
                callback->originatorProcessTransmittedFrame(transmitStep->getFrameToTransmit());
                break;
            }
            case IFrameSequenceStep::Type::RECEIVE: {
                auto receiveStep = static_cast<IReceiveStep *>(lastStep);
                if (!receiveStep->completesOnReception())
                    break;
                if (receiveStep->getCompletion() != IFrameSequenceStep::Completion::ACCEPTED || receiveStep->getReceivedFrame() == nullptr)
                    break;
                ITransmitStep *transmitStep = nullptr;
                for (int i = context->getNumSteps() - 2; i >= 0; --i) {
                    if (context->getStep(i)->getType() == IFrameSequenceStep::Type::TRANSMIT) {
                        transmitStep = check_and_cast<ITransmitStep *>(context->getStep(i));
                        break;
                    }
                }
                ASSERT(transmitStep != nullptr);
                callback->originatorProcessReceivedFrame(receiveStep->getReceivedFrame(), transmitStep->getFrameToTransmit());
                break;
            }
            default:
                throw cRuntimeError("Unknown frame sequence step type");
        }
    }
}

void FrameSequenceHandler::finishFrameSequence()
{
    EV_INFO << "Frame sequence finished.\n";
    auto inProgressFrames = context->getInProgressFrames();
    callback->frameSequenceFinished();
    delete context;
    delete frameSequence;
    context = nullptr;
    frameSequence = nullptr;
    callback = nullptr;
    inProgressFrames->clearDroppedFrames();
}

void FrameSequenceHandler::abortFrameSequence()
{
    EV_INFO << "Frame sequence aborted.\n";
    auto inProgressFrames = context->getInProgressFrames();
    auto step = context->getLastStep();
    ITransmitStep *failedTxStep = nullptr;
    if (step->getType() == IFrameSequenceStep::Type::TRANSMIT) {
        failedTxStep = check_and_cast<ITransmitStep *>(step);
    }
    else {
        for (int i = context->getNumSteps() - 2; i >= 0; --i) {
            if (context->getStep(i)->getType() == IFrameSequenceStep::Type::TRANSMIT) {
                failedTxStep = check_and_cast<ITransmitStep *>(context->getStep(i));
                break;
            }
        }
    }
    ASSERT(failedTxStep != nullptr);
    auto frameToTransmit = failedTxStep->getFrameToTransmit();
    auto header = frameToTransmit->peekAtFront<Ieee80211MacHeader>();
    if (auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(header))
        callback->originatorProcessFailedFrame(frameToTransmit);
    else if (auto rtsTxStep = dynamic_cast<RtsTransmitStep *>(failedTxStep))
        callback->originatorProcessRtsProtectionFailed(const_cast<Packet *>(rtsTxStep->getProtectedFrame()));
    else if (auto blockAckReq = dynamicPtrCast<const Ieee80211BlockAckReq>(header))
        callback->originatorProcessFailedFrame(frameToTransmit);
    callback->frameSequenceFinished();
    delete context;
    delete frameSequence;
    context = nullptr;
    frameSequence = nullptr;
    callback = nullptr;
    inProgressFrames->clearDroppedFrames();
}

FrameSequenceHandler::~FrameSequenceHandler()
{
    delete frameSequence;
    delete context;
}

} // namespace ieee80211
} // namespace inet
