//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"

#include <algorithm>
#include <exception>

#include "inet/physicallayer/wireless/ieee80211/contract/packetlevel/IIeee80211ModeSetListener.h"

#include "inet/common/packet/chunk/BitCountChunk.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/Simsignals.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DsssOfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ErpOfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211FhssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HrDsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211IrMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalMediumAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IMultibandReceiverAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IMultibandTransmitterAnalogModel.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/mobility/contract/IMobility.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/ListeningDecision.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211CcaListening.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"

#include <memory>

namespace inet {

namespace physicallayer {

Define_Module(Ieee80211Radio);

simsignal_t Ieee80211Radio::radioChannelChangedSignal = cComponent::registerSignal("radioChannelChanged");
simsignal_t IIeee80211CcaProvider::ccaStateChangedSignal = cComponent::registerSignal("ccaStateChanged");

namespace {

bool isCanonicalPpduBandwidth(Hz bandwidth)
{
    return bandwidth == MHz(20) || bandwidth == MHz(40) || bandwidth == MHz(80) || bandwidth == MHz(160);
}

const IIeee80211Mode *findFastestMode(const Ieee80211ModeSet *modeSet, Hz bandwidth)
{
    if (modeSet == nullptr)
        return nullptr;
    for (int index = modeSet->getNumModes() - 1; index >= 0; --index) {
        const auto *candidate = modeSet->getMode(index);
        if (candidate->getDataMode()->getBandwidth() == bandwidth)
            return candidate;
    }
    return nullptr;
}

const Ieee80211Channel *cloneChannelForBand(const Ieee80211Channel *channel, const IIeee80211Band *band)
{
    if (channel == nullptr)
        return nullptr;
    if (band == nullptr)
        throw cRuntimeError("Cannot retain an IEEE 802.11 channel without a band");
    if (channel->isExplicitGeometry())
        return new Ieee80211Channel(band, channel->getChannelNumber(), channel->getChannelWidth(),
                channel->getCenterFrequencyIndex0(), channel->getCenterFrequencyIndex1());
    return new Ieee80211Channel(band, channel->getChannelNumber(), channel->getSecondaryChannelOffset());
}

void validateModeForChannel(const Ieee80211Channel *channel, const IIeee80211Mode *mode)
{
    if (channel == nullptr || mode == nullptr)
        throw cRuntimeError("IEEE 802.11 target channel and mode must be defined");
    Hz bandwidth = mode->getDataMode()->getBandwidth();
    if (isCanonicalPpduBandwidth(bandwidth))
        channel->validatePpduWidth(bandwidth);
    else if (!(channel->getOperatingBandwidth() == MHz(20) && bandwidth <= MHz(22)))
        throw cRuntimeError("Unsupported IEEE 802.11 PPDU bandwidth %s for the target channel", bandwidth.str().c_str());
    if (bandwidth >= MHz(80) && dynamic_cast<const Ieee80211VhtMode *>(mode) == nullptr)
        throw cRuntimeError("IEEE 802.11 80/160 MHz PPDUs require a VHT mode");
}

bool isModeCompatibleWithChannel(const Ieee80211Channel *channel, const IIeee80211Mode *mode)
{
    if (channel == nullptr || mode == nullptr)
        return false;
    Hz bandwidth = mode->getDataMode()->getBandwidth();
    if (isCanonicalPpduBandwidth(bandwidth)) {
        if (bandwidth > channel->getOperatingBandwidth())
            return false;
    }
    else if (!(channel->getOperatingBandwidth() == MHz(20) && bandwidth <= MHz(22)))
        return false;
    return bandwidth < MHz(80) || dynamic_cast<const Ieee80211VhtMode *>(mode) != nullptr;
}

void validateModeSetAndOperationMode(const std::string& opMode, const Ieee80211ModeSet *modeSet)
{
    if (!opMode.empty() && modeSet != nullptr && opMode != modeSet->getName())
        throw cRuntimeError("IEEE 802.11 operation mode '%s' is inconsistent with mode set '%s'", opMode.c_str(), modeSet->getName());
}

bool isWideChannel(const Ieee80211Channel *channel)
{
    return channel != nullptr && (channel->getChannelWidth() == IEEE80211_CHANNEL_WIDTH_80MHZ ||
            channel->getChannelWidth() == IEEE80211_CHANNEL_WIDTH_160MHZ ||
            channel->getChannelWidth() == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ);
}

void validateWideConfiguration(const Ieee80211Channel *channel, const std::string& opMode, const Ieee80211ModeSet *modeSet)
{
    if (isWideChannel(channel)) {
        if (opMode != "ac" || modeSet == nullptr || strcmp(modeSet->getName(), "ac"))
            throw cRuntimeError("IEEE 802.11 VHT 80/160 MHz operation requires the ac operation mode and mode set");
        if (channel->getBand() == nullptr || !Ieee80211Channel::isVhtCapableBand(channel->getBand()))
            throw cRuntimeError("IEEE 802.11 VHT 80/160 MHz operation requires a 5 GHz band");
    }
}

void validate80Plus80Contracts(const Ieee80211Channel *channel, const Ieee80211Transmitter *transmitter,
        const Ieee80211Receiver *receiver, const IRadioMedium *medium)
{
    if (channel == nullptr || !channel->is80Plus80())
        return;
    if (transmitter == nullptr || dynamic_cast<const IMultibandTransmitterAnalogModel *>(transmitter->getAnalogModel()) == nullptr)
        throw cRuntimeError("IEEE 802.11 VHT 80+80 MHz requires a multiband transmitter analog model");
    if (receiver == nullptr || dynamic_cast<const IMultibandReceiverAnalogModel *>(receiver->getAnalogModel()) == nullptr)
        throw cRuntimeError("IEEE 802.11 VHT 80+80 MHz requires a multiband receiver analog model");
    if (medium == nullptr || dynamic_cast<const DimensionalMediumAnalogModel *>(medium->getAnalogModel()) == nullptr)
        throw cRuntimeError("IEEE 802.11 VHT 80+80 MHz requires a dimensional medium analog model");
}

}

Ieee80211Radio::Ieee80211Radio() :
    FlatRadioBase(),
    ccaSnapshot(std::make_unique<Ieee80211CcaSnapshot>())
{
}

bool Ieee80211Radio::computeIsBandBusy(const FrequencyBand& frequencyBand, Ieee80211CcaGroup group, bool legacyHt40) const
{
    const simtime_t now = simTime();
    const Coord& position = antenna->getMobility()->getCurrentPosition();
    Ieee80211CcaListening listening(this, now, now + SimTime::fromRaw(1), position, position,
            frequencyBand.centerFrequency, frequencyBand.bandwidth, group, legacyHt40);
    const IListeningDecision *decision = medium->listenOnMedium(this, &listening);
    // INET's ListeningDecision uses isListeningPossible() for the radio's
    // energy-present/busy state (Radio maps it to RECEPTION_STATE_BUSY).
    // Preserve that established contract while exposing a typed CCA marker.
    bool busy = decision->isListeningPossible();
    delete decision;
    return busy;
}

void Ieee80211Radio::updateCcaState()
{
    auto ieee80211Receiver = dynamic_cast<const Ieee80211Receiver *>(receiver);
    auto channel = ieee80211Receiver == nullptr ? nullptr : ieee80211Receiver->getChannel();
    bool ht40Configured = channel != nullptr &&
            channel->getSecondaryChannelOffset() != IEEE80211_SECONDARY_CHANNEL_NONE &&
            modeSet != nullptr && modeSet->isHtOperationSupported() &&
            ieee80211Receiver->getBandwidth() == MHz(40);
    bool groupedCcaConfigured = channel != nullptr && modeSet != nullptr && !strcmp(modeSet->getName(), "ac") &&
            (channel->getChannelWidth() == IEEE80211_CHANNEL_WIDTH_80MHZ ||
             channel->getChannelWidth() == IEEE80211_CHANNEL_WIDTH_160MHZ ||
             channel->getChannelWidth() == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ);
    bool ccaConfigured = ht40Configured || groupedCcaConfigured;
    bool ccaEnabled = ccaConfigured && isReceiverMode(radioMode);
    bool primary20Busy = false;
    bool secondary20Busy = false;
    bool secondary40Busy = false;
    bool secondary80Busy = false;
    bool radioRegistered = false;
    if (medium != nullptr && medium->getCommunicationCache() != nullptr) {
        medium->getCommunicationCache()->mapRadios([&] (const IRadio *radio) {
            radioRegistered = radioRegistered || radio == this;
        });
    }
    if (ccaEnabled && channel != nullptr && radioRegistered) {
        // IEEE Std 802.11-2024, 21.3.18.5.2/.3/.4: sample each configured
        // CCA group independently. The receiver owns threshold/preamble
        // classification; the radio only samples and publishes the result.
        if (ht40Configured) {
            primary20Busy = computeIsBandBusy(channel->getPrimary20Band(), IEEE80211_CCA_PRIMARY20, true);
            secondary20Busy = computeIsBandBusy(channel->getSecondary20Band(), IEEE80211_CCA_SECONDARY20, true);
        }
        else if (groupedCcaConfigured) {
            primary20Busy = computeIsBandBusy(channel->getPrimary20Band(), IEEE80211_CCA_PRIMARY20);
            if (channel->getChannelWidth() != IEEE80211_CHANNEL_WIDTH_20MHZ)
                secondary20Busy = computeIsBandBusy(channel->getSecondary20Band(), IEEE80211_CCA_SECONDARY20);
            if (channel->getChannelWidth() == IEEE80211_CHANNEL_WIDTH_80MHZ ||
                    channel->getChannelWidth() == IEEE80211_CHANNEL_WIDTH_160MHZ ||
                    channel->getChannelWidth() == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ)
                secondary40Busy = computeIsBandBusy(channel->getSecondary40Band(), IEEE80211_CCA_SECONDARY40);
            if (channel->getChannelWidth() == IEEE80211_CHANNEL_WIDTH_160MHZ ||
                    channel->getChannelWidth() == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ)
                secondary80Busy = computeIsBandBusy(channel->getSecondary80Band(), IEEE80211_CCA_SECONDARY80);
        }
    }
    auto snapshotWidth = channel == nullptr ? channelWidth : channel->getChannelWidth();
    if (ccaSnapshot->isEnabled() != ccaEnabled || ccaSnapshot->getChannelWidth() != snapshotWidth ||
            ccaSnapshot->getConfigurationRevision() != ccaConfigurationRevision ||
            ccaSnapshot->isPrimary20Busy() != primary20Busy || ccaSnapshot->isSecondary20Busy() != secondary20Busy ||
            ccaSnapshot->isSecondary40Busy() != secondary40Busy || ccaSnapshot->isSecondary80Busy() != secondary80Busy) {
        ccaSnapshot = std::make_unique<Ieee80211CcaSnapshot>(ccaEnabled, snapshotWidth, ccaConfigurationRevision,
                primary20Busy, secondary20Busy, secondary40Busy, secondary80Busy);
        emit(IIeee80211CcaProvider::ccaStateChangedSignal, ccaSnapshot.get());
    }
}

void Ieee80211Radio::updateTransceiverState()
{
    FlatRadioBase::updateTransceiverState();
    updateCcaState();
}

void Ieee80211Radio::initialize(int stage)
{
    FlatRadioBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        const char *fcsModeString = par("fcsMode");
        fcsMode = parseFcsMode(fcsModeString, true);
        opMode = par("opMode").stringValue();
    }
    if (stage == INITSTAGE_PHYSICAL_LAYER) {
        const char *bandName = par("bandName");
        const IIeee80211Band *targetBand = *bandName ? Ieee80211CompliantBands::getBand(bandName) : nullptr;
        const Ieee80211ModeSet *targetModeSet = !opMode.empty() ? Ieee80211ModeSet::getModeSet(opMode.c_str()) : nullptr;
        auto targetSecondaryChannelOffset = Ieee80211Channel::parseSecondaryChannelOffset(par("htSecondaryChannelOffset"));
        auto targetChannelWidth = Ieee80211Channel::parseChannelWidth(par("channelWidth"));
        int targetPrimaryChannelCenterFrequencyIndex = par("primaryChannelCenterFrequencyIndex");
        int targetChannelCenterFrequencyIndex0 = par("channelCenterFrequencyIndex0");
        int targetChannelCenterFrequencyIndex1 = par("channelCenterFrequencyIndex1");
        int channelNumber = par("channelNumber");
        Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
        Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
        validateModeSetAndOperationMode(opMode, targetModeSet);

        std::unique_ptr<Ieee80211Channel> targetChannel;
        if (channelNumber != -1) {
            if (targetBand == nullptr)
                throw cRuntimeError("An initial IEEE 802.11 channel requires a band");
            if ((targetChannelWidth == IEEE80211_CHANNEL_WIDTH_20MHZ || targetChannelWidth == IEEE80211_CHANNEL_WIDTH_40MHZ) &&
                    targetPrimaryChannelCenterFrequencyIndex == -1 && targetChannelCenterFrequencyIndex0 == -1)
                targetChannel = std::make_unique<Ieee80211Channel>(targetBand, channelNumber, targetSecondaryChannelOffset);
            else {
                // An explicit primary center-frequency index is the
                // authoritative primary-channel identity.  channelNumber is
                // only the legacy/default identity when that field is not
                // configured; using it unconditionally loses the requested
                // primary subchannel when the two parameters differ.
                int targetPrimaryChannelNumber = targetPrimaryChannelCenterFrequencyIndex != -1 ?
                        targetPrimaryChannelCenterFrequencyIndex : channelNumber;
                targetChannel = std::make_unique<Ieee80211Channel>(targetBand,
                        targetPrimaryChannelNumber,
                        targetChannelWidth, targetChannelCenterFrequencyIndex0, targetChannelCenterFrequencyIndex1);
            }
        }

        Hz configuredBandwidth = Hz(par("bandwidth").doubleValue());
        Hz targetBandwidth;
        if (!std::isnan(configuredBandwidth.get()))
            targetBandwidth = configuredBandwidth;
        else if (targetChannel != nullptr && (targetChannel->isExplicitGeometry() || targetChannel->getChannelWidth() != IEEE80211_CHANNEL_WIDTH_20MHZ))
            targetBandwidth = targetChannel->getOperatingBandwidth();
        else
            targetBandwidth = ieee80211Receiver->getBandwidth();
        if (targetChannel != nullptr && targetChannel->is80Plus80() &&
                !std::isnan(targetBandwidth.get()) && targetBandwidth != MHz(160))
            throw cRuntimeError("Configured VHT 80+80 MHz operation requires a 160 MHz default bandwidth");
        if (targetChannel != nullptr && isCanonicalPpduBandwidth(targetBandwidth))
            targetChannel->validatePpduWidth(targetBandwidth);
        validateWideConfiguration(targetChannel.get(), opMode, targetModeSet);
        validate80Plus80Contracts(targetChannel.get(), ieee80211Transmitter, ieee80211Receiver, medium.get());

        const IIeee80211Mode *targetMode = nullptr;
        if (targetModeSet != nullptr) {
            if (!std::isnan(ieee80211Transmitter->getBitrate().get()))
                targetMode = isCanonicalPpduBandwidth(targetBandwidth) ?
                        targetModeSet->findMode(ieee80211Transmitter->getBitrate(), targetBandwidth) :
                        targetModeSet->findMode(ieee80211Transmitter->getBitrate());
            if (targetMode == nullptr)
                targetMode = findFastestMode(targetModeSet, targetBandwidth);
            if (targetMode == nullptr)
                targetMode = targetModeSet->getFastestMode();
            if (targetChannel != nullptr)
                validateModeForChannel(targetChannel.get(), targetMode);
        }
        if (targetModeSet != nullptr && targetModeSet->isHtOperationSupported() &&
                targetBandwidth == MHz(40) && targetSecondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_NONE)
            throw cRuntimeError("HT 40 MHz operation requires a secondary channel offset of above or below");
        if (targetSecondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_NONE &&
                (targetModeSet == nullptr || !targetModeSet->isHtOperationSupported() || targetBandwidth != MHz(40)))
            throw cRuntimeError("htSecondaryChannelOffset above/below requires HT 40 MHz operation");

        // All validation above is complete before any authoritative radio,
        // child-channel, bandwidth, mode-set, or mode field is changed.
        this->band = targetBand;
        this->modeSet = targetModeSet;
        if (targetChannel != nullptr)
            setChannel(targetChannel.release());
        else {
            ieee80211Transmitter->setBand(targetBand);
            ieee80211Receiver->setBand(targetBand);
        }
        ieee80211Transmitter->setModeSet(targetModeSet);
        ieee80211Receiver->setModeSet(targetModeSet);
        ieee80211Receiver->setBandwidth(targetBandwidth);
        ieee80211Transmitter->setBandwidth(targetBandwidth);
        if (targetMode != nullptr)
            ieee80211Transmitter->setMode(targetMode);
    }
}

void Ieee80211Radio::handleUpperCommand(cMessage *message)
{
    if (changingModeSet)
        throw cRuntimeError("Reentrant radio configuration change");
    if (message->getKind() == RADIO_C_CONFIGURE) {
        ConfigureRadioCommand *configureCommand = dynamic_cast<ConfigureRadioCommand *>(message->getControlInfo());
        auto ieee80211Command = dynamic_cast<Ieee80211ConfigureRadioCommand *>(configureCommand);
        if (configureCommand != nullptr) {
            Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
            Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
            const Ieee80211Channel *currentChannel = ieee80211Receiver->getChannel();
            const char *requestedOpMode = ieee80211Command != nullptr ? ieee80211Command->getOpMode() : "";
            std::string targetOpMode = *requestedOpMode ? requestedOpMode : this->opMode;
            const Ieee80211Channel *channel = ieee80211Command != nullptr ? ieee80211Command->getChannel() : nullptr;
            const IIeee80211Band *bandParam = ieee80211Command != nullptr ? ieee80211Command->getBand() : nullptr;
            const IIeee80211Band *targetBand = bandParam != nullptr ? bandParam :
                    (channel != nullptr && channel->getBand() != nullptr) ? channel->getBand() : this->band;
            const Ieee80211ModeSet *modeSetParam = ieee80211Command != nullptr ? ieee80211Command->getModeSet() : nullptr;
            const Ieee80211ModeSet *targetModeSet = modeSetParam != nullptr ? modeSetParam :
                    *targetOpMode.c_str() ? Ieee80211ModeSet::getModeSet(targetOpMode.c_str()) : this->modeSet;
            if (targetOpMode.empty() && targetModeSet != nullptr)
                targetOpMode = targetModeSet->getName();
            validateModeSetAndOperationMode(targetOpMode, targetModeSet);
            if (channel != nullptr && bandParam != nullptr && channel->getBand() != nullptr && channel->getBand() != bandParam)
                throw cRuntimeError("IEEE 802.11 configure command band and channel refer to different bands");
            int newChannelNumber = ieee80211Command != nullptr ? ieee80211Command->getChannelNumber() : -1;
            int targetChannelNumber = channel != nullptr ? channel->getChannelNumber() :
                    newChannelNumber != -1 ? newChannelNumber :
                    currentChannel != nullptr ? currentChannel->getChannelNumber() : -1;
            auto targetSecondaryChannelOffset = channel != nullptr ? channel->getSecondaryChannelOffset() :
                    htSecondaryChannelOffset;
            auto targetChannelWidth = channel != nullptr ? channel->getChannelWidth() : this->channelWidth;
            int targetCenterFrequencyIndex0 = channel != nullptr ? channel->getCenterFrequencyIndex0() : channelCenterFrequencyIndex0;
            int targetCenterFrequencyIndex1 = channel != nullptr ? channel->getCenterFrequencyIndex1() : channelCenterFrequencyIndex1;
            if (targetChannelNumber != -1 && (targetBand == nullptr || targetChannelNumber < 0))
                throw cRuntimeError("Invalid target 802.11 channel number %d", targetChannelNumber);
            Hz newBandwidth = configureCommand->getBandwidth();
            Hz targetBandwidth = std::isnan(newBandwidth.get()) ? ieee80211Receiver->getBandwidth() : newBandwidth;
            if (targetBandwidth == MHz(20) && targetChannelWidth <= IEEE80211_CHANNEL_WIDTH_40MHZ)
                targetSecondaryChannelOffset = IEEE80211_SECONDARY_CHANNEL_NONE;
            std::unique_ptr<Ieee80211Channel> targetChannelObject;
            if (targetChannelNumber != -1) {
                if (channel != nullptr && !channel->isExplicitGeometry())
                    targetChannelObject = std::make_unique<Ieee80211Channel>(targetBand, targetChannelNumber, targetSecondaryChannelOffset);
                else if (channel == nullptr && currentChannel != nullptr && currentChannel->isExplicitGeometry())
                    targetChannelObject = std::make_unique<Ieee80211Channel>(targetBand, targetChannelNumber, targetChannelWidth,
                            targetCenterFrequencyIndex0, targetCenterFrequencyIndex1);
                else if (channel == nullptr && targetCenterFrequencyIndex0 == -1 && targetChannelWidth <= IEEE80211_CHANNEL_WIDTH_40MHZ)
                    targetChannelObject = std::make_unique<Ieee80211Channel>(targetBand, targetChannelNumber, targetSecondaryChannelOffset);
                else
                    targetChannelObject = std::make_unique<Ieee80211Channel>(targetBand, targetChannelNumber, targetChannelWidth,
                            targetCenterFrequencyIndex0, targetCenterFrequencyIndex1);
            }

            if (!std::isnan(newBandwidth.get()))
                targetBandwidth = newBandwidth;
            else if (targetChannelObject != nullptr && (targetChannelObject->isExplicitGeometry() || targetChannelObject->getChannelWidth() != IEEE80211_CHANNEL_WIDTH_20MHZ))
                targetBandwidth = targetChannelObject->getOperatingBandwidth();
            else
                targetBandwidth = ieee80211Receiver->getBandwidth();
            bps newBitrate = configureCommand->getBitrate();
            const IIeee80211Mode *requestedMode = ieee80211Command != nullptr ? ieee80211Command->getMode() : nullptr;
            if (targetChannelObject != nullptr && targetChannelObject->is80Plus80()) {
                if (!std::isnan(newBandwidth.get()) && newBandwidth != MHz(160))
                    throw cRuntimeError("Configured VHT 80+80 MHz operation requires a 160 MHz default bandwidth");
                if (requestedMode != nullptr && requestedMode->getDataMode()->getBandwidth() != MHz(160))
                    throw cRuntimeError("Configured VHT 80+80 MHz operation requires a 160 MHz default mode");
                targetBandwidth = MHz(160);
            }
            if (targetChannelObject != nullptr && isCanonicalPpduBandwidth(targetBandwidth))
                targetChannelObject->validatePpduWidth(targetBandwidth);
            validateWideConfiguration(targetChannelObject.get(), targetOpMode, targetModeSet);
            validate80Plus80Contracts(targetChannelObject.get(), ieee80211Transmitter, ieee80211Receiver, medium.get());

            const IIeee80211Mode *resolvedMode = requestedMode;
            const bool hasExplicitModeOrBitrate = requestedMode != nullptr || !std::isnan(newBitrate.get());
            if (resolvedMode != nullptr && (targetModeSet == nullptr || !targetModeSet->containsMode(resolvedMode)))
                throw cRuntimeError("Requested IEEE 802.11 mode is not in the target mode set");
            if (resolvedMode == nullptr && targetModeSet != nullptr && !std::isnan(newBitrate.get()))
                resolvedMode = isCanonicalPpduBandwidth(targetBandwidth) ? targetModeSet->getMode(newBitrate, targetBandwidth) : targetModeSet->getMode(newBitrate);
            if (resolvedMode == nullptr && targetModeSet != nullptr) {
                const auto *currentMode = ieee80211Transmitter->getMode();
                // A channel-only transition to 80+80 establishes the complete
                // operating state.  Do not silently retain a current VHT80
                // mode; narrower primary-hierarchy PPDUs remain available as
                // packet-level mode requests after configuration.
                if (targetChannelObject != nullptr && targetChannelObject->is80Plus80() &&
                        !hasExplicitModeOrBitrate && std::isnan(newBandwidth.get()))
                    resolvedMode = findFastestMode(targetModeSet, MHz(160));
                else if (currentMode != nullptr && targetModeSet->containsMode(currentMode))
                    resolvedMode = currentMode;
                if (resolvedMode != nullptr && targetChannelObject != nullptr && !isModeCompatibleWithChannel(targetChannelObject.get(), resolvedMode))
                    resolvedMode = nullptr;
                if (resolvedMode == nullptr)
                    resolvedMode = findFastestMode(targetModeSet, targetBandwidth);
                if (resolvedMode == nullptr)
                    resolvedMode = targetModeSet->getFastestMode();
            }
            if (resolvedMode != nullptr && targetChannelObject != nullptr)
                validateModeForChannel(targetChannelObject.get(), resolvedMode);
            if (targetModeSet != nullptr && targetModeSet->isHtOperationSupported() &&
                    ((targetBandwidth == MHz(40)) ||
                     (resolvedMode != nullptr && dynamic_cast<const Ieee80211HtMode *>(resolvedMode) != nullptr &&
                      resolvedMode->getDataMode()->getBandwidth() == MHz(40))) &&
                    targetSecondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_NONE)
                throw cRuntimeError("HT 40 MHz operation requires a secondary channel offset of above or below");

            // Validate inherited Radio/Narrowband/Flat fields before changing
            // any IEEE state.  The base classes otherwise apply these fields
            // after this method returns, which can overwrite a complete
            // channel geometry or throw after a partial commit.
            W newPower = configureCommand->getPower();
            bps inheritedBitrate = configureCommand->getBitrate();
            Hz inheritedCenterFrequency = configureCommand->getCenterFrequency();
            Hz inheritedBandwidth = configureCommand->getBandwidth();
            if (!std::isnan(newPower.get()) && (!std::isfinite(newPower.get()) || newPower <= W(0)))
                throw cRuntimeError("IEEE 802.11 configure command requires a positive finite power");
            if (!std::isnan(inheritedBitrate.get()) && (!std::isfinite(inheritedBitrate.get()) || inheritedBitrate <= bps(0)))
                throw cRuntimeError("IEEE 802.11 configure command requires a positive finite bitrate");
            if (!std::isnan(inheritedCenterFrequency.get()) && (!std::isfinite(inheritedCenterFrequency.get()) || inheritedCenterFrequency <= Hz(0)))
                throw cRuntimeError("IEEE 802.11 configure command requires a positive finite center frequency");
            if (!std::isnan(inheritedBandwidth.get()) && (!std::isfinite(inheritedBandwidth.get()) || inheritedBandwidth <= Hz(0)))
                throw cRuntimeError("IEEE 802.11 configure command requires a positive finite bandwidth");
            if (!std::isnan(inheritedCenterFrequency.get()) && targetChannelObject != nullptr &&
                    inheritedCenterFrequency != targetChannelObject->getOperatingCenterFrequency())
                throw cRuntimeError("A configured IEEE 802.11 channel owns the center frequency; the generic center override is inconsistent");
            if (!std::isnan(inheritedBandwidth.get()) && targetChannelObject != nullptr &&
                    (targetChannelObject->isExplicitGeometry() || targetChannelObject->getChannelWidth() != IEEE80211_CHANNEL_WIDTH_20MHZ) &&
                    inheritedBandwidth != targetBandwidth)
                throw cRuntimeError("A configured IEEE 802.11 channel owns the bandwidth; the generic bandwidth override is inconsistent");
            int requestedRadioMode = configureCommand->getRadioMode();
            if (requestedRadioMode != -1 && (requestedRadioMode < RADIO_MODE_OFF || requestedRadioMode > RADIO_MODE_SWITCHING ||
                    requestedRadioMode == RADIO_MODE_SWITCHING || getRadioMode() == RADIO_MODE_SWITCHING))
                throw cRuntimeError("Invalid or currently unavailable IEEE 802.11 radio mode request: %d", requestedRadioMode);

            // Keep this fact separate from the unique_ptr below: releasing a
            // target channel transfers ownership to the child transmitter,
            // but it must not make inherited-field validation look as if no
            // channel was configured.
            const bool targetChannelConfigured = targetChannelObject != nullptr;
            const bool targetChannelOwnsBandwidth = targetChannelConfigured &&
                    (targetChannelObject->isExplicitGeometry() || targetChannelObject->getChannelWidth() != IEEE80211_CHANNEL_WIDTH_20MHZ);

            bool channelChanged = currentChannel == nullptr || targetBand != this->band;
            if (!channelChanged && targetChannelObject != nullptr)
                channelChanged = targetChannelNumber != currentChannel->getChannelNumber() ||
                        targetChannelObject->getSecondaryChannelOffset() != currentChannel->getSecondaryChannelOffset() ||
                        targetChannelObject->getChannelWidth() != currentChannel->getChannelWidth() ||
                        targetChannelObject->isExplicitGeometry() != currentChannel->isExplicitGeometry() ||
                        targetChannelObject->getCenterFrequencyIndex0() != currentChannel->getCenterFrequencyIndex0() ||
                        targetChannelObject->getCenterFrequencyIndex1() != currentChannel->getCenterFrequencyIndex1();
            bool changeChannel = targetChannelObject != nullptr && (channelChanged || !std::isnan(newBandwidth.get()));
            auto applyConfiguration = [&]() {
                auto tx = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
                if (!std::isnan(targetBandwidth.get())) {
                    tx->setBandwidth(targetBandwidth);
                    ieee80211Receiver->setBandwidth(targetBandwidth);
                }
                if (changeChannel) {
                    tx->setChannel(new Ieee80211Channel(*targetChannelObject));
                    ieee80211Receiver->setChannel(new Ieee80211Channel(*targetChannelObject));
                    htSecondaryChannelOffset = targetChannelObject->getSecondaryChannelOffset();
                    channelWidth = targetChannelObject->getChannelWidth();
                    channelCenterFrequencyIndex0 = targetChannelObject->getCenterFrequencyIndex0();
                    channelCenterFrequencyIndex1 = targetChannelObject->getCenterFrequencyIndex1();
                }
                else if (targetBand != this->band) {
                    tx->setBand(targetBand);
                    ieee80211Receiver->setBand(targetBand);
                }
                this->band = targetBand;
                this->opMode = targetOpMode;
                if (!std::isnan(configureCommand->getCenterFrequency().get()))
                    setCenterFrequency(configureCommand->getCenterFrequency());
            };
            if (publishModeSet || resolvedMode != nullptr || changeChannel || !std::isnan(newBandwidth.get()) ||
                    !std::isnan(configureCommand->getCenterFrequency().get()))
                changeModeSet(targetModeSet, resolvedMode, resolvedMode != nullptr, applyConfiguration,
                        publishModeSet, changeChannel ? targetChannelNumber : -1);
            // Apply inherited fields only after the complete target tuple has
            // been validated and installed.  A channel-owned center/bandwidth
            // is already synchronized by setChannel and is intentionally not
            // passed through the generic base setters.
            if (configureCommand->getModulation() != nullptr)
                setModulation(configureCommand->getModulation());
            if (!std::isnan(newPower.get()))
                setPower(newPower);
            if (!std::isnan(inheritedBitrate.get()))
                setBitrate(inheritedBitrate);
            if (!targetChannelConfigured && !std::isnan(inheritedCenterFrequency.get()))
                setCenterFrequency(inheritedCenterFrequency);
            if (!targetChannelOwnsBandwidth && !std::isnan(inheritedBandwidth.get()))
                setBandwidth(inheritedBandwidth);
            if (requestedRadioMode != -1)
                setRadioMode((RadioMode)requestedRadioMode);
            delete message;
            return;
        }
    }
    FlatRadioBase::handleUpperCommand(message);
}

void Ieee80211Radio::setModeSet(const Ieee80211ModeSet *modeSet)
{
    Enter_Method("setModeSet");
    changeModeSet(modeSet, nullptr, false);
}

void Ieee80211Radio::setModeSetAndMode(const Ieee80211ModeSet *modeSet, const IIeee80211Mode *mode)
{
    Enter_Method("setModeSetAndMode");
    changeModeSet(modeSet, mode, true);
}

void Ieee80211Radio::changeModeSet(const Ieee80211ModeSet *modeSet, const IIeee80211Mode *mode, bool explicitMode,
        const std::function<void()>& applyConfiguration, bool publishModeSet, int channelNumber)
{
    if (changingModeSet)
        throw cRuntimeError("Reentrant radio mode-set change");
    if (modeSet != nullptr && mode != nullptr && !modeSet->containsMode(mode))
        throw cRuntimeError("Invalid mode");
    auto transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(this->transmitter));
    auto receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(this->receiver));
    const auto *oldTransmitterModeSet = transmitter->getModeSet();
    const auto *oldReceiverModeSet = receiver->getModeSet();
    const auto *oldMode = transmitter->getMode();
    auto oldReceptionTimer = receptionTimer;
    auto oldBand = band;
    auto oldOpMode = opMode;
    auto oldSecondaryChannelOffset = htSecondaryChannelOffset;
    auto restoreTransmitterChannel = applyConfiguration ? transmitter->saveChannelState() : std::function<void()>();
    auto restoreReceiverChannel = applyConfiguration ? receiver->saveChannelState() : std::function<void()>();

    // Discover the same subscribers that receive the hierarchical notification,
    // deduplicating participants subscribed at more than one level. Capture all
    // state first: a failing participant may have partially changed itself.
    std::vector<IIeee80211ModeSetListener *> participants;
    std::vector<std::function<void()>> restore;
    for (cComponent *component = publishModeSet ? this : nullptr; component != nullptr; component = component->getParentModule()) {
        for (auto listener : component->getLocalSignalListeners(modesetChangedSignal)) {
            auto participant = dynamic_cast<IIeee80211ModeSetListener *>(listener);
            if (participant != nullptr && std::find(participants.begin(), participants.end(), participant) == participants.end())
                participants.push_back(participant);
        }
    }
    if (modeSet == nullptr && !participants.empty())
        throw cRuntimeError("Cannot clear the radio mode set while MAC mode-set consumers are attached");
    for (auto participant : participants)
        restore.push_back(participant->saveModeSetState());

    changingModeSet = true;
    try {
        if (explicitMode)
            transmitter->setModeSetAndMode(modeSet, mode);
        else
            transmitter->setModeSet(modeSet);
        receiver->setModeSet(modeSet);
        if (applyConfiguration)
            applyConfiguration();
        for (auto participant : participants)
            participant->applyModeSet(modeSet);
    }
    catch (...) {
        transmitter->setModeSetAndMode(oldTransmitterModeSet, oldMode);
        receiver->setModeSet(oldReceiverModeSet);
        if (applyConfiguration) {
            restoreTransmitterChannel();
            restoreReceiverChannel();
            band = oldBand;
            opMode.swap(oldOpMode);
            htSecondaryChannelOffset = oldSecondaryChannelOffset;
        }
        for (auto it = restore.rbegin(); it != restore.rend(); ++it)
            (*it)();
        receptionTimer = oldReceptionTimer;
        changingModeSet = false;
        throw;
    }
    this->modeSet = modeSet;
    ++ccaConfigurationRevision;
    EV << "Changing radio mode set to " << modeSet << endl;
    receptionTimer = nullptr;
    // The transaction is committed. Observer failures must not undo a state
    // already published to earlier listeners. Keep the reentrancy guard during
    // publication so every listener observes the same committed mode set.
    std::exception_ptr observerFailure;
    if (getComponentType() != nullptr) {
        try {
            if (channelNumber != -1)
                emit(radioChannelChangedSignal, channelNumber);
        }
        catch (...) {
            observerFailure = std::current_exception();
        }
        try {
            if (publishModeSet && modeSet != nullptr)
                emit(modesetChangedSignal, const_cast<Ieee80211ModeSet *>(modeSet));
        }
        catch (...) {
            if (!observerFailure)
                observerFailure = std::current_exception();
        }
        // Listening changes are independent committed facts: the medium must get
        // its publication attempt even when a mode-set observer throws.
        try {
            emit(listeningChangedSignal, 0);
        }
        catch (...) {
            if (!observerFailure)
                observerFailure = std::current_exception();
        }
    }
    changingModeSet = false;
    if (observerFailure)
        std::rethrow_exception(observerFailure);
    EV << "Changing radio mode set to " << modeSet << " and mode to " << transmitter->getMode() << endl;
}

void Ieee80211Radio::setMode(const IIeee80211Mode *mode)
{
    if (changingModeSet)
        throw cRuntimeError("Reentrant radio configuration change");
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    ieee80211Transmitter->setMode(mode);
    ++ccaConfigurationRevision;
    EV << "Changing radio mode to " << mode << endl;
    receptionTimer = nullptr;
    if (getComponentType() != nullptr)
        emit(listeningChangedSignal, 0);
}

void Ieee80211Radio::setBand(const IIeee80211Band *band)
{
    if (changingModeSet)
        throw cRuntimeError("Reentrant radio configuration change");
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
    const Ieee80211Channel *currentChannel = ieee80211Receiver->getChannel();
    if (currentChannel != nullptr) {
        auto replacement = std::unique_ptr<const Ieee80211Channel>(cloneChannelForBand(currentChannel, band));
        setChannel(replacement.release());
    }
    else {
        ieee80211Transmitter->setBand(band);
        ieee80211Receiver->setBand(band);
        this->band = band;
    }
    ++ccaConfigurationRevision;
    EV << "Changing radio band to " << band << endl;
    receptionTimer = nullptr;
    const auto *channel = ieee80211Transmitter->getChannel();
    if (channel != nullptr)
        emit(radioChannelChangedSignal, channel->getChannelNumber());
    emit(listeningChangedSignal, 0);
}

void Ieee80211Radio::setChannel(const Ieee80211Channel *channel)
{
    if (changingModeSet)
        throw cRuntimeError("Reentrant radio configuration change");
    if (channel == nullptr)
        throw cRuntimeError("IEEE 802.11 radio channel cannot be null");
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
    std::unique_ptr<Ieee80211Channel> receiverChannel = std::make_unique<Ieee80211Channel>(*channel);
    ieee80211Transmitter->setChannel(channel);
    ieee80211Receiver->setChannel(receiverChannel.release());
    band = channel->getBand();
    htSecondaryChannelOffset = channel->getSecondaryChannelOffset();
    channelWidth = channel->getChannelWidth();
    ++ccaConfigurationRevision;
    if (channel->isExplicitGeometry()) {
        primaryChannelCenterFrequencyIndex = channel->getChannelNumber();
        channelCenterFrequencyIndex0 = channel->getCenterFrequencyIndex0();
        channelCenterFrequencyIndex1 = channel->getCenterFrequencyIndex1();
    }
    else {
        primaryChannelCenterFrequencyIndex = -1;
        channelCenterFrequencyIndex0 = -1;
        channelCenterFrequencyIndex1 = 0;
    }
    if (channel->isExplicitGeometry() || channel->getChannelWidth() != IEEE80211_CHANNEL_WIDTH_20MHZ) {
        Hz operatingBandwidth = channel->getOperatingBandwidth();
        ieee80211Receiver->setBandwidth(operatingBandwidth);
        ieee80211Transmitter->setBandwidth(operatingBandwidth);
    }
    EV << "Changing radio channel to " << channel->getChannelNumber() << endl;
    receptionTimer = nullptr;
    emit(radioChannelChangedSignal, channel->getChannelNumber());
    emit(listeningChangedSignal, 0);
}

void Ieee80211Radio::setChannelNumber(int newChannelNumber)
{
    auto channel = createConfiguredChannel(newChannelNumber);
    setChannel(channel);
}

const Ieee80211Channel *Ieee80211Radio::createConfiguredChannel(int channelNumber) const
{
    if ((channelWidth == IEEE80211_CHANNEL_WIDTH_20MHZ || channelWidth == IEEE80211_CHANNEL_WIDTH_40MHZ) &&
            primaryChannelCenterFrequencyIndex == -1 && channelCenterFrequencyIndex0 == -1)
        return new Ieee80211Channel(band, channelNumber, htSecondaryChannelOffset);
    return new Ieee80211Channel(band,
            channelNumber,
            channelWidth,
            channelCenterFrequencyIndex0 == -1 ? channelNumber : channelCenterFrequencyIndex0,
            channelCenterFrequencyIndex1);
}

void Ieee80211Radio::insertFcs(const Ptr<Ieee80211PhyHeader>& phyHeader) const
{
    if (auto header = dynamic_cast<Ieee80211FhssPhyHeader *>(phyHeader.get())) {
        header->setFcsMode(fcsMode);
        switch (fcsMode) {
            case FCS_COMPUTED:
                header->setFcs(0); // TODO calculate FCS
                break;
            case FCS_DECLARED_CORRECT:
                header->setFcs(0xC00D);
                break;
            case FCS_DECLARED_INCORRECT:
                header->setFcs(0xBAAD);
                break;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else if (auto header = dynamic_cast<Ieee80211IrPhyHeader *>(phyHeader.get())) {
        header->setFcsMode(fcsMode);
        switch (fcsMode) {
            case FCS_COMPUTED:
                header->setFcs(0); // TODO calculate FCS
                break;
            case FCS_DECLARED_CORRECT:
                header->setFcs(0xC00D);
                break;
            case FCS_DECLARED_INCORRECT:
                header->setFcs(0xBAAD);
                break;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else if (auto header = dynamic_cast<Ieee80211DsssPhyHeader *>(phyHeader.get())) {
        header->setFcsMode(fcsMode);
        switch (fcsMode) {
            case FCS_COMPUTED:
                header->setFcs(0); // TODO calculate FCS
                break;
            case FCS_DECLARED_CORRECT:
                header->setFcs(0xC00D);
                break;
            case FCS_DECLARED_INCORRECT:
                header->setFcs(0xBAAD);
                break;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
}

bool Ieee80211Radio::verifyFcs(const Ptr<const Ieee80211PhyHeader>& phyHeader) const
{
    if (auto header = dynamicPtrCast<const Ieee80211FhssPhyHeader>(phyHeader)) {
        switch (header->getFcsMode()) {
            case FCS_COMPUTED:
                return true; // TODO calculate and check FCS
            case FCS_DECLARED_CORRECT:
                return true;
            case FCS_DECLARED_INCORRECT:
                return false;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else if (auto header = dynamicPtrCast<const Ieee80211IrPhyHeader>(phyHeader)) {
        switch (header->getFcsMode()) {
            case FCS_COMPUTED:
                return true; // TODO calculate and check FCS
            case FCS_DECLARED_CORRECT:
                return true;
            case FCS_DECLARED_INCORRECT:
                return false;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else if (auto header = dynamicPtrCast<const Ieee80211DsssPhyHeader>(phyHeader)) {
        switch (header->getFcsMode()) {
            case FCS_COMPUTED:
                return true; // TODO calculate and check FCS
            case FCS_DECLARED_CORRECT:
                return true;
            case FCS_DECLARED_INCORRECT:
                return false;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else
        return true;
}

void Ieee80211Radio::encapsulate(Packet *packet) const
{
    auto ieee80211Transmitter = check_and_cast<const Ieee80211Transmitter *>(transmitter);
    auto mode = ieee80211Transmitter->computeTransmissionMode(packet);
    auto phyHeader = mode->getHeaderMode()->createHeader();
    phyHeader->setChunkLength(b(mode->getHeaderMode()->getLength()));
    phyHeader->setLengthField(B(packet->getDataLength()));
    insertFcs(phyHeader);
    packet->insertAtFront(phyHeader);

    auto tailLength = dynamic_cast<const Ieee80211OfdmMode *>(mode) ? b(6) : b(0);
    auto paddingLength = mode->getDataMode()->getPaddingLength(B(phyHeader->getLengthField()));
    if (tailLength + paddingLength != b(0)) {
        const auto& phyTrailer = makeShared<BitCountChunk>(tailLength + paddingLength);
        packet->insertAtBack(phyTrailer);
    }
    const Protocol *protocol = nullptr;
    if (dynamic_cast<Ieee80211FhssPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211FhssPhy;
    else if (dynamic_cast<Ieee80211IrPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211IrPhy;
    else if (dynamic_cast<Ieee80211DsssPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211DsssPhy;
    else if (dynamic_cast<Ieee80211HrDsssPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211HrDsssPhy;
    else if (dynamic_cast<Ieee80211OfdmPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211OfdmPhy;
    else if (dynamic_cast<Ieee80211ErpOfdmPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211ErpOfdmPhy;
    else if (dynamic_cast<Ieee80211HtPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211HtPhy;
    else if (dynamic_cast<Ieee80211VhtPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211VhtPhy;
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY header type.");
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(protocol);
}

void Ieee80211Radio::decapsulate(Packet *packet) const
{
    auto mode = packet->getTag<Ieee80211ModeInd>()->getMode();
    const auto& phyHeader = popIeee80211PhyHeaderAtFront(packet, b(-1), Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
    if (phyHeader->isIncorrect() || phyHeader->isIncomplete() || phyHeader->isImproperlyRepresented() || !verifyFcs(phyHeader))
        packet->setBitError(true);
    auto tailLength = dynamic_cast<const Ieee80211OfdmMode *>(mode) ? b(6) : b(0);
    auto paddingLength = mode->getDataMode()->getPaddingLength(B(phyHeader->getLengthField()));
    if (tailLength + paddingLength != b(0))
        packet->popAtBack(tailLength + paddingLength, Chunk::PF_ALLOW_INCORRECT);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211Mac);
}

const Ptr<const Ieee80211PhyHeader> Ieee80211Radio::popIeee80211PhyHeaderAtFront(Packet *packet, b length, int flags)
{
    int id = packet->getTag<PacketProtocolTag>()->getProtocol()->getId();
    if (id == Protocol::ieee80211FhssPhy.getId())
        return packet->popAtFront<Ieee80211FhssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211IrPhy.getId())
        return packet->popAtFront<Ieee80211IrPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211DsssPhy.getId())
        return packet->popAtFront<Ieee80211DsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HrDsssPhy.getId())
        return packet->popAtFront<Ieee80211HrDsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211OfdmPhy.getId())
        return packet->popAtFront<Ieee80211OfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211ErpOfdmPhy.getId())
        return packet->popAtFront<Ieee80211ErpOfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HtPhy.getId())
        return packet->popAtFront<Ieee80211HtPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211VhtPhy.getId())
        return packet->popAtFront<Ieee80211VhtPhyHeader>(length, flags);
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY protocol.");
}

const Ptr<const Ieee80211PhyHeader> Ieee80211Radio::peekIeee80211PhyHeaderAtFront(const Packet *packet, b length, int flags)
{
    int id = packet->getTag<PacketProtocolTag>()->getProtocol()->getId();
    if (id == Protocol::ieee80211FhssPhy.getId())
        return packet->peekAtFront<Ieee80211FhssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211IrPhy.getId())
        return packet->peekAtFront<Ieee80211IrPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211DsssPhy.getId())
        return packet->peekAtFront<Ieee80211DsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HrDsssPhy.getId())
        return packet->peekAtFront<Ieee80211HrDsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211OfdmPhy.getId())
        return packet->peekAtFront<Ieee80211OfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211ErpOfdmPhy.getId())
        return packet->peekAtFront<Ieee80211ErpOfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HtPhy.getId())
        return packet->peekAtFront<Ieee80211HtPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211VhtPhy.getId())
        return packet->peekAtFront<Ieee80211VhtPhyHeader>(length, flags);
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY protocol.");
}

} // namespace physicallayer

} // namespace inet
