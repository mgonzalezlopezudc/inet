//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IVHTDLMUEXECUTIONSERVICES_H
#define __INET_IVHTDLMUEXECUTIONSERVICES_H

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfContext.h"

namespace inet {
namespace queueing { class IPacketQueue; }
namespace ieee80211 {

class Ieee80211Mac;
class IOriginatorMacDataService;

class INET_API IVhtDlMuExecutionServices
{
  public:
    virtual ~IVhtDlMuExecutionServices() = default;
    virtual Ieee80211Mac *getVhtDlMuMac() const = 0;
    virtual IOriginatorMacDataService *getVhtDlMuOriginatorDataService() const = 0;
    virtual queueing::IPacketQueue *resolveVhtDlMuQueue(
            HcfQueueToken sourceQueueToken) const = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
