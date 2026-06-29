//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"

#include <algorithm>
#include <sstream>

#include "inet/common/INETMath.h"
#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeSoundingFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatTransmitterBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HePreamblePuncturing.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTwtGating.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingCoordinator.h"

// HE HCF coordination function.
//
// This module implements the 802.11ax AP and non-AP STA behavior for DL/UL OFDMA
// and MU-MIMO TXOPs.  The relevant normative text is in IEEE 802.11-2024:
//   - Clause 26.1 / 26.2: HE introduction, channel access and TXOP rules.
//   - Clause 26.5: UL multi-user operation (Trigger frames, HE TB PPDU response).
//   - Clause 26.5.4: Uplink OFDMA random access (UORA).
//   - Clause 26.4.4: frame exchange rules for HE MU and HE TB PPDUs.
//   - Clause 27.3.11: HE PPDU formats (HE SU, HE ER SU, HE MU, HE TB).
//   - Clause 27.3.11.13: HE MU PPDU format and HE-SIG-B.
//   - Clause 27.3.11.12: HE TB PPDU format.
//
// Implementation notes / deviations from the standard:
//   - DL MU scheduling is restricted to QoS data frames that already have an
//     active originator Block Ack agreement.  The standard does not require a
//     Block Ack agreement before scheduling a DL MU PPDU, but INET's model
//     uses A-MPDU aggregation and per-TID reordering, so this is an
//     implementation-enforced precondition, not a normative one.
//   - Per-STA queue banks are used to keep frames destined to different STAs
//     separable; this is an INET-specific queuing architecture.
//   - The BSRP trigger allocation assigns one RU per associated STA plus the
//     remaining RUs as random-access RUs.  The standard permits many other RU
//     allocation strategies for BSRP; this is a simple approximation.
//   - UORA is modeled with a per-BSS global OCW and uniform RA-RU selection.
//     The standard uses per-AC OCW state and a more involved selection rule;
//     the current model is conservative in that it follows the OCW update
//     procedure but collapses per-AC state.
//   - UL basic trigger responses use single-TID A-MPDUs only; multi-TID
//     aggregation in HE TB PPDUs is not implemented.

namespace inet {
namespace ieee80211 {

Define_Module(HeHcf);

HeHcf::~HeHcf()
{
    cancelAndDelete(ulTriggerTimer);
    for (auto& entry : triggeredUlExchanges) {
        for (auto pkt : entry.second.packets) {
            delete pkt;
        }
    }
}

void HeHcf::initialize(int stage)
{
    Hcf::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        dlScheduler = check_and_cast<IIeee80211HeDlScheduler *>(getSubmodule("dlScheduler"));
        ulCoordinator = check_and_cast<HeUlCoordinator *>(getSubmodule("ulCoordinator"));
        ulTriggerTimer = new cMessage("heUlTriggerTimer");
        delete frameSequenceHandler;
        frameSequenceHandler = new HeFrameSequenceHandler();

        enableDlMuMimo = par("enableDlMuMimo").boolValue();
        csiValidityDuration = par("csiValidityDuration");
        defaultCsiLeakage = par("defaultCsiLeakage");
        csiLeakageOverrides = par("csiLeakageOverrides").stdstringValue();
        csiManager.configure(csiValidityDuration, defaultCsiLeakage, csiLeakageOverrides);

        WATCH(pendingUlTrigger);
        WATCH(ulTriggerAccessRequested);
        WATCH(forceNextSingleUser[0]);
        WATCH(forceNextSingleUser[1]);
        WATCH(forceNextSingleUser[2]);
        WATCH(forceNextSingleUser[3]);
        WATCH_MAP(triggeredUlExchanges);
        WATCH_EXPR("pendingUlTriggerName", getPendingUlTriggerName());
        WATCH_EXPR("stationQueueBanks", getStationQueueBankCount());
        WATCH_EXPR("triggeredUlExchangeCount", triggeredUlExchanges.size());
        WATCH_EXPR("heHcfSummary", getHeHcfSummary());
    }
    else if (stage == INITSTAGE_LINK_LAYER && mac->isApInAxMode()) {
        queueBankManager = std::make_unique<StationQueueBankManager>(getSubmodule("queueBanks"));
        for (const auto& station : mac->getMib()->bssAccessPointData.stations) {
            if (station.second == Ieee80211Mib::ASSOCIATED)
                queueBankManager->createQueueBank(station.first);
        }
        WATCH_EXPR("csiTableSummary", getCsiTableSummary());
        if (ulCoordinator->isEnabled())
            scheduleAfter(par("ulTriggerCheckInterval"), ulTriggerTimer);
    }
}


AccessCategory HeHcf::mapTidToAccessCategory(Tid tid) const
{
    switch (tid) {
        case 1:
        case 2: return AC_BK;
        case 0:
        case 3: return AC_BE;
        case 4:
        case 5: return AC_VI;
        case 6:
        case 7: return AC_VO;
        default: throw omnetpp::cRuntimeError("Invalid TID for HE UL scheduling: %d", tid);
    }
}


const char *HeHcf::getPendingUlTriggerName() const
{
    switch (pendingUlTrigger) {
        case IIeee80211HeUlTriggerPolicy::NO_TRIGGER: return "NO_TRIGGER";
        case IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER: return "BASIC_TRIGGER";
        case IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER: return "BSRP_TRIGGER";
        default: return "UNKNOWN";
    }
}

int HeHcf::getStationQueueBankCount() const
{
    return queueBankManager == nullptr ? 0 : queueBankManager->getQueueBanks().size();
}

std::string HeHcf::getCsiTableSummary() const
{
    int validEntries = 0;
    for (const auto& entry : csiManager.csiTable)
        if (entry.second.valid && simTime() <= entry.second.expiryTime)
            validEntries++;
    std::stringstream stream;
    stream << "entries=" << csiManager.csiTable.size()
           << ", valid=" << validEntries;
    return stream.str();
}

std::string HeHcf::getHeHcfSummary() const
{
    std::stringstream stream;
    stream << "DL scheduler=" << (dlScheduler == nullptr ? "none" : "configured")
           << ", UL coordinator=" << (ulCoordinator != nullptr && ulCoordinator->isEnabled() ? "enabled" : "disabled")
           << ", pendingTrigger=" << getPendingUlTriggerName()
           << ", queueBanks=" << getStationQueueBankCount()
           << ", triggeredUL=" << triggeredUlExchanges.size()
           << ", dlMuMimo=" << (enableDlMuMimo ? "enabled" : "disabled")
           << ", csiEntries=" << csiManager.csiTable.size();
    return stream.str();
}

void HeHcf::finish()
{
    if (queueBankManager != nullptr) {
        for (const auto& pair : queueBankManager->getQueueBanks()) {
            pair.second->clear();
        }
    }
    cSimpleModule::finish();
}

void HeHcf::handleMessage(cMessage *msg)
{
    if (msg != ulTriggerTimer) {
        Hcf::handleMessage(msg);
        return;
    }
    scheduleAfter(par("ulTriggerCheckInterval"), ulTriggerTimer);
    if (!mac->isApInAxMode() || !ulCoordinator->isEnabled() ||
            frameSequenceHandler->isSequenceRunning() || edca->getChannelOwner() != nullptr ||
            tx->isBusy() || ulTriggerAccessRequested)
        return;
    auto triggerType = ulCoordinator->selectTrigger(mac->getMib());
    if (triggerType == IIeee80211HeUlTriggerPolicy::NO_TRIGGER)
        return;
    EV_INFO << "Requesting channel access for HE UL "
             << (triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ? "BSRP" : "Basic")
             << " Trigger\n";
    pendingUlTrigger = triggerType;
    ulTriggerAccessRequested = true;
    auto ac = triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ?
            AC_BE : ulCoordinator->getPreferredAccessCategory();
    edca->requestChannelAccess(ac, this);
}

queueing::IPacketQueue *HeHcf::getPerStaQueue(const MacAddress& staAddr, AccessCategory ac)
{
    if (queueBankManager != nullptr) {
        auto staBank = queueBankManager->getQueueBank(staAddr);
        if (staBank != nullptr) {
            auto staQueue = staBank->getQueue((StationQueueBank::AccessCategory)ac);
            if (staQueue != nullptr) {
                EV_DEBUG << "Using per-STA queue for STA " << staAddr << " AC " << ac << "\n";
                return staQueue;
            }
            EV_WARN << "Could not get per-STA queue for STA " << staAddr << " AC " << ac << ", using shared queue\n";
        }
        else
            EV_DEBUG << "Queue bank not found for STA " << staAddr << ", using shared queue\n";
    }
    else
        EV_DEBUG << "Queue bank manager not available, using shared queue\n";
    return Hcf::getPerStaQueue(staAddr, ac);
}

StationQueueBank *HeHcf::createStationQueueBank(const MacAddress& staAddr)
{
    if (queueBankManager == nullptr) {
        EV_WARN << "Queue bank manager not initialized (not an 802.11ax AP?)\n";
        return nullptr;
    }
    return queueBankManager->createQueueBank(staAddr);
}

void HeHcf::destroyStationQueueBank(const MacAddress& staAddr)
{
    if (queueBankManager == nullptr) {
        EV_WARN << "Queue bank manager not initialized (not an 802.11ax AP?)\n";
        return;
    }
    queueBankManager->destroyQueueBank(staAddr);
}

StationQueueBank *HeHcf::getStationQueueBank(const MacAddress& staAddr) const
{
    return queueBankManager == nullptr ? nullptr : queueBankManager->getQueueBank(staAddr);
}

bool HeHcf::releaseChannelIfNoFallbackFrame(AccessCategory ac)
{
    auto fallbackEdcaf = edca->getEdcaf(ac);
    if (fallbackEdcaf->getPendingQueue()->isEmpty() &&
            fallbackEdcaf->getInProgressFrames()->getLength() == 0)
        stagePerStaFrameForSingleUserTransmission(ac);
    if (fallbackEdcaf->getInProgressFrames()->getFrameToTransmit() != nullptr)
        return false;

    EV_WARN << "Channel granted without an eligible SU, DL-MU, or UL trigger frame; releasing channel.\n";
    fallbackEdcaf->releaseChannel(this);
    fallbackEdcaf->getTxopProcedure()->endTxop();
    return true;
}

void HeHcf::startFrameSequence(AccessCategory ac)
{
    if (forceNextSingleUser[ac]) {
        EV_INFO << "Start FS: forced single-user TXOP for AC " << ac << "\n";
        forceNextSingleUser[ac] = false;
        Hcf::startFrameSequence(ac);
        return;
    }

    ASSERT(modeSet != nullptr);
    bool isHeMode = strcmp(modeSet->getName(), "ax") == 0;
    if (isHeMode) {
        if (tryStartUlMuFrameSequence(ac))
            return;
        if (tryStartDlMuFrameSequence(ac))
            return;
    }
    else
        EV_INFO << "Non-HE mode, falling back to SU\n";

    if (releaseChannelIfNoFallbackFrame(ac))
        return;
    Hcf::startFrameSequence(ac);
}

void HeHcf::handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs)
{
    std::vector<Edcaf *> collidedEdcafsWithFrame;
    for (auto edcaf : internallyCollidedEdcafs) {
        if (edcaf->getPendingQueue()->isEmpty() && edcaf->getInProgressFrames()->getLength() == 0)
            stagePerStaFrameForSingleUserTransmission(edcaf->getAccessCategory());
        if (edcaf->getInProgressFrames()->getFrameToTransmit() != nullptr)
            collidedEdcafsWithFrame.push_back(edcaf);
    }
    if (!collidedEdcafsWithFrame.empty())
        Hcf::handleInternalCollision(collidedEdcafsWithFrame);
}

bool HeHcf::hasFrameToTransmit(AccessCategory ac)
{
    if (Hcf::hasFrameToTransmit(ac))
        return true;
    if (queueBankManager == nullptr)
        return false;
    for (const auto& entry : queueBankManager->getQueueBanks()) {
        if (!entry.second->getQueue((StationQueueBank::AccessCategory)ac)->isEmpty())
            return true;
    }
    return false;
}

bool HeHcf::hasFrameToTransmit()
{
    auto edcaf = edca->getChannelOwner();
    return edcaf != nullptr && hasFrameToTransmit(edcaf->getAccessCategory());
}

uint16_t HeHcf::getAssociationId(const MacAddress& address) const
{
    auto aid = mac->getMib()->getAssociationId(address);
    return aid > 0 ? aid : 0;
}

} // namespace ieee80211
} // namespace inet
