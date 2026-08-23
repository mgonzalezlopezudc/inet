//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCENDTOENDTEST_H
#define __INET_IEEE80211LDPCENDTOENDTEST_H

#include "inet/common/Module.h"
#include "inet/common/SimpleModule.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/contract/IIeee80211FecSuccessModel.h"

namespace inet {

class INET_API DeterministicIeee80211FecSuccessModel : public Module, public physicallayer::IIeee80211FecSuccessModel
{
  protected:
    double targetDataSuccessRate = 0;
    int minimumTargetUncodedBits = -1;
    bool expectedVht = false;
    mutable int targetEvaluationCount = 0;

  protected:
    virtual void initialize(int stage) override;
    virtual void finish() override;

  public:
    virtual double computeDataSuccessRate(const physicallayer::IIeee80211DataMode& mode,
            const physicallayer::Ieee80211DataEncodingPlan& plan, double snrDb) const override;

    int getTargetEvaluationCount() const { return targetEvaluationCount; }
};

class INET_API Ieee80211LdpcEndToEndTest : public SimpleModule, public cListener
{
    using cListener::finish;

  protected:
    cModule *stationRadio = nullptr;
    cModule *stationDcf = nullptr;
    cModule *stationRecovery = nullptr;
    cModule *serverApp = nullptr;
    DeterministicIeee80211FecSuccessModel *fecSuccessModel = nullptr;
    bool expectedSuccess = false;
    int expectedAttempts = -1;
    int minimumTargetUncodedBits = -1;
    int onAirLdpcDataCount = 0;
    int macLdpcDataCount = 0;
    int retriedLdpcDataCount = 0;
    int ldpcAckCount = 0;
    int retryLimitDropCount = 0;
    int deliveryCount = 0;
    bool awaitingLdpcAck = false;

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;
    virtual void finish() override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *object, cObject *details) override;
};

} // namespace inet

#endif
