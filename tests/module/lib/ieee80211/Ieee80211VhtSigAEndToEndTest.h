//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211VHTSIGAENDTOENDTEST_H
#define __INET_IEEE80211VHTSIGAENDTOENDTEST_H

#include "inet/common/SimpleModule.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {

namespace ieee80211 {
class Ieee80211Mib;
class IIeee80211PeerCapabilities;
}

class INET_API Ieee80211VhtSigAEndToEndTest : public SimpleModule, public cListener
{
    using cListener::finish;

  protected:
    cModule *stationRadio = nullptr;
    cModule *apRadio = nullptr;
    cModule *uplinkSink = nullptr;
    cModule *downlinkSink = nullptr;
    ieee80211::Ieee80211Mib *apMib = nullptr;
    ieee80211::IIeee80211PeerCapabilities *stationPeerCapabilities = nullptr;
    ieee80211::IIeee80211PeerCapabilities *apPeerCapabilities = nullptr;
    MacAddress stationAddress;
    MacAddress bssid;
    int expectedAssociationId = -1;
    int expectedStaToApPartialAid = -1;
    int expectedApToStaPartialAid = -1;
    bool expectedLdpc = false;
    bool expectedAdhoc = false;
    bool verifyMissingAssociationIdInvariant = false;
    int staToApTransmissionCount = 0;
    int apToStaTransmissionCount = 0;
    int uplinkDeliveryCount = 0;
    int downlinkDeliveryCount = 0;

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;
    virtual void finish() override;
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *object, cObject *details) override;
};

} // namespace inet

#endif
