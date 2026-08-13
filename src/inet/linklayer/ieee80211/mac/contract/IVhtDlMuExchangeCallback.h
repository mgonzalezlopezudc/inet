//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IVHTDLMUEXCHANGECALLBACK_H
#define __INET_IVHTDLMUEXCHANGECALLBACK_H

#include "inet/common/INETDefs.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfContext.h"
#include <vector>

namespace inet {
namespace queueing { class IPacketQueue; }
namespace ieee80211 {

class Ieee80211Mac;
class IOriginatorMacDataService;

/** Typed VHT DL MU operations required by the frame sequence. */
class INET_API IVhtDlMuExchangeCallback
{
  public:
    enum class UserResult { TRANSMITTED, BLOCK_ACK_RECEIVED, BLOCK_ACK_TIMED_OUT };

    virtual ~IVhtDlMuExchangeCallback() {}

    virtual Ieee80211Mac *getVhtDlMuMac() const = 0;
    virtual IOriginatorMacDataService *getVhtDlMuOriginatorDataService() const = 0;
    virtual queueing::IPacketQueue *resolveVhtDlMuQueue(
            HcfQueueToken sourceQueueToken) const = 0;
    virtual void vhtDlMuPlanCommitted(uint64_t transactionToken,
            Packet *containerPacket,
            const std::vector<std::vector<Packet *>>& userPackets) = 0;
    virtual void processVhtDlMuFailedFrame(Packet *packet) = 0;
    virtual void processVhtDlMuUserResult(uint64_t transactionToken,
            unsigned int userIndex, UserResult result) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
