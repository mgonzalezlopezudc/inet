//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211RadioMedium.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuTag.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatTransmitterBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmitterAnalogModel.h"
#include "inet/physicallayer/wireless/common/signal/WirelessSignal.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee80211RadioMedium);

void Ieee80211RadioMedium::addTransmission(const IRadio *transmitterRadio, const ITransmission *transmission)
{
    auto packet = transmission->getPacket();
    auto heMuTag = packet ? packet->findTag<Ieee80211HeMuTag>() : nullptr;
    auto ieee80211Trans = dynamic_cast<const Ieee80211Transmission *>(transmission);

    if (heMuTag != nullptr && !heMuTag->getAllocations().empty() && ieee80211Trans != nullptr) {
        auto flatTransmitter = check_and_cast<const FlatTransmitterBase *>(transmitterRadio->getTransmitter());
        W totalPower = flatTransmitter->getPower();
        Hz totalBandwidth = ieee80211Trans->getMode()->getDataMode()->getBandwidth();
        Hz centerFreq = ieee80211Trans->getChannel()->getCenterFrequency();

        const auto& allocations = heMuTag->getAllocations();
        int numRUs = allocations.size();
        auto rus = calculateHeRus(centerFreq, totalBandwidth, numRUs);

        std::vector<const ITransmission *> subTransmissions;

        for (const auto& alloc : allocations) {
            int ruIndex = alloc.ruIndex;
            if (ruIndex < 0 || ruIndex >= (int)rus.size())
                throw cRuntimeError("Invalid RU index %d", ruIndex);
            const auto& ru = rus[ruIndex];

            W ruPower = totalPower * (ru.bandwidth.get() / totalBandwidth.get());

            auto ruAnalogModel = flatTransmitter->getAnalogModel()->createAnalogModel(
                transmission->getPreambleDuration(),
                transmission->getHeaderDuration(),
                transmission->getDataDuration(),
                ru.centerFrequency,
                ru.bandwidth,
                ruPower
            );

            auto ruTransmission = new Ieee80211Transmission(
                transmitterRadio,
                alloc.packet,
                transmission->getStartTime(),
                transmission->getEndTime(),
                transmission->getPreambleDuration(),
                transmission->getHeaderDuration(),
                transmission->getDataDuration(),
                transmission->getStartPosition(),
                transmission->getEndPosition(),
                transmission->getStartOrientation(),
                transmission->getEndOrientation(),
                nullptr, nullptr, nullptr, nullptr, // models
                ruAnalogModel,
                ieee80211Trans->getMode(),
                ieee80211Trans->getChannel()
            );

            subTransmissions.push_back(ruTransmission);
            // Add the sub-transmission to the medium
            RadioMedium::addTransmission(transmitterRadio, ruTransmission);
        }

        muSubTransmissions[transmission] = subTransmissions;
        // Keep the main transmission registered on the medium for compatibility/logical tracking
        RadioMedium::addTransmission(transmitterRadio, transmission);
    }
    else {
        RadioMedium::addTransmission(transmitterRadio, transmission);
    }
}

void Ieee80211RadioMedium::removeTransmission(const ITransmission *transmission)
{
    auto it = muSubTransmissions.find(transmission);
    if (it != muSubTransmissions.end()) {
        for (auto subTransmission : it->second) {
            RadioMedium::removeTransmission(subTransmission);
        }
        muSubTransmissions.erase(it);
    }
    RadioMedium::removeTransmission(transmission);
}

void Ieee80211RadioMedium::sendToRadio(IRadio *transmitter, const IRadio *receiver, const IWirelessSignal *signal)
{
    auto mainTransmission = signal->getTransmission();
    auto it = muSubTransmissions.find(mainTransmission);
    if (it != muSubTransmissions.end()) {
        for (auto subTransmission : it->second) {
            auto subSignal = new WirelessSignal(subTransmission);
            RadioMedium::sendToRadio(transmitter, receiver, subSignal);
            delete subSignal;
        }
    }
    else {
        RadioMedium::sendToRadio(transmitter, receiver, signal);
    }
}

bool Ieee80211RadioMedium::isPotentialReceiver(const IRadio *receiver, const ITransmission *transmission) const
{
    // A main HE MU transmission should not propagate physically (only its sub-transmissions should)
    if (transmission->getPacket() != nullptr && transmission->getPacket()->findTag<Ieee80211HeMuTag>() != nullptr)
        return false;
    return RadioMedium::isPotentialReceiver(receiver, transmission);
}

bool Ieee80211RadioMedium::isInterferingTransmission(const ITransmission *transmission, const IListening *listening) const
{
    // A main HE MU transmission does not cause interference directly (its sub-transmissions do)
    if (transmission->getPacket() != nullptr && transmission->getPacket()->findTag<Ieee80211HeMuTag>() != nullptr)
        return false;
    return RadioMedium::isInterferingTransmission(transmission, listening);
}

bool Ieee80211RadioMedium::isInterferingTransmission(const ITransmission *transmission, const IReception *reception) const
{
    // A main HE MU transmission does not cause interference directly (its sub-transmissions do)
    if (transmission->getPacket() != nullptr && transmission->getPacket()->findTag<Ieee80211HeMuTag>() != nullptr)
        return false;
    return RadioMedium::isInterferingTransmission(transmission, reception);
}

} // namespace physicallayer
} // namespace inet
