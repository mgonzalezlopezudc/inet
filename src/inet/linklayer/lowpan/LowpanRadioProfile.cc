// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanRadioProfile.h"
#include "inet/linklayer/ieee802154/Ieee802154FrameFormat.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IAntenna.h"
#include "inet/physicallayer/wireless/common/propagation/ConstantSpeedPropagation.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatRadioBase.h"
#include "inet/physicallayer/wireless/ieee802154/packetlevel/Ieee802154NarrowbandTransmitter.h"
#include "inet/mobility/static/StationaryMobility.h"
namespace inet { namespace lowpan {
simtime_t LowpanRadioProfile::maximumPropagation(const physicallayer::IRadio *sender,
        const std::vector<const physicallayer::IRadio *>& peers)
{
    auto propagation = dynamic_cast<const physicallayer::ConstantSpeedPropagation *>(sender->getMedium()->getPropagation());
    if (propagation == nullptr)
        throw cRuntimeError("LoWPAN delivery deadlines require ConstantSpeedPropagation");
    double speed = propagation->getPropagationSpeed().get<mps>();
    if (!std::isfinite(speed) || speed <= 0)
        throw cRuntimeError("LoWPAN propagation speed must be finite and positive");
    auto sourcePosition = sender->getAntenna()->getMobility()->getCurrentPosition();
    simtime_t maximum = SIMTIME_ZERO;
    for (auto peer : peers) {
        auto mobility = dynamic_cast<StationaryMobility *>(peer->getAntenna()->getMobility());
        if (mobility == nullptr || mobility->par("updateFromDisplayString").boolValue())
            throw cRuntimeError("LoWPAN delivery deadlines require StationaryMobility with updateFromDisplayString=false");
        double distance = sourcePosition.distance(mobility->getCurrentPosition());
        if (!std::isfinite(distance))
            throw cRuntimeError("LoWPAN deadline propagation requires finite stationary coordinates");
        maximum = std::max(maximum, SimTime(distance / speed));
    }
    return maximum;
}

simtime_t LowpanRadioProfile::transmissionTail(const physicallayer::IRadio *radio, simtime_t turnaround, simtime_t propagation)
{
    if (dynamic_cast<const physicallayer::FlatRadioBase *>(radio) == nullptr)
        throw cRuntimeError("LoWPAN delivery deadlines require a flat packet radio");
    auto transmitter = dynamic_cast<const physicallayer::Ieee802154NarrowbandTransmitter *>(radio->getTransmitter());
    if (transmitter == nullptr)
        throw cRuntimeError("LoWPAN delivery deadlines require the IEEE 802.15.4 narrowband transmitter");
    double bitrate = transmitter->getBitrate().get<bps>();
    auto headerBits = transmitter->getHeaderLength().get();
    simtime_t preamble = transmitter->par("preambleDuration");
    if (!std::isfinite(bitrate) || bitrate <= 0 || headerBits < 0 || preamble < SIMTIME_ZERO || turnaround < SIMTIME_ZERO)
        throw cRuntimeError("Invalid LoWPAN transmitter timing profile");
    return turnaround + preamble + SimTime(headerBits / bitrate) + SimTime(Ieee802154FrameFormat::MAX_FRAME_BYTES * 8 / bitrate) + propagation + SimTime::fromRaw(1);
}

void LowpanRadioProfile::validateInputPath(const physicallayer::IRadio *radio)
{
    for (auto gate = radio->getRadioGate()->getPathStartGate(); gate != nullptr; gate = gate->getNextGate()) {
        auto channel = gate->getChannel();
        if (channel != nullptr && typeid(*channel) != typeid(cIdealChannel))
            throw cRuntimeError("LoWPAN deadline profile does not allow radio input delay channels");
    }
}
} }
