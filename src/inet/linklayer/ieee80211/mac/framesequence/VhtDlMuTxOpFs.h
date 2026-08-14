// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef __INET_VHTDLMUTXOPFS_H
#define __INET_VHTDLMUTXOPFS_H

#include <memory>

#include "inet/linklayer/ieee80211/mac/contract/IAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IVhtDlMuExchangeCallback.h"
#include "inet/linklayer/ieee80211/mac/framesequence/VhtDlMuPlan.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

class INET_API VhtDlMuStalePlan : public cRuntimeError
{
  public:
    explicit VhtDlMuStalePlan(const char *message) : cRuntimeError("%s", message) {}
};

/** Constrained VHT DL MU exchange: MU PPDU, then SIFS BAR/BA per user. */
class INET_API VhtDlMuTxOpFs : public IFrameSequence
{
  private:
    struct PreparedCommit;

  public:
    struct ActiveUser {
        IIeee80211VhtDlMuScheduler::Candidate candidate;
        std::vector<Packet *> packets;
    };

  protected:
    int step = -1;
    const VhtDlMuPlan plan;
    physicallayer::Ieee80211ModeSet *modeSet = nullptr;
    IAckHandler *ackHandler = nullptr;
    IFrameSequenceHandler::ICallback *callback = nullptr;
    IVhtDlMuExchangeCallback *vhtCallback = nullptr;
    uint64_t exchangeId = 0;
    Packet *containerPacket = nullptr;
    std::unique_ptr<PreparedCommit> preparedCommit;
    bool preparationAttempted = false;
    bool preparationSucceeded = false;
    bool commitAttempted = false;
    bool committed = false;
    bool containerOwnershipTransferred = false;
    std::vector<ActiveUser> activeUsers;

    std::unique_ptr<PreparedCommit> buildPreparedCommit(FrameSequenceContext *context);
    Packet *buildBlockAckReq(FrameSequenceContext *context, int userIndex) const;
    bool isExpectedBlockAck(Packet *packet, FrameSequenceContext *context,
            int userIndex) const;
    virtual void beforePreparedCommit() {}

  public:
    VhtDlMuTxOpFs(const VhtDlMuPlan& plan,
            physicallayer::Ieee80211ModeSet *modeSet, IAckHandler *ackHandler,
            IFrameSequenceHandler::ICallback *callback,
            IVhtDlMuExchangeCallback *vhtCallback, uint64_t exchangeId = 0);
    virtual ~VhtDlMuTxOpFs();

    /** Completes all fallible DL MU preparation without mutating live protocol ownership. */
    bool prepare(FrameSequenceContext *context);
    /** Adopts a successful preparation into live protocol ownership. */
    virtual bool commit(FrameSequenceContext *context);
    virtual void startSequence(FrameSequenceContext *context, int firstStep) override;
    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override;
    virtual bool completeStep(FrameSequenceContext *context) override;
    virtual std::string getHistory() const override;

    bool isContainerPacket(Packet *packet) const { return packet == containerPacket; }
    const std::vector<ActiveUser>& getActiveUsers() const { return activeUsers; }
};

} // namespace ieee80211
} // namespace inet

#endif
