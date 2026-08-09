//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_MINSTRELRATECONTROLBASE_H
#define __INET_MINSTRELRATECONTROLBASE_H

#include <map>
#include <string>
#include <vector>

#include "inet/linklayer/ieee80211/mac/contract/IIeee80211PeerRateControl.h"
#include "inet/linklayer/ieee80211/mac/ratecontrol/RateControlBase.h"

namespace inet {
namespace ieee80211 {

/** Shared Minstrel statistics and candidate-selection policy. */
class INET_API MinstrelRateControlBase : public RateControlBase, public IIeee80211PeerRateControl
{
  protected:
    struct RateStats {
        double ewmaSuccessProbability = 0.9;
        int attempts = 0;
        int successes = 0;
        simtime_t lastProbe = SIMTIME_ZERO;
        uint64_t appliedSnirGeneration = 0;
    };

    struct PeerState {
        std::map<const physicallayer::IIeee80211Mode *, RateStats> rates;
        int selectionCount = 0;
        double latestSnirDb = NaN;
        simtime_t latestSnirUpdate = SIMTIME_ZERO;
        uint64_t snirGeneration = 0;
    };

    simtime_t updateInterval = SimTime(100, SIMTIME_MS);
    double ewmaWeight = 0.75;
    double lookaroundRatio = 0.1;
    double initialSuccessProbability = 0.9;
    bool seedFromSnir = true;
    double snirMcs0ThresholdDb = 4;
    double snirMcsStepDb = 3;
    int minMcs = 0;
    int maxMcs = 11;
    int maxNss = 1;
    std::map<MacAddress, PeerState> peers;

    simsignal_t selectedMcsSignal = -1;
    simsignal_t selectedNssSignal = -1;
    simsignal_t probeSignal = -1;
    simsignal_t successProbabilitySignal = -1;
    simsignal_t txSuccessSignal = -1;
    simsignal_t retryCountSignal = -1;

  protected:
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;

    void registerMinstrelSignals(const std::string& prefix);
    PeerState& getPeerState(const MacAddress& peer);
    const MacAddress getReceiverAddress(Packet *frame) const;

    virtual bool isRateCandidate(const physicallayer::IIeee80211Mode *mode) const = 0;
    virtual int getModeMcs(const physicallayer::IIeee80211Mode *mode) const = 0;
    virtual int getModeNss(const physicallayer::IIeee80211Mode *mode) const = 0;

    const physicallayer::IIeee80211Mode *selectCandidate(const MacAddress& peer,
            const std::vector<const physicallayer::IIeee80211Mode *>& candidates,
            bool& probing);
    void emitSelection(const MacAddress& peer, const physicallayer::IIeee80211Mode *mode, bool probing);
    void reportModeTxResult(const MacAddress& peer, const physicallayer::IIeee80211Mode *mode,
            int retryCount, bool success);
    void reportModeRxSnir(const MacAddress& peer, double snirDb);

  public:
    virtual const physicallayer::IIeee80211Mode *getRate() override { return currentMode; }
    virtual void frameTransmitted(Packet *frame, int retryCount, bool isSuccessful,
            bool isGivenUp) override;
    virtual void frameReceived(Packet *frame) override;
    virtual const physicallayer::IIeee80211Mode *selectRate(const MacAddress& peer,
            const std::vector<const physicallayer::IIeee80211Mode *>& candidates) override;
    virtual void invalidatePeer(const MacAddress& peer);
};

} // namespace ieee80211
} // namespace inet

#endif
