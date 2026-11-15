// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/physicallayer/wireless/ieee802154/Ieee802154EnergyObservation.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarNoise.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarReceptionAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IAntenna.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IReceiver.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/BandListening.h"

namespace inet {
namespace physicallayer {

void Ieee802154EnergyObservation::clear()
{
    contributions.clear();
    listening.reset();
    radio = nullptr;
}

void Ieee802154EnergyObservation::begin(const IRadio *newRadio, simtime_t start, simtime_t end)
{
    if (!newRadio || start != simTime() || end <= start)
        throw cRuntimeError("Energy observation requires a radio and a positive interval starting now");
    clear();
    radio = newRadio;
    auto position = radio->getAntenna()->getMobility()->getCurrentPosition();
    listening.reset(radio->getReceiver()->createListening(radio, start, end, position, position));
    if (!dynamic_cast<const BandListening *>(listening.get()))
        throw cRuntimeError("IEEE 802.15.4 scalar energy observation requires band listening");
    std::unique_ptr<const IInterference> interference(radio->getMedium()->computeInterference(radio, listening.get()));
    for (auto reception : *interference->getInterferingReceptions())
        include(reception);
}

void Ieee802154EnergyObservation::include(const IReception *reception)
{
    if (!listening || reception->getReceiverRadio() != radio)
        return;
    auto start = std::max(listening->getStartTime(), reception->getStartTime());
    auto end = std::min(listening->getEndTime(), reception->getEndTime());
    if (start >= end)
        return;
    auto scalar = dynamic_cast<const ScalarReceptionAnalogModel *>(reception->getAnalogModel());
    auto band = static_cast<const BandListening *>(listening.get());
    if (!scalar)
        throw cRuntimeError("IEEE 802.15.4 energy observation supports scalar receptions only");
    auto separation = std::abs((scalar->getCenterFrequency() - band->getCenterFrequency()).get<Hz>());
    if (separation >= (scalar->getBandwidth() + band->getBandwidth()).get<Hz>() / 2)
        return;
    if (scalar->getCenterFrequency() != band->getCenterFrequency() || scalar->getBandwidth() > band->getBandwidth())
        throw cRuntimeError("Partially overlapping energy requires a dimensional PHY");
    contributions[reception->getTransmission()->getId()] = {start, end, scalar->getPower()};
}

Ieee802154EnergyObservation::Result Ieee802154EnergyObservation::finish()
{
    if (!listening || simTime() != listening->getEndTime())
        throw cRuntimeError("Energy observation must finish at its requested end");
    auto start = listening->getStartTime(), end = listening->getEndTime();
    std::unique_ptr<const INoise> background(radio->getMedium()->getBackgroundNoise() ?
            radio->getMedium()->getBackgroundNoise()->computeNoise(listening.get()) : nullptr);
    auto scalarBackground = dynamic_cast<const ScalarNoise *>(background.get());
    if (background && !scalarBackground)
        throw cRuntimeError("IEEE 802.15.4 scalar energy observation requires scalar background noise");
    std::map<simtime_t, W> changes;
    changes[start] = W(0);
    changes[end] = W(0);
    for (const auto& entry : contributions) {
        const auto& contribution = entry.second;
        changes.try_emplace(contribution.start, W(0)).first->second += contribution.power;
        changes.try_emplace(contribution.end, W(0)).first->second -= contribution.power;
    }
    W current(0), peak(0);
    double integral = 0;
    for (auto it = changes.begin(); it != changes.end(); ++it) {
        current += it->second;
        auto next = std::next(it);
        if (next == changes.end())
            break;
        auto duration = next->first - it->first;
        integral += current.get<W>() * duration.dbl();
        W backgroundPeak(0);
        if (background)
            backgroundPeak = background->computeMaxPower(it->first, SimTime::fromRaw(next->first.raw() - 1));
        peak = std::max(peak, current + backgroundPeak);
    }
    W mean(integral / (end - start).dbl());
    if (scalarBackground) {
        math::Interval<simtime_t> interval(math::Point<simtime_t>(start), math::Point<simtime_t>(end), 1, 0, 0);
        mean += scalarBackground->getPower()->getMean(interval);
    }
    clear();
    return {peak, mean};
}

} // namespace physicallayer
} // namespace inet
