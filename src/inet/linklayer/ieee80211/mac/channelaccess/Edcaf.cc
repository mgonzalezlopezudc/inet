//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/Simsignals.h"
#include "inet/networklayer/common/NetworkInterface.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(Edcaf);

inline double fallback(double a, double b) { return a != -1 ? a : b; }
inline simtime_t fallback(simtime_t a, simtime_t b) { return a != -1 ? a : b; }

Edcaf::~Edcaf()
{
    delete stationRetryCounters;
    if (muEdcaTimer != nullptr) {
        auto edca = dynamic_cast<omnetpp::cSimpleModule *>(getParentModule());
        if (edca != nullptr) {
            edca->cancelAndDelete(muEdcaTimer);
        } else {
            delete muEdcaTimer;
        }
    }
}

void Edcaf::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        getContainingNicModule(this)->subscribe(modesetChangedSignal, this);
        ac = getAccessCategory(par("accessCategory"));
        contention = check_and_cast<IContention *>(getSubmodule("contention"));
        collisionController = check_and_cast<IEdcaCollisionController *>(getModuleByPath(par("collisionControllerModule")));
        std::string pendingQueueModuleStr = par("pendingQueueModule").stringValue();
        if (pendingQueueModuleStr.empty()) {
            pendingQueue = check_and_cast<queueing::IPacketQueue *>(getSubmodule("pendingQueue"));
        } else {
            pendingQueue = check_and_cast<queueing::IPacketQueue *>(getModuleByPath(pendingQueueModuleStr.c_str()));
        }
        recoveryProcedure = check_and_cast<QosRecoveryProcedure *>(getSubmodule("recoveryProcedure"));
        ackHandler = check_and_cast<QosAckHandler *>(getSubmodule("ackHandler"));
        inProgressFrames = check_and_cast<InProgressFrames *>(getSubmodule("inProgressFrames"));
        txopProcedure = check_and_cast<TxopProcedure *>(getSubmodule("txopProcedure"));
        stationRetryCounters = new StationRetryCounters();
        muEdcaTimer = nullptr;
        isMuEdcaTimerActive = false;
        WATCH(owning);
        WATCH(slotTime);
        WATCH(sifs);
        WATCH(ifs);
        WATCH(eifs);
        WATCH(ac);
        WATCH(cw);
        WATCH(cwMin);
        WATCH(cwMax);
        WATCH(isMuEdcaTimerActive);
        WATCH_EXPR("accessCategory", printAccessCategory(ac));
        WATCH_EXPR("contentionState", owning ? "Owning" : contention->isContentionInProgress() ? "Contending" : "Idle");
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        auto rx = check_and_cast<IRx *>(getModuleByPath(par("rxModule")));
        rx->registerContention(contention);
        calculateTimingParameters();
    }
}

void Edcaf::calculateTimingParameters()
{
    slotTime = modeSet->getSlotTime();
    sifs = modeSet->getSifsTime();
    int aifsnVal = par("aifsn");
    int cwMinVal = par("cwMin");
    int cwMaxVal = par("cwMax");

    if (isMuEdcaTimerActive) {
        int muAifsn = par("muAifsn");
        int muCwMin = par("muCwMin");
        int muCwMax = par("muCwMax");
        if (muAifsn != -1) aifsnVal = muAifsn;
        if (muCwMin != -1) cwMinVal = muCwMin;
        if (muCwMax != -1) cwMaxVal = muCwMax;
    }

    // IEEE Std 802.11-2024, 10.3.2.3.6 and 10.23.2.4:
    // AIFS[AC] = AIFSN[AC] * aSlotTime + aSIFSTime.
    simtime_t aifs = sifs + fallback(aifsnVal, getAifsNumber(ac)) * slotTime;
    ifs = aifs;
    // IEEE Std 802.11-2024, 10.3.2.3.7: after erroneous reception, EDCA uses
    // EIFS-DIFS+AIFS[AC]; this implementation folds that interval into eifs.
    eifs = sifs + aifs + modeSet->getSlowestMandatoryMode()->getDuration(LENGTH_ACK);
    EV_DEBUG << "Timing parameters are initialized: slotTime = " << slotTime << ", sifs = " << sifs << ", ifs = " << ifs << ", eifs = " << eifs << std::endl;
    ASSERT(ifs > sifs);
    if (cwMinVal == -1)
        cwMin = getCwMin(ac, modeSet->getCwMin());
    else
        cwMin = cwMinVal;
    if (cwMaxVal == -1)
        cwMax = getCwMax(ac, modeSet->getCwMax(), modeSet->getCwMin());
    else
        cwMax = cwMaxVal;
    if (cw < cwMin || cw > cwMax)
        cw = cwMin;
    EV_DEBUG << "Contention window parameters are initialized: cw = " << cw << ", cwMin = " << cwMin << ", cwMax = " << cwMax << std::endl;
}

void Edcaf::incrementCw()
{
    Enter_Method("incrementCw");
    // IEEE Std 802.11-2024, 10.23.2.2: failed EDCA attempts update CW[AC] to
    // min(CWmax[AC], 2^QSRC[AC] * (CWmin[AC] + 1) - 1).
    int newCw = 2 * cw + 1;
    if (newCw > cwMax)
        cw = cwMax;
    else
        cw = newCw;
    EV_DEBUG << "Contention window is incremented: cw = " << cw << std::endl;
}

void Edcaf::resetCw()
{
    Enter_Method("resetCw");
    // IEEE Std 802.11-2024, 10.23.2.2: successful final TXOP completion resets
    // CW[AC] to CWmin[AC].
    cw = cwMin;
    EV_DEBUG << "Contention window is reset: cw = " << cw << std::endl;
}

int Edcaf::getAifsNumber(AccessCategory ac)
{
    // IEEE Std 802.11-2024, 9.4.2.27 and 10.3.2.3.6: defaults are used until
    // EDCA Parameter Set values override dot11EDCATableAIFSN.
    switch (ac) {
        case AC_BK: return 7;
        case AC_BE: return 3;
        case AC_VI: return 2;
        case AC_VO: return 2;
        default: throw cRuntimeError("Unknown access category = %d", ac);
    }
}

AccessCategory Edcaf::getAccessCategory(const char *ac)
{
    if (strcmp("AC_BK", ac) == 0)
        return AC_BK;
    if (strcmp("AC_VI", ac) == 0)
        return AC_VI;
    if (strcmp("AC_VO", ac) == 0)
        return AC_VO;
    if (strcmp("AC_BE", ac) == 0)
        return AC_BE;
    throw cRuntimeError("Unknown Access Category = %s", ac);
}

void Edcaf::channelAccessGranted()
{
    Enter_Method("channelAccessGranted");
    ASSERT(callback != nullptr);
    if (!collisionController->isInternalCollision(this)) {
        owning = true;
        emit(channelOwnershipChangedSignal, owning);
        callback->channelGranted(this);
    }
    else
        EV_WARN << "Ignoring channel access granted due to internal collision.\n";
}

void Edcaf::releaseChannel(IChannelAccess::ICallback *callback)
{
    Enter_Method("releaseChannel");
    ASSERT(owning);
    owning = false;
    emit(channelOwnershipChangedSignal, owning);
    this->callback = nullptr;
    EV_INFO << "Channel released.\n";
}

void Edcaf::requestChannel(IChannelAccess::ICallback *callback)
{
    Enter_Method("requestChannel");
    this->callback = callback;
    ASSERT(!owning);
    if (contention->isContentionInProgress())
        EV_DEBUG << "Contention has been already started.\n";
    else
        // IEEE Std 802.11-2024, 10.23.2.4: an EDCAF with a queued frame starts
        // contention using its AC-specific CW, AIFS and slot timing.
        contention->startContention(cw, ifs, eifs, slotTime, this);
}

void Edcaf::expectedChannelAccess(simtime_t time)
{
    collisionController->expectedChannelAccess(this, time);
}

bool Edcaf::isInternalCollision()
{
    return collisionController->isInternalCollision(this);
}

int Edcaf::getCwMax(AccessCategory ac, int aCwMax, int aCwMin)
{
    // IEEE Std 802.11-2024, default EDCA parameter tables define AC-specific
    // CWmax values relative to aCWmax/aCWmin.
    switch (ac) {
        case AC_BK: return aCwMax;
        case AC_BE: return aCwMax;
        case AC_VI: return aCwMin;
        case AC_VO: return (aCwMin + 1) / 2 - 1;
        default: throw cRuntimeError("Unknown access category = %d", ac);
    }
}

int Edcaf::getCwMin(AccessCategory ac, int aCwMin)
{
    // IEEE Std 802.11-2024, default EDCA parameter tables define AC-specific
    // CWmin values as fractions of aCWmin for AC_VI and AC_VO.
    switch (ac) {
        case AC_BK: return aCwMin;
        case AC_BE: return aCwMin;
        case AC_VI: return (aCwMin + 1) / 2 - 1;
        case AC_VO: return (aCwMin + 1) / 4 - 1;
        default: throw cRuntimeError("Unknown access category = %d", ac);
    }
}

void Edcaf::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (signalID == modesetChangedSignal) {
        modeSet = check_and_cast<Ieee80211ModeSet *>(obj);
        calculateTimingParameters();
    }
}

void Edcaf::startMuEdcaTimer()
{
    double timerDuration = par("muEdcaTimer").doubleValue();
    if (timerDuration <= 0)
        return;

    auto edca = check_and_cast<omnetpp::cSimpleModule *>(getParentModule());
    omnetpp::cContextSwitcher switcher(edca);

    if (muEdcaTimer == nullptr) {
        std::string name = "MU-EDCA-Timer-";
        switch (ac) {
            case AC_BK: name += "BK"; break;
            case AC_BE: name += "BE"; break;
            case AC_VI: name += "VI"; break;
            case AC_VO: name += "VO"; break;
            default: break;
        }
        muEdcaTimer = new omnetpp::cMessage(name.c_str());
    } else if (muEdcaTimer->isScheduled()) {
        edca->cancelEvent(muEdcaTimer);
    }

    isMuEdcaTimerActive = true;
    edca->scheduleAt(simTime() + timerDuration, muEdcaTimer);
    calculateTimingParameters();
}

void Edcaf::muEdcaTimerExpired()
{
    isMuEdcaTimerActive = false;
    muEdcaTimer = nullptr;
    calculateTimingParameters();
}

} // namespace ieee80211
} // namespace inet
