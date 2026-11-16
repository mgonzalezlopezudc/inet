// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IIEEE802154MANAGEMENTSERVICEPROVIDER_H
#define __INET_IIEEE802154MANAGEMENTSERVICEPROVIDER_H

#include "inet/linklayer/ieee802154/Ieee802154MacPollRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacStartRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacStartSuperframeRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacScanRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacBeaconRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacAssociateResponse.h"
#include "inet/linklayer/ieee802154/Ieee802154MacAssociateRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacDisassociateRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacOrphanResponse.h"
#include "inet/linklayer/ieee802154/Ieee802154MacRxEnableRequest.h"

namespace inet {

/** Additional native service role; IDs and cancel/reset barriers are shared with the MAC provider. */
class INET_API IIeee802154ManagementServiceProvider
{
  public:
    virtual ~IIeee802154ManagementServiceProvider() = default;
    virtual Ieee802154MacServiceAdmission submitPoll(uint64_t id, const Ieee802154MacPollRequest& request) = 0;
    virtual Ieee802154MacServiceAdmission submitPurge(uint64_t id, uint8_t msduHandle) = 0;
    virtual Ieee802154MacServiceAdmission submitStart(uint64_t id, const Ieee802154MacStartRequest& request) = 0;
    virtual Ieee802154MacServiceAdmission submitStartSuperframe(uint64_t id, const Ieee802154MacStartSuperframeRequest& request) = 0;
    virtual Ieee802154MacServiceAdmission submitScan(uint64_t id, const Ieee802154MacScanRequest& request) = 0;
    virtual Ieee802154MacServiceAdmission submitBeacon(uint64_t id, const Ieee802154MacBeaconRequest& request) = 0;
    virtual Ieee802154MacServiceAdmission submitAssociateResponse(uint64_t id, const Ieee802154MacAssociateResponse& response) = 0;
    virtual Ieee802154MacServiceAdmission submitAssociate(uint64_t id, const Ieee802154MacAssociateRequest& request) = 0;
    virtual Ieee802154MacServiceAdmission submitDisassociate(uint64_t id, const Ieee802154MacDisassociateRequest& request) = 0;
    virtual Ieee802154MacServiceAdmission submitOrphanResponse(uint64_t id, const Ieee802154MacOrphanResponse& response) = 0;
    virtual Ieee802154MacServiceAdmission submitRxEnable(uint64_t id, const Ieee802154MacRxEnableRequest& request) = 0;
};

} // namespace inet
#endif
