//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211RadioMedium.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
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

bool Ieee80211RadioMedium::findHeMuRuForReceiver(const IRadio *receiver, const ITransmission *transmission, Ieee80211HeRu& ru) const
{
    auto packet = transmission->getPacket();
    if (transmission->getPacketProtocol() != &Protocol::ieee80211HePhy || packet == nullptr || !packet->hasAtFront<Ieee80211HeMuPhyHeader>())
        return false;
    auto heMuPhyHeader = packet->peekAtFront<Ieee80211HeMuPhyHeader>();
    auto networkInterface = getContainingNicModule(check_and_cast<const cModule *>(receiver));
    auto receiverStaId = computeHeMuStaId(networkInterface->getMacAddress());
    auto scalarTransmissionAnalogModel = dynamic_cast<const ScalarSignalAnalogModel *>(transmission->getAnalogModel());
    auto ieee80211Transmission = dynamic_cast<const Ieee80211Transmission *>(transmission);
    if (scalarTransmissionAnalogModel == nullptr || ieee80211Transmission == nullptr)
        return false;
    auto rus = calculateHeRus(scalarTransmissionAnalogModel->getCenterFrequency(), ieee80211Transmission->getMode()->getDataMode()->getBandwidth(), heMuPhyHeader->getUsersArraySize());
    for (unsigned int i = 0; i < heMuPhyHeader->getUsersArraySize(); ++i) {
        const auto& user = heMuPhyHeader->getUsers(i);
        if (user.staId == receiverStaId) {
            for (const auto& candidateRu : rus) {
                if (candidateRu.index == user.ruIndex) {
                    ru = candidateRu;
                    return true;
                }
            }
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
    auto ruPower = aggregatePower * (ru.bandwidth.get() / totalBandwidth.get());
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

} // namespace physicallayer
} // namespace inet
