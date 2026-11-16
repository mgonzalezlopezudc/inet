// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IIEEE802154SUPERFRAMEPROVIDER_H
#define __INET_IIEEE802154SUPERFRAMEPROVIDER_H

#include <optional>
#include "inet/clock/contract/ClockTime.h"
#include "inet/linklayer/ieee802154/contract/IIeee802154AccessProvider.h"
#include "inet/physicallayer/wireless/ieee802154/contract/Ieee802154PhyServicePrimitives.h"

namespace inet {

/** Values copied at the actual first-symbol reception event. */
struct Ieee802154SuperframeReceptionAnchor
{
    int transmissionId;
    simtime_t physicalStart;
    clocktime_t localStart;
    simtime_t expectedEnd;
};

class INET_API IIeee802154SuperframeProvider
{
  public:
    virtual ~IIeee802154SuperframeProvider() = default;
    /** Sample the local clock at the current event, never at a historical instant. */
    virtual clocktime_t getCurrentClockTime() const = 0;
    /** Reserve a future boundary; notify the MAC one PHY turnaround before it. */
    virtual bool scheduleBeaconPreparation(clocktime_t boundary, simtime_t readinessLead) = 0;
    virtual void cancelBeaconPreparation() = 0;
    virtual bool scheduleCapEnd(clocktime_t boundary) = 0;
    virtual void cancelCapEnd() = 0;
    virtual void scheduleSyncWake(clocktime_t when) = 0;
    virtual void scheduleSyncDeadline(clocktime_t when) = 0;
    virtual void cancelSyncEvents() = 0;
    /** Publish only a window derived from an actual beacon; expiry is checked on lookup. */
    virtual Ieee802154AccessWindow publishAccessWindow(const Ieee802154AccessWindow& window) = 0;
    virtual std::optional<Ieee802154AccessWindow> getAccessWindow() const = 0;
    virtual void clearAccessWindow() = 0;
    virtual void scheduleAccessSearchDeadline(clocktime_t when) = 0;
    virtual void cancelAccessSearchDeadline() = 0;
    virtual simtime_t getFutureSimulationTime(clocktime_t target) const = 0;
    virtual clocktime_t getFutureClockTime(simtime_t target) const = 0;
    virtual void recordReceptionStart(const physicallayer::Ieee802154PhyReceiveStart& start) = 0;
    virtual std::optional<Ieee802154SuperframeReceptionAnchor> takeReceptionAnchor(int transmissionId) = 0;
    virtual void retireExpiredReceptionAnchors() = 0;
    virtual void clearReceptionAnchors() = 0;
};

} // namespace inet
#endif
