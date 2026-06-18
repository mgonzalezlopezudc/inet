//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_FRAMESEQUENCESTEP_H
#define __INET_FRAMESEQUENCESTEP_H

#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"

namespace inet {
namespace ieee80211 {

class INET_API TransmitStep : public ITransmitStep
{
  protected:
    Completion completion = Completion::UNDEFINED;
    Packet *frameToTransmit = nullptr;
    simtime_t ifs = -1;
    bool owner = false;

  public:
    TransmitStep(Packet *frame, simtime_t ifs, bool owner = false) :
        frameToTransmit(frame),
        ifs(ifs),
        owner(owner)
    {}

    virtual ~TransmitStep() { if (owner) delete frameToTransmit; }

    virtual Completion getCompletion() override { return completion; }
    virtual void setCompletion(Completion completion) override { this->completion = completion; }
    virtual Packet *getFrameToTransmit() override { return frameToTransmit; }
    virtual simtime_t getIfs() override { return ifs; }
};

class INET_API RtsTransmitStep : public TransmitStep
{
  protected:
    const Packet *protectedFrame = nullptr;

  public:
    RtsTransmitStep(Packet *protectedFrame, Packet *frame, simtime_t ifs) :
        TransmitStep(frame, ifs, true),
        protectedFrame(protectedFrame)
    {}

    virtual const Packet *getProtectedFrame() { return protectedFrame; }
};

class INET_API ReceiveStep : public IReceiveStep
{
  protected:
    Completion completion = Completion::UNDEFINED;
    simtime_t timeout = -1;
    Packet *receivedFrame = nullptr;

  public:
    ReceiveStep(simtime_t timeout = -1) :
        timeout(timeout)
    {}
    virtual ~ReceiveStep() { delete receivedFrame; }

    virtual Completion getCompletion() override { return completion; }
    virtual void setCompletion(Completion completion) override { this->completion = completion; }
    virtual simtime_t getTimeout() override { return timeout; }
    virtual Packet *getReceivedFrame() override { return receivedFrame; }
    virtual void setFrameToReceive(Packet *frame) override { this->receivedFrame = frame; }
};

class INET_API ReceiveCollectionStep : public ReceiveStep
{
  protected:
    std::vector<Packet *> receivedFrames;

  public:
    using ReceiveStep::ReceiveStep;
    virtual ~ReceiveCollectionStep()
    {
        for (auto frame : receivedFrames)
            delete frame;
    }

    virtual Packet *getReceivedFrame() override { return receivedFrames.empty() ? nullptr : receivedFrames.front(); }
    virtual void setFrameToReceive(Packet *frame) override { receivedFrames.push_back(frame); }
    virtual bool completesOnReception() const override { return false; }
    virtual const std::vector<Packet *>& getReceivedFrames() const { return receivedFrames; }
};

} // namespace ieee80211
} // namespace inet

#endif
