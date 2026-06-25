//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEMINSTRELRATECONTROL_H
#define __INET_HEMINSTRELRATECONTROL_H

#include <map>
#include <vector>

#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HeRateControl.h"
#include "inet/linklayer/ieee80211/mac/ratecontrol/RateControlBase.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"

namespace inet {
namespace ieee80211 {

/**
 * Minstrel-style HE rate controller.
 *
 * The implementation keeps an EWMA success probability per peer/MCS/NSS tuple,
 * periodically samples a non-best rate, and scores rates by expected goodput.
 * It also implements the legacy IRateControl interface so existing SU rate
 * selection can opt into the same module.
 */
class INET_API HeMinstrelRateControl : public RateControlBase, public IIeee80211HeRateControl
{
  protected:
    struct RateKey {
        int mcs = 0;
        int numberOfSpatialStreams = 1;

        bool operator<(const RateKey& other) const
        {
            return mcs < other.mcs ||
                    (mcs == other.mcs && numberOfSpatialStreams < other.numberOfSpatialStreams);
        }
    };

    struct RateStats {
        double ewmaSuccessProbability = 0.9;
        int attempts = 0;
        int successes = 0;
        simtime_t lastProbe = SIMTIME_ZERO;
        double lastSnirDb = NaN;
    };

    struct PeerState {
        std::map<RateKey, RateStats> rates;
        int selectionCount = 0;
    };

  protected:
    simtime_t updateInterval = SimTime(100, SIMTIME_MS);
    double ewmaWeight = 0.75;
    double lookaroundRatio = 0.1;
    double initialSuccessProbability = 0.9;
    bool seedFromSnir = true;
    bool enableExtendedRangeSu = false;
    bool preferDcm = false;
    double snirMcs0ThresholdDb = 4;
    double snirMcsStepDb = 3;
    int minMcs = 0;
    int maxMcs = 11;
    int maxNss = 1;
    std::map<MacAddress, PeerState> peers;

    simsignal_t selectedMcsSignal;
    simsignal_t selectedNssSignal;
    simsignal_t probeSignal;
    simsignal_t successProbabilitySignal;

  protected:
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;

    const MacAddress getReceiverAddress(Packet *frame) const;
    const physicallayer::Ieee80211HeMode *findHeMode(int mcs, int nss, Hz bandwidth,
            bool extendedRangeSu, bool ldpc) const;
    int clampMcsForConstraints(int mcs, int ruToneSize, uint8_t ppduFormat,
            int maxNss, const Constraints& constraints) const;
    double scoreRate(const RateStats& stats, const physicallayer::Ieee80211HeMode *mode) const;
    PeerState& getPeerState(const MacAddress& peer);

  public:
    virtual const physicallayer::IIeee80211Mode *getRate() override;
    virtual void frameTransmitted(Packet *frame, int retryCount, bool isSuccessful, bool isGivenUp) override;
    virtual void frameReceived(Packet *frame) override;

    virtual Selection selectHeMode(const MacAddress& peer, Hz bandwidth, int ruToneSize,
            uint8_t ppduFormat, int maxNss, const Constraints& constraints) override;
    virtual void reportHeTxResult(const MacAddress& peer, int mcs, int numberOfSpatialStreams,
            int ruToneSize, int retryCount, bool success, int64_t ackedBytes) override;
    virtual void reportHeRxSnir(const MacAddress& peer, double snirDb) override;
};

} // namespace ieee80211
} // namespace inet

#endif
