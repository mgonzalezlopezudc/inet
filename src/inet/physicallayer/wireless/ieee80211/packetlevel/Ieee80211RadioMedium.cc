//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211RadioMedium.h"

#include <algorithm>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarMediumAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/signal/Interference.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/Reception.h"
#include "inet/networklayer/common/NetworkInterface.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee80211RadioMedium);

void Ieee80211RadioMedium::addTransmission(const IRadio *transmitterRadio, const ITransmission *transmission)
{
    RadioMedium::addTransmission(transmitterRadio, transmission);
}

bool Ieee80211RadioMedium::findHeMuRuForReceiver(const IRadio *receiver, const ITransmission *transmission, Ieee80211HeRu& ru) const
{
    auto packet = transmission->getPacket();
    if (transmission->getPacketProtocol() != &Protocol::ieee80211HePhy || packet == nullptr || !packet->hasAtFront<Ieee80211HeMuPhyHeader>())
        return false;
    auto heMuPhyHeader = packet->peekAtFront<Ieee80211HeMuPhyHeader>();
    auto networkInterface = getContainingNicModule(check_and_cast<const cModule *>(receiver));
    auto receiverStaId = resolveHeMuStaId(networkInterface, networkInterface->getMacAddress());
    auto scalarTransmissionAnalogModel = dynamic_cast<const ScalarSignalAnalogModel *>(transmission->getAnalogModel());
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    if (scalarTransmissionAnalogModel == nullptr || ieee80211Transmission == nullptr)
        return false;
    if (heMuPhyHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK &&
            heMuPhyHeader->getUsersArraySize() == 1) {
        const auto& user = heMuPhyHeader->getUsers(0);
        constexpr double HE_TONE_SPACING = 78125;
        auto channelBandwidth = ieee80211Transmission->getMode()->getDataMode()->getBandwidth();
        int channelTones = getHeChannelToneCount(channelBandwidth);
        ru.index = user.ruIndex;
        ru.toneSize = user.ruToneSize;
        ru.toneOffset = user.ruToneOffset;
        ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
        ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
        ru.bandwidth = Hz(ru.toneSize * HE_TONE_SPACING);
        double centerTone = ru.toneOffset + ru.toneSize / 2.0 - channelTones / 2.0;
        auto fullChannelCenter = scalarTransmissionAnalogModel->getCenterFrequency() -
                Hz(centerTone * HE_TONE_SPACING);
        ru.centerFrequency = fullChannelCenter + Hz(centerTone * HE_TONE_SPACING);
        return true;
    }
    for (unsigned int i = 0; i < heMuPhyHeader->getUsersArraySize(); ++i) {
        const auto& user = heMuPhyHeader->getUsers(i);
        if (user.staId == receiverStaId) {
            constexpr double HE_TONE_SPACING = 78125;
            auto channelBandwidth = ieee80211Transmission->getMode()->getDataMode()->getBandwidth();
            if (user.ruToneSize == 0) {
                auto legacyRus = calculateHeRus(scalarTransmissionAnalogModel->getCenterFrequency(),
                        channelBandwidth, heMuPhyHeader->getUsersArraySize());
                if (user.ruIndex >= 0 && user.ruIndex < (int)legacyRus.size()) {
                    ru = legacyRus[user.ruIndex];
                    return true;
                }
                return false;
            }
            int channelTones = getHeChannelToneCount(channelBandwidth);
            ru.index = user.ruIndex;
            ru.toneSize = user.ruToneSize;
            ru.toneOffset = user.ruToneOffset;
            ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
            ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
            ru.bandwidth = Hz(ru.toneSize * HE_TONE_SPACING);
            double centerTone = ru.toneOffset + ru.toneSize / 2.0 - channelTones / 2.0;
            ru.centerFrequency = scalarTransmissionAnalogModel->getCenterFrequency() + Hz(centerTone * HE_TONE_SPACING);
            return true;
        }
    }
    return false;
}

const IReception *Ieee80211RadioMedium::computeHeMuRuReception(const IRadio *receiver, const ITransmission *transmission, const Ieee80211HeRu& ru) const
{
    auto scalarMediumAnalogModel = dynamic_cast<const ScalarMediumAnalogModel *>(analogModel);
    auto scalarTransmissionAnalogModel = dynamic_cast<const ScalarSignalAnalogModel *>(transmission->getAnalogModel());
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    if (scalarMediumAnalogModel == nullptr || scalarTransmissionAnalogModel == nullptr || ieee80211Transmission == nullptr)
        return nullptr;

    auto arrival = getArrival(receiver, transmission);
    auto totalBandwidth = ieee80211Transmission->getMode()->getDataMode()->getBandwidth();
    auto aggregatePower = scalarMediumAnalogModel->computeReceptionPower(receiver, transmission, arrival);
    auto packet = transmission->getPacket();
    auto heMuHeader = packet != nullptr && packet->hasAtFront<Ieee80211HeMuPhyHeader>() ?
            packet->peekAtFront<Ieee80211HeMuPhyHeader>() : nullptr;
    bool isTriggerBasedUplink = heMuHeader != nullptr &&
            heMuHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK;
    auto ruPower = isTriggerBasedUplink ? aggregatePower :
            aggregatePower * (ru.bandwidth.get() / totalBandwidth.get());
    auto ruAnalogModel = new ScalarReceptionAnalogModel(
            scalarTransmissionAnalogModel->getPreambleDuration(),
            scalarTransmissionAnalogModel->getHeaderDuration(),
            scalarTransmissionAnalogModel->getDataDuration(),
            ru.centerFrequency,
            ru.bandwidth,
            ruPower);
    return new Reception(receiver, transmission, arrival->getStartTime(), arrival->getEndTime(),
            arrival->getStartPosition(), arrival->getEndPosition(),
            arrival->getStartOrientation(), arrival->getEndOrientation(), ruAnalogModel);
}

const IReception *Ieee80211RadioMedium::computeReception(const IRadio *receiver, const ITransmission *transmission) const
{
    Ieee80211HeRu ru;
    if (findHeMuRuForReceiver(receiver, transmission, ru)) {
        if (auto reception = computeHeMuRuReception(receiver, transmission, ru))
            return reception;
    }
    return RadioMedium::computeReception(receiver, transmission);
}

const IInterference *Ieee80211RadioMedium::computeInterference(const IRadio *receiver,
        const IListening *listening, const ITransmission *transmission) const
{
    Ieee80211HeRu desiredRu;
    if (!findHeMuRuForReceiver(receiver, transmission, desiredRu))
        return RadioMedium::computeInterference(receiver, listening, transmission);

    interferenceComputationCount++;
    auto reception = getReception(receiver, transmission);
    auto allInterferingReceptions = computeInterferingReceptions(reception);
    auto overlappingReceptions = new std::vector<const IReception *>();
    double desiredMin = desiredRu.centerFrequency.get() - desiredRu.bandwidth.get() / 2;
    double desiredMax = desiredRu.centerFrequency.get() + desiredRu.bandwidth.get() / 2;
    for (auto interferingReception : *allInterferingReceptions) {
        auto analogModel = dynamic_cast<const ScalarReceptionAnalogModel *>(
                interferingReception->getAnalogModel());
        if (analogModel == nullptr) {
            overlappingReceptions->push_back(interferingReception);
            continue;
        }
        double interferingMin = analogModel->getCenterFrequency().get() -
                analogModel->getBandwidth().get() / 2;
        double interferingMax = analogModel->getCenterFrequency().get() +
                analogModel->getBandwidth().get() / 2;
        if (std::min(desiredMax, interferingMax) > std::max(desiredMin, interferingMin))
            overlappingReceptions->push_back(interferingReception);
    }
    delete allInterferingReceptions;
    auto noise = backgroundNoise ? backgroundNoise->computeNoise(listening) : nullptr;
    return new Interference(noise, overlappingReceptions);
}

} // namespace physicallayer
} // namespace inet
