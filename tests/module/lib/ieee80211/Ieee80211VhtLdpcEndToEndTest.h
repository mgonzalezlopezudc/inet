//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211VHTLDPCENDTOENDTEST_H
#define __INET_IEEE80211VHTLDPCENDTOENDTEST_H

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211LdpcPerSuccessModel.h"

namespace inet {

namespace physicallayer {
class Ieee80211DataEncodingPlan;
class IIeee80211DataMode;
}

namespace ieee80211 {
class Ieee80211Mib;
}

class INET_API RecordingIeee80211LdpcPerSuccessModel : public physicallayer::Ieee80211LdpcPerSuccessModel
{
  protected:
    mutable int dataEvaluationCount = 0;

  public:
    virtual double computeDataSuccessRate(const physicallayer::IIeee80211DataMode& mode,
            const physicallayer::Ieee80211DataEncodingPlan& plan, double snrDb) const override;

    int getDataEvaluationCount() const { return dataEvaluationCount; }
};

class INET_API Ieee80211VhtLdpcEndToEndTest : public SimpleModule, public cListener
{
    using cListener::finish;

  protected:
    cModule *stationRadio = nullptr;
    cModule *stationDcf = nullptr;
    cModule *serverApp = nullptr;
    RecordingIeee80211LdpcPerSuccessModel *fecSuccessModel = nullptr;
    ieee80211::Ieee80211Mib *apMib = nullptr;
    MacAddress stationAddress;
    MacAddress bssid;
    int expectedAssociationId = -1;
    int expectedPartialAid = -1;
    int onAirLdpcDataCount = 0;
    int macLdpcDataCount = 0;
    int retriedLdpcDataCount = 0;
    int ldpcAckCount = 0;
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
