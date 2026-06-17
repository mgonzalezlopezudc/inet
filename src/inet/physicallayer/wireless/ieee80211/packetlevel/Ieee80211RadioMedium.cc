//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211RadioMedium.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarMediumAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarSignalAnalogModel.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/Reception.h"
#include "inet/networklayer/common/NetworkInterface.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee80211RadioMedium);

void Ieee80211RadioMedium::addTransmission(const IRadio *transmitterRadio, const ITransmission *transmission)
{
    RadioMedium::addTransmission(transmitterRadio, transmission);
}

const Ieee80211HeMuRuAllocation *Ieee80211RadioMedium::findHeMuAllocationForReceiver(const IRadio *receiver, const ITransmission *transmission) const
{
    auto packet = transmission->getPacket();
    auto heMuTag = packet != nullptr ? packet->findTag<Ieee80211HeMuTag>() : nullptr;
    if (heMuTag == nullptr)
        return nullptr;
    auto networkInterface = getContainingNicModule(check_and_cast<const cModule *>(receiver));
    auto receiverAddress = networkInterface->getMacAddress();
    for (const auto& allocation : heMuTag->getAllocations())
        if (allocation.staAddress == receiverAddress)
            return &allocation;
    return nullptr;
}

const IReception *Ieee80211RadioMedium::computeHeMuRuReception(const IRadio *receiver, const ITransmission *transmission, const Ieee80211HeMuRuAllocation& allocation) const
{
    auto scalarMediumAnalogModel = dynamic_cast<const ScalarMediumAnalogModel *>(analogModel);
    auto scalarTransmissionAnalogModel = dynamic_cast<const ScalarSignalAnalogModel *>(transmission->getAnalogModel());
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    if (scalarMediumAnalogModel == nullptr || scalarTransmissionAnalogModel == nullptr || ieee80211Transmission == nullptr)
        return nullptr;

    auto arrival = getArrival(receiver, transmission);
    auto totalBandwidth = ieee80211Transmission->getMode()->getDataMode()->getBandwidth();
    auto aggregatePower = scalarMediumAnalogModel->computeReceptionPower(receiver, transmission, arrival);
    auto ruPower = aggregatePower * (allocation.ru.bandwidth.get() / totalBandwidth.get());
    auto ruAnalogModel = new ScalarReceptionAnalogModel(
            scalarTransmissionAnalogModel->getPreambleDuration(),
            scalarTransmissionAnalogModel->getHeaderDuration(),
            scalarTransmissionAnalogModel->getDataDuration(),
            allocation.ru.centerFrequency,
            allocation.ru.bandwidth,
            ruPower);
    return new Reception(receiver, transmission, arrival->getStartTime(), arrival->getEndTime(),
            arrival->getStartPosition(), arrival->getEndPosition(),
            arrival->getStartOrientation(), arrival->getEndOrientation(), ruAnalogModel);
}

const IReception *Ieee80211RadioMedium::computeReception(const IRadio *receiver, const ITransmission *transmission) const
{
    if (auto allocation = findHeMuAllocationForReceiver(receiver, transmission)) {
        if (auto reception = computeHeMuRuReception(receiver, transmission, *allocation))
            return reception;
    }
    return RadioMedium::computeReception(receiver, transmission);
}

} // namespace physicallayer
} // namespace inet
