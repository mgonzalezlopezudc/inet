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

namespace {

/** The sole projection point from concrete IEEE 802.11 radio/MIB state. */
class Ieee80211HeLinkPhyContext : public IIeee80211HeLinkPhyContext
{
  private:
    HeHcf *owner;
    Ieee80211Mac *mac;

  public:
    Ieee80211HeLinkPhyContext(HeHcf *owner, Ieee80211Mac *mac) :
        owner(owner),
        mac(mac)
    {
    }

    virtual Ieee80211HeLinkPhySnapshot getSnapshot() const override
    {
        auto nic = getContainingNicModule(owner);
        auto radio = check_and_cast<const physicallayer::IRadio *>(nic->getSubmodule("radio"));
        auto transmitter = check_and_cast<const physicallayer::Ieee80211Transmitter *>(radio->getTransmitter());
        auto receiver = check_and_cast<const physicallayer::FlatReceiverBase *>(radio->getReceiver());
        auto channel = transmitter->getChannel();
        auto activeMode = transmitter->getMode();
        if (channel == nullptr || activeMode == nullptr)
            throw cRuntimeError("HE planning requires an active IEEE 802.11 channel and mode");
        auto bandwidth = activeMode->getDataMode()->getBandwidth();
        auto heMode = dynamic_cast<const physicallayer::Ieee80211HeMode *>(activeMode);
        if (heMode == nullptr) {
            auto modeSet = transmitter->getModeSet();
            auto matchingMode = modeSet == nullptr ? nullptr :
                    modeSet->findHeMode(0, 1, bandwidth, bandwidth > MHz(20));
            heMode = dynamic_cast<const physicallayer::Ieee80211HeMode *>(matchingMode);
        }
        if (heMode == nullptr)
            throw cRuntimeError("HE planning requires an HE mode matching the active channel bandwidth");

        physicallayer::Ieee80211HeGuardInterval guardInterval;
        switch (heMode->getDataMode()->getGuardIntervalType()) {
            case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_SHORT:
                guardInterval = physicallayer::HE_GI_0_8_US;
                break;
            case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_MEDIUM:
                guardInterval = physicallayer::HE_GI_1_6_US;
                break;
            case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG:
                guardInterval = physicallayer::HE_GI_3_2_US;
                break;
            default:
                throw cRuntimeError("Unsupported active HE guard interval");
        }
        auto puncturedSubchannels = resolveHePreamblePuncturing(owner, bandwidth);
        uint8_t puncturedSubchannelMask = 0;
        for (size_t i = 0; i < puncturedSubchannels.size(); ++i)
            if (puncturedSubchannels[i])
                puncturedSubchannelMask |= 1U << i;
        auto mib = mac->getMib();
        if (mib == nullptr)
            throw cRuntimeError("HE planning requires an initialized IEEE 802.11 MIB");
        return Ieee80211HeLinkPhySnapshot(channel->getChannelNumber(), channel->getCenterFrequency(),
                bandwidth, transmitter->getPower(), transmitter->getMaxPower(), receiver->getSensitivity(),
                owner->par("receiverNoiseFigure").doubleValue(), radio->getAntenna()->getNumAntennas(),
                guardInterval, physicallayer::getHeDefaultLtfType(guardInterval),
                mib->heOperation.defaultPeDurationUs, puncturedSubchannels,
                puncturedSubchannelMask, mib->localHeCapabilities);
    }

    virtual Ieee80211HePeerLinkSnapshot getPeerSnapshot(const MacAddress& address,
            simtime_t maximumLinkEstimateAge) const override
    {
        auto mib = mac->getMib();
        if (mib == nullptr)
            throw cRuntimeError("HE peer projection requires an initialized IEEE 802.11 MIB");
        auto advertisement = mib->bssAccessPointData.advertisedHeCapabilities.find(address);
        auto negotiated = mib->findNegotiatedHeCapabilities(address);
        auto link = mib->findStationLink(address);
        auto pathLossDb = link == nullptr ? NaN : link->pathLossDb;
        auto hasFreshPathLoss = link != nullptr && link->valid &&
                simTime() - link->lastUpdate <= maximumLinkEstimateAge;
        return Ieee80211HePeerLinkSnapshot(
                advertisement != mib->bssAccessPointData.advertisedHeCapabilities.end(),
                advertisement == mib->bssAccessPointData.advertisedHeCapabilities.end() ?
                        Ieee80211HeCapabilities() : advertisement->second,
                negotiated != nullptr,
                negotiated == nullptr ? Ieee80211NegotiatedHeCapabilities() : *negotiated,
                pathLossDb, hasFreshPathLoss);
    }
};

} // namespace

Define_Module(HeHcf);

HeHcf::~HeHcf()
{
    cancelAndDelete(ulTriggerTimer);
    cancelAndDelete(triggeredUlResponseTimer);
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
        triggeredUlResponseTimer = new cMessage("heTriggeredUlResponseTimer");
        heTbResponseCommittedSignal = registerSignal("heTbResponseCommitted");
        linkPhyContext = std::make_unique<Ieee80211HeLinkPhyContext>(this, mac);
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

const IIeee80211HeLinkPhyContext& HeHcf::getLinkPhyContext() const
{
    if (linkPhyContext == nullptr)
        throw cRuntimeError("HE link/PHY context is not initialized");
    return *linkPhyContext;
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
        case IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER: return "NFRP_TRIGGER";
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
    if (msg == triggeredUlResponseTimer) {
        handleTriggeredUlResponseTimeout();
        return;
    }
    if (msg != ulTriggerTimer) {
        Hcf::handleMessage(msg);
        return;
    }
    scheduleAfter(par("ulTriggerCheckInterval"), ulTriggerTimer);
    // 26.5.2.2 permits only an HE AP to solicit UL MU HE TB PPDUs.  The AP
    // still has to obtain the medium through EDCA/HCF (10.23), so this timer
    // only requests channel access; the Trigger is transmitted after EDCAF wins.
    if (!mac->isApInAxMode() || !ulCoordinator->isEnabled() ||
            frameSequenceHandler->isSequenceRunning() || edca->getChannelOwner() != nullptr ||
            tx->isBusy() || ulTriggerAccessRequested)
        return;
    auto triggerType = par("enableNdpFeedbackReport").boolValue() ?
            IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER : ulCoordinator->selectTrigger(mac->getMib());
    if (triggerType == IIeee80211HeUlTriggerPolicy::NO_TRIGGER)
        return;
    EV_INFO << "Requesting channel access for HE UL "
             << (triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ? "BSRP" :
                     triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER ? "NFRP" : "Basic")
             << " Trigger\n";
    pendingUlTrigger = triggerType;
    ulTriggerAccessRequested = true;
    auto ac = triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER || triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER ?
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
    invalidatePeerDerivedState(staAddr);
    if (queueBankManager == nullptr) {
        EV_WARN << "Queue bank manager not initialized (not an 802.11ax AP?)\n";
        return nullptr;
    }
    return queueBankManager->createQueueBank(staAddr);
}

void HeHcf::destroyStationQueueBank(const MacAddress& staAddr)
{
    invalidatePeerDerivedState(staAddr);
    if (queueBankManager == nullptr) {
        EV_WARN << "Queue bank manager not initialized (not an 802.11ax AP?)\n";
        return;
    }
    queueBankManager->destroyQueueBank(staAddr);
}

int HeHcf::retireQueuedPacketsForPeer(const MacAddress& peer)
{
    std::vector<Packet *> packets;
    if (queueBankManager != nullptr) {
        auto bank = queueBankManager->getQueueBank(peer);
        if (bank != nullptr) {
            for (int ac = StationQueueBank::AC_BK; ac <= StationQueueBank::AC_VO; ++ac) {
                auto queue = bank->getQueue(static_cast<StationQueueBank::AccessCategory>(ac));
                for (int index = 0; index < queue->getNumPackets(); ++index)
                    packets.push_back(queue->getPacket(index));
            }
        }
    }
    if (edca != nullptr) {
        for (int ac = AC_BK; ac <= AC_VO; ++ac) {
            auto queue = edca->getEdcaf(static_cast<AccessCategory>(ac))->getPendingQueue();
            for (int index = 0; index < queue->getNumPackets(); ++index) {
                auto packet = queue->getPacket(index);
                auto header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(
                        packet->peekAtFront<Ieee80211MacHeader>());
                if (header != nullptr && header->getReceiverAddress() == peer)
                    packets.push_back(packet);
            }
        }
    }
    int retired = 0;
    for (auto packet : packets)
        if (retireQueuedPacket(packet, peer))
            retired++;
    return retired;
}

int HeHcf::retireInProgressPacketsForPeer(const MacAddress& peer)
{
    int retired = 0;
    for (int ac = AC_BK; ac <= AC_VO; ++ac)
        retired += edca->getEdcaf(static_cast<AccessCategory>(ac))->
                getInProgressFrames()->retireFramesForPeer(peer);
    return retired;
}

bool HeHcf::retireQueuedPacket(Packet *packet, const MacAddress& peer)
{
    if (queueBankManager != nullptr) {
        auto bank = queueBankManager->getQueueBank(peer);
        if (bank != nullptr) {
            for (int ac = StationQueueBank::AC_BK; ac <= StationQueueBank::AC_VO; ++ac) {
                auto queue = bank->getQueue(static_cast<StationQueueBank::AccessCategory>(ac));
                for (int index = 0; index < queue->getNumPackets(); ++index) {
                    if (queue->getPacket(index) == packet) {
                        if (edca != nullptr) {
                            auto header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(
                                    packet->peekAtFront<Ieee80211MacHeader>());
                            if (header != nullptr)
                                edca->getEdcaf(static_cast<AccessCategory>(ac))->
                                        getAckHandler()->retireFrame(header);
                        }
                        queue->removePacket(packet);
                        delete packet;
                        return true;
                    }
                }
            }
        }
    }
    if (edca != nullptr) {
        for (int ac = AC_BK; ac <= AC_VO; ++ac) {
            auto queue = edca->getEdcaf(static_cast<AccessCategory>(ac))->getPendingQueue();
            for (int index = 0; index < queue->getNumPackets(); ++index) {
                if (queue->getPacket(index) == packet) {
                    auto header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(
                            packet->peekAtFront<Ieee80211MacHeader>());
                    if (header != nullptr)
                        edca->getEdcaf(static_cast<AccessCategory>(ac))->
                                getAckHandler()->retireFrame(header);
                    queue->removePacket(packet);
                    delete packet;
                    return true;
                }
            }
        }
    }
    return false;
}

bool HeHcf::retireInProgressPacket(Packet *packet)
{
    if (edca == nullptr)
        return false;
    for (int ac = AC_BK; ac <= AC_VO; ++ac)
        if (edca->getEdcaf(static_cast<AccessCategory>(ac))->
                getInProgressFrames()->retireFrame(packet))
            return true;
    return false;
}

void HeHcf::retireDeferredPackets()
{
    for (const auto& entry : packetsPendingRetirement) {
        auto packet = entry.first;
        if (!retireQueuedPacket(packet, entry.second))
            retireInProgressPacket(packet);
    }
    packetsPendingRetirement.clear();
}

void HeHcf::frameSequenceFinished()
{
    retireDeferredPackets();
    Hcf::frameSequenceFinished();
}

StationQueueBank *HeHcf::getStationQueueBank(const MacAddress& staAddr) const
{
    return queueBankManager == nullptr ? nullptr : queueBankManager->getQueueBank(staAddr);
}

void HeHcf::invalidatePeerDerivedState(const MacAddress& peer)
{
    Hcf::invalidatePeerDerivedState(peer);
    retireQueuedPacketsForPeer(peer);
    if (frameSequenceHandler != nullptr && frameSequenceHandler->isSequenceRunning()) {
        for (int ac = AC_BK; ac <= AC_VO; ++ac) {
            auto inProgress = edca->getEdcaf(static_cast<AccessCategory>(ac))->getInProgressFrames();
            for (int index = 0; index < inProgress->getLength(); ++index) {
                auto packet = inProgress->getFrames(index);
                auto header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(
                        packet->peekAtFront<Ieee80211MacHeader>());
                if (header != nullptr && header->getReceiverAddress() == peer)
                    packetsPendingRetirement[packet] = peer;
            }
        }
    }
    else if (edca != nullptr)
        retireInProgressPacketsForPeer(peer);
    if (dlScheduler != nullptr)
        dlScheduler->invalidatePeer(peer);
    if (ulCoordinator != nullptr)
        ulCoordinator->invalidatePeer(peer);
    peerOperatingModes.erase(peer);
    csiManager.invalidatePeer(peer);
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
    const bool forceSingleUser = forceNextSingleUser[ac];
    if (forceSingleUser) {
        EV_INFO << "Start FS: forced single-user TXOP for AC " << ac << "\n";
        forceNextSingleUser[ac] = false;
    }

    ASSERT(modeSet != nullptr);
    bool isHeMode = strcmp(modeSet->getName(), "ax") == 0;
    if (!isHeMode)
        EV_INFO << "Non-HE mode, falling back to SU\n";
    HeTxopCoordinatorService::Actions actions;
    actions.tryStartUlMu = [this, ac] () { return tryStartUlMuFrameSequence(ac); };
    actions.tryStartDlMu = [this, ac] () { return tryStartDlMuFrameSequence(ac); };
    actions.releaseChannelIfNoSu = [this, ac] () { return releaseChannelIfNoFallbackFrame(ac); };
    actions.startSu = [this, ac] () { Hcf::startFrameSequence(ac); };
    txopCoordinator.start(isHeMode, forceSingleUser, actions);
}

void HeHcf::handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs)
{
    std::vector<Edcaf *> collidedEdcafsWithFrame;
    for (auto edcaf : internallyCollidedEdcafs) {
        // 10.23 internal collision handling is still performed by HCF, but HE
        // AP queues may be per-STA.  Stage the oldest per-STA fallback frame so
        // the base HCF collision logic sees the same pending-frame surface.
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

bool HeHcf::getPeerOperatingMode(const MacAddress& address, Ieee80211HeOperatingMode& mode) const
{
    auto it = peerOperatingModes.find(address);
    if (it != peerOperatingModes.end()) {
        mode = it->second;
        return true;
    }
    return false;
}

} // namespace ieee80211
} // namespace inet
