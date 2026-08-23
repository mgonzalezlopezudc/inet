//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/rateselection/Ieee80211LdpcSelection.h"

#include <algorithm>

namespace inet {
namespace ieee80211 {

using namespace physicallayer;

namespace {

bool isControlContextAllowed(Ieee80211LdpcFrameContext frameContext, const IIeee80211Mode *solicitingMode)
{
    // IEEE Std 802.11-2024 10.6.6 and 10.15 restrict LDPC use for control
    // responses and control frames that initiate a TXOP.
    switch (frameContext) {
        case Ieee80211LdpcFrameContext::DATA_OR_MANAGEMENT:
            return true;
        case Ieee80211LdpcFrameContext::CONTROL_RESPONSE:
            return solicitingMode != nullptr && solicitingMode->getDataMode()->getFecType() == Ieee80211FecType::LDPC;
        case Ieee80211LdpcFrameContext::TXOP_INITIATING_CONTROL:
        case Ieee80211LdpcFrameContext::OTHER_CONTROL:
            return false;
        default:
            throw cRuntimeError("Unknown IEEE 802.11 LDPC frame context");
    }
}

} // namespace

const IIeee80211Mode *constrainIeee80211ModeByPeerOperatingMode(
        const Ieee80211ModeSet *modeSet, const IIeee80211Mode *candidate,
        const IIeee80211PeerCapabilities *peerCapabilities,
        const MacAddress& receiverAddress, bool forcedMode)
{
    if (modeSet == nullptr || candidate == nullptr)
        throw cRuntimeError("Cannot constrain an IEEE 802.11 mode without a mode set and candidate mode");
    auto format = candidate->getDataMode()->getPhyFormat();
    if ((format != Ieee80211PhyFormat::HT && format != Ieee80211PhyFormat::VHT_SU) || peerCapabilities == nullptr)
        return candidate;
    auto receiverSet = peerCapabilities->resolveIntendedReceivers(receiverAddress);
    if (!receiverSet.complete || receiverSet.receivers.empty())
        return candidate;

    int maximumBandwidthMhz = candidate->getDataMode()->getBandwidth().get<MHz>();
    int maximumSpatialStreams = candidate->getDataMode()->getNumberOfSpatialStreams();
    auto isSupportedByAllReceivers = [&](const IIeee80211Mode *mode) {
        auto dataMode = mode->getDataMode();
        for (const auto& receiver : receiverSet.receivers) {
            auto status = peerCapabilities->getPeerLdpcStatus(receiver);
            int staticMaximumBandwidthMhz = format == Ieee80211PhyFormat::HT ?
                    status.maximumHtRxBandwidthMhz : status.maximumVhtRxBandwidthMhz;
            if (staticMaximumBandwidthMhz != -1 &&
                dataMode->getBandwidth() > MHz(staticMaximumBandwidthMhz))
                return false;
            if (status.operatingModeType0Valid &&
                (dataMode->getBandwidth() > MHz(status.operatingModeMaximumBandwidthMhz) ||
                 dataMode->getNumberOfSpatialStreams() > status.operatingModeMaximumSpatialStreams))
                return false;
            if (format == Ieee80211PhyFormat::VHT_SU && status.vhtRxMcsMapKnown) {
                int nss = dataMode->getNumberOfSpatialStreams();
                int mcs = static_cast<int>(dataMode->getMcsIndex());
                int mapValue = nss >= 1 && nss <= 8 ?
                        (status.vhtRxMcsMap >> (2 * (nss - 1))) & 0x03 : 0x03;
                int maximumMcs = mapValue == 0 ? 7 : mapValue == 1 ? 8 : mapValue == 2 ? 9 : -1;
                if (mcs > maximumMcs)
                    return false;
            }
            if (format == Ieee80211PhyFormat::HT && status.htRxMcsSetKnown) {
                int mcs = static_cast<int>(dataMode->getMcsIndex());
                if (mcs < 0 || mcs >= static_cast<int>(8 * status.htRxMcsSet.size()) ||
                    (status.htRxMcsSet[mcs / 8] & (1 << (mcs % 8))) == 0)
                    return false;
            }
        }
        return true;
    };

    for (const auto& receiver : receiverSet.receivers) {
        auto status = peerCapabilities->getPeerLdpcStatus(receiver);
        int staticMaximumBandwidthMhz = format == Ieee80211PhyFormat::HT ?
                status.maximumHtRxBandwidthMhz : status.maximumVhtRxBandwidthMhz;
        if (staticMaximumBandwidthMhz != -1)
            maximumBandwidthMhz = std::min(maximumBandwidthMhz, staticMaximumBandwidthMhz);
        if (status.operatingModeType0Valid) {
            maximumBandwidthMhz = std::min(maximumBandwidthMhz, status.operatingModeMaximumBandwidthMhz);
            maximumSpatialStreams = std::min(maximumSpatialStreams, status.operatingModeMaximumSpatialStreams);
        }
    }
    if (isSupportedByAllReceivers(candidate))
        return candidate;
    if (forcedMode)
        throw cRuntimeError("Explicit IEEE 802.11 mode exceeds the receiver's latest Operating Mode or advertised VHT-MCS/NSS set");

    const IIeee80211Mode *best = nullptr;
    auto candidateBitrate = candidate->getDataMode()->getNetBitrate();
    for (int i = 0; i < modeSet->getNumModes(); i++) {
        const auto *entry = modeSet->getMode(i);
        const auto *dataMode = entry->getDataMode();
        if (dataMode->getPhyFormat() != format ||
            dataMode->getBandwidth() > MHz(maximumBandwidthMhz) ||
            dataMode->getNumberOfSpatialStreams() > maximumSpatialStreams ||
            dataMode->getNetBitrate() > candidateBitrate)
            continue;
        auto mode = modeSet->getFecVariant(entry, candidate->getDataMode()->getFecType());
        if (mode != nullptr && isSupportedByAllReceivers(mode) &&
            (best == nullptr || mode->getDataMode()->getNetBitrate() > best->getDataMode()->getNetBitrate()))
            best = mode;
    }
    if (best == nullptr)
        throw cRuntimeError("No legal IEEE 802.11 mode satisfies the receiver's latest Operating Mode and advertised VHT-MCS/NSS set");
    return best;
}

Ieee80211LdpcSelectionResult selectIeee80211FecMode(const Ieee80211ModeSet *modeSet,
        const IIeee80211Mode *candidate, bool forcedLdpc,
        const Ieee80211LdpcSelectionConfig& config,
        const IIeee80211PeerCapabilities *peerCapabilities,
        const MacAddress& receiverAddress,
        Ieee80211LdpcFrameContext frameContext,
        const IIeee80211Mode *solicitingMode)
{
    if (modeSet == nullptr || candidate == nullptr)
        throw cRuntimeError("Cannot select IEEE 802.11 FEC without a mode set and candidate mode");

    candidate = constrainIeee80211ModeByPeerOperatingMode(modeSet, candidate,
            peerCapabilities, receiverAddress, forcedLdpc);
    auto format = candidate->getDataMode()->getPhyFormat();
    if (format != Ieee80211PhyFormat::HT && format != Ieee80211PhyFormat::VHT_SU) {
        if (forcedLdpc)
            throw cRuntimeError("LDPC can only be selected for HT or VHT data fields");
        return {candidate, false};
    }

    bool localReady = format == Ieee80211PhyFormat::HT ? config.htTxActivated : config.vhtTxActivated;
    bool allReceiversReady = false;
    bool noLdpcPreferred = false;
    if (peerCapabilities != nullptr) {
        auto receiverSet = peerCapabilities->resolveIntendedReceivers(receiverAddress);
        allReceiversReady = receiverSet.complete && !receiverSet.receivers.empty();
        for (const auto& receiver : receiverSet.receivers) {
            auto status = peerCapabilities->getPeerLdpcStatus(receiver);
            auto capability = format == Ieee80211PhyFormat::HT ? status.htRxLdpc : status.vhtRxLdpc;
            allReceiversReady &= capability == Ieee80211CapabilityStatus::SUPPORTED;
            noLdpcPreferred |= status.hasOperatingMode && status.noLdpcPreferred;
        }
    }
    bool controlAllowed = isControlContextAllowed(frameContext, solicitingMode);
    bool mandatoryGatesPass = localReady && allReceiversReady && controlAllowed;

    if (forcedLdpc && !mandatoryGatesPass)
        throw cRuntimeError("Illegal forced LDPC mode: local activation, peer receive capability, receiver set, or control-frame rule is not satisfied");

    // The Operating Mode Notification No-LDPC bit is the 10.15 SHOULD NOT
    // preference; an explicit legal request may override that preference.
    auto fecType = mandatoryGatesPass && (!noLdpcPreferred || forcedLdpc) ? Ieee80211FecType::LDPC : Ieee80211FecType::BCC;
    auto selectedMode = modeSet->getFecVariant(candidate, fecType);
    return {selectedMode, forcedLdpc && noLdpcPreferred};
}

} // namespace ieee80211
} // namespace inet
