//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCSOFTENDTOENDTEST_H
#define __INET_IEEE80211LDPCSOFTENDTOENDTEST_H

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/ieee80211/mgmt/contract/IIeee80211PeerCapabilities.h"

namespace inet {

class INET_API Ieee80211LdpcSoftEndToEndTest : public SimpleModule, public cListener,
        public ieee80211::IIeee80211PeerCapabilities
{
    using cListener::finish;

  protected:
    cModule *stationRadio = nullptr;
    cModule *stationDcf = nullptr;
    cModule *stationRecovery = nullptr;
    cModule *serverApp = nullptr;
    cModule *stationTransmitter = nullptr;
    cModule *apReceiver = nullptr;
    bool expectedExact = false;
    bool expectedSuccess = false;
    bool expectedVht = false;
    int expectedAttempts = -1;
    int minimumTargetPsduOctets = -1;
    int onAirDataCount = 0;
    int macDataCount = 0;
    int retriedDataCount = 0;
    int ackCount = 0;
    int retryLimitDropCount = 0;
    int deliveryCount = 0;
    int encodeInvocations = 0;
    int decodeInvocations = 0;
    int decodeConvergences = 0;
    long decodeIterations = 0;
    bool awaitingAck = false;

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;
    virtual void finish() override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *object,
            cObject *details) override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, long value,
            cObject *details) override;

  public:
    // Deterministic test-only peer view for management-free ad-hoc fixtures.
    virtual ieee80211::Ieee80211PeerLdpcStatus getPeerLdpcStatus(
            const MacAddress& peer) const override;
    virtual ieee80211::Ieee80211IntendedReceiverSet resolveIntendedReceivers(
            const MacAddress& receiverAddress) const override;
    virtual ieee80211::Ieee80211VhtSigAParameters getVhtSigAParameters(
            const MacAddress& receiverAddress) const override;
};

} // namespace inet

#endif
