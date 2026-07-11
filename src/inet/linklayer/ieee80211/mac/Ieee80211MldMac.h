//
// Copyright (C) 2024
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211MLDMAC_H
#define __INET_IEEE80211MLDMAC_H

#include "inet/linklayer/base/MacProtocolBase.h"
#include "inet/linklayer/ieee80211/mac/sequencenumberassignment/QoSSequenceNumberAssignment.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"

namespace inet {
namespace ieee80211 {

class Ieee80211Mac;

class INET_API Ieee80211MldMac : public MacProtocolBase
{
  protected:
    int numLinks = 1;
    int lowerLinkInBaseGateId = -1;
    int lowerLinkOutBaseGateId = -1;
    MacAddress mldMacAddress;
    ieee80211::QoSSequenceNumberAssignment *sequenceNumberAssignment = nullptr;
    queueing::IPacketQueue *pendingQueue[4] = {nullptr};
    int nextLinkIndex = 0;
    std::vector<Ieee80211Mac *> linkMacs;

  protected:
    virtual void initialize(int stage) override;
    virtual void configureNetworkInterface() override;

    virtual void handleUpperMessage(cMessage *message) override;
    virtual void handleLowerMessage(cMessage *message) override;

    virtual bool isUpperMessage(cMessage *message) const override;
    virtual bool isLowerMessage(cMessage *message) const override;

  public:
    virtual ~Ieee80211MldMac();
    ieee80211::QoSSequenceNumberAssignment* getSequenceNumberAssignment() const { return sequenceNumberAssignment; }
    queueing::IPacketQueue* getPendingQueue(int ac) const { return pendingQueue[ac]; }
    void registerLinkMac(Ieee80211Mac *linkMac);
    void linkTransmissionStateChanged(Ieee80211Mac *linkMac, physicallayer::IRadio::TransmissionState state);
    bool isOtherLinkTransmitting(Ieee80211Mac *thisLinkMac) const;
    bool isOtherLinkTransmittingDuring(Ieee80211Mac *thisLinkMac, simtime_t start, simtime_t end) const;
};

} // namespace ieee80211
} // namespace inet

#endif
