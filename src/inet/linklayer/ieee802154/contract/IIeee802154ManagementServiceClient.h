// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IIEEE802154MANAGEMENTSERVICECLIENT_H
#define __INET_IIEEE802154MANAGEMENTSERVICECLIENT_H

#include "inet/linklayer/ieee802154/Ieee802154MacPollRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacStartRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacStartSuperframeRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacSyncRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacScanRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacBeaconRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacAssociateResponse.h"
#include "inet/linklayer/ieee802154/Ieee802154MacAssociateRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacDisassociateRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154MacOrphanResponse.h"
#include "inet/linklayer/ieee802154/Ieee802154MacRxEnableRequest.h"

namespace inet {

/** Borrowed confirmation values; providers detach terminal state before invoking callbacks. */
class INET_API IIeee802154ManagementServiceClient
{
  public:
    virtual ~IIeee802154ManagementServiceClient() = default;
    virtual void handlePollConfirm(uint64_t id, const Ieee802154MacPollConfirm& confirm) = 0;
    virtual void handlePurgeConfirm(uint64_t id, const Ieee802154MacPurgeConfirm& confirm) = 0;
    virtual void handleStartConfirm(uint64_t id, const Ieee802154MacStartConfirm& confirm) = 0;
    virtual void handleStartSuperframeConfirm(uint64_t id, const Ieee802154MacStartSuperframeConfirm& confirm) = 0;
    virtual void handleSyncConfirm(uint64_t id, const Ieee802154MacSyncConfirm& confirm) = 0;
    virtual void handleScanConfirm(uint64_t id, const Ieee802154MacScanConfirm& confirm) = 0;
    virtual void handleBeaconNotifyIndication(const Ieee802154PanDescriptor& descriptor) = 0;
    virtual void handleBeaconConfirm(uint64_t id, const Ieee802154MacBeaconConfirm& confirm) = 0;
    virtual void handleBeaconRequestIndication(const Ieee802154MacBeaconRequestIndication& indication) = 0;
    virtual void handleAssociateResponseStatus(uint64_t id, const Ieee802154MacAssociateResponseStatus& status) = 0;
    virtual void handleAssociateConfirm(uint64_t id, const Ieee802154MacAssociateConfirm& confirm) = 0;
    virtual void handleAssociateIndication(const Ieee802154MacAssociateIndication& indication) = 0;
    virtual void handleDisassociateConfirm(uint64_t id, const Ieee802154MacDisassociateConfirm& confirm) = 0;
    virtual void handleDisassociateIndication(const Ieee802154MacDisassociateIndication& indication) = 0;
    virtual void handleOrphanIndication(const Ieee802154MacOrphanIndication& indication) = 0;
    virtual void handleOrphanResponseStatus(uint64_t id, const Ieee802154MacOrphanResponseStatus& status) = 0;
    virtual void handleSyncLossIndication(const Ieee802154MacSyncLossIndication& indication) = 0;
    virtual void handleRxEnableConfirm(uint64_t id, const Ieee802154MacRxEnableConfirm& confirm) = 0;
    virtual void handleRxEnableIndication(const Ieee802154MacRxEnableIndication& indication) = 0;
};

} // namespace inet
#endif
