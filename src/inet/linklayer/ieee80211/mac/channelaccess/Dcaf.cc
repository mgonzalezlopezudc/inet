//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/channelaccess/Dcaf.h"

#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/contract/IRx.h"
#include "inet/networklayer/common/NetworkInterface.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(Dcaf);

void Dcaf::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        getContainingNicModule(this)->subscribe(modesetChangedSignal, this);
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        // TODO calculateTimingParameters()
        pendingQueue = check_and_cast<queueing::IPacketQueue *>(getSubmodule("pendingQueue"));
        inProgressFrames = check_and_cast<InProgressFrames *>(getSubmodule("inProgressFrames"));
        contention = check_and_cast<IContention *>(getSubmodule("contention"));
        auto rx = check_and_cast<IRx *>(getModuleByPath(par("rxModule")));
        rx->registerContention(contention);
        calculateTimingParameters();
        WATCH(owning);
        WATCH(slotTime);
        WATCH(sifs);
        WATCH(ifs);
        WATCH(eifs);
        WATCH(cw);
        WATCH(cwMin);
        WATCH(cwMax);
        WATCH_EXPR("contentionState", owning ? "Owning" : (contention != nullptr && contention->isContentionInProgress()) ? "Contending" : "Idle");
    }
}

void Dcaf::calculateTimingParameters()
{
    slotTime = modeSet->getSlotTime();
    sifs = modeSet->getSifsTime();
    int difsNumber = par("difsn");
    // IEEE Std 802.11-2024, 10.3.2.3.5 and 10.3.4.2: DCF transmits after
    // DIFS when the medium is idle and the backoff counter is zero.
    // For non-DMG PHYs, DIFS is aSIFSTime + 2 * aSlotTime.
    ifs = difsNumber == -1 ? sifs + 2 * slotTime : difsNumber * slotTime;
    // IEEE Std 802.11-2024, 10.3.2.3.7: after an erroneous reception, DCF
    // defers by EIFS before returning to normal DIFS/backoff medium access.
    eifs = sifs + ifs + modeSet->getSlowestMandatoryMode()->getDuration(LENGTH_ACK);
    EV_DEBUG << "Timing parameters are initialized: slotTime = " << slotTime << ", sifs = " << sifs << ", ifs = " << ifs << ", eifs = " << eifs << std::endl;
    ASSERT(ifs > sifs);
    cwMin = par("cwMin");
    cwMax = par("cwMax");
    if (cwMin == -1)
        cwMin = modeSet->getCwMin();
    if (cwMax == -1)
        cwMax = modeSet->getCwMax();
    cw = cwMin;
    EV_DEBUG << "Contention window parameters are initialized: cw = " << cw << ", cwMin = " << cwMin << ", cwMax = " << cwMax << std::endl;
}

void Dcaf::incrementCw()
{
    Enter_Method("incrementCw");
    // IEEE Std 802.11-2024, 10.3.3: CW advances through powers of two minus
    // one after failed transmissions until capped by aCWmax.
    int newCw = 2 * cw + 1;
    if (newCw > cwMax)
        cw = cwMax;
    else
        cw = newCw;
    EV_DEBUG << "Contention window is incremented: cw = " << cw << std::endl;
}

void Dcaf::resetCw()
{
    Enter_Method("resetCw");
    // IEEE Std 802.11-2024, 10.3.3 and 10.3.4.3: successful exchanges and
    // retry-limit cases reset CW to aCWmin before the next random backoff.
    cw = cwMin;
    EV_DEBUG << "Contention window is reset: cw = " << cw << std::endl;
}

void Dcaf::channelAccessGranted()
{
    Enter_Method("channelAccessGranted");
    ASSERT(callback != nullptr);
    owning = true;
    emit(channelOwnershipChangedSignal, owning);
    callback->channelGranted(this);
}

void Dcaf::releaseChannel(IChannelAccess::ICallback *callback)
{
    Enter_Method("releaseChannel");
    owning = false;
    emit(channelOwnershipChangedSignal, owning);
    this->callback = nullptr;
    EV_INFO << "Channel released.\n";
}

void Dcaf::requestChannel(IChannelAccess::ICallback *callback)
{
    Enter_Method("requestChannel");
    this->callback = callback;
    if (owning)
        callback->channelGranted(this);
    else if (!contention->isContentionInProgress())
        // IEEE Std 802.11-2024, 10.3.4.3: random backoff uses a counter drawn
        // uniformly from [0, CW] and decrements at idle DIFS/EIFS slot times.
        contention->startContention(cw, ifs, eifs, slotTime, this);
    else
        EV_DEBUG << "Contention has been already started.\n";
}

void Dcaf::expectedChannelAccess(simtime_t time)
{
    // don't care
}

void Dcaf::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (signalID == modesetChangedSignal) {
        modeSet = check_and_cast<Ieee80211ModeSet *>(obj);
        calculateTimingParameters();
    }
}

} /* namespace ieee80211 */
} /* namespace inet */
