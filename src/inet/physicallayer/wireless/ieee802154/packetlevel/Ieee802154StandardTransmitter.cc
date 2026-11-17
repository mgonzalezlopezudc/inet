// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/physicallayer/wireless/ieee802154/packetlevel/Ieee802154StandardTransmitter.h"
#include "inet/physicallayer/wireless/ieee802154/packetlevel/Ieee802154StandardTransmission.h"
#include "inet/mobility/contract/IMobility.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/RadioControlInfo_m.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee802154StandardTransmitter);

const ITransmission *Ieee802154StandardTransmitter::createTransmission(const IRadio *radio, const Packet *packet, simtime_t startTime) const
{
    auto length = packet->getDataLength();
    auto rate = computeTransmissionDataBitrate(packet);
    if (length < b(0) || length > B(127) || length.get<b>() % 8 || rate != kbps(250) ||
            headerLength != b(16) || preambleDuration != SimTime(128,SIMTIME_US))
        throw cRuntimeError("Invalid selected O-QPSK PSDU/rate/overhead configuration");
    auto headerDuration = SimTime(headerLength.get<b>() / rate.get<bps>());
    auto dataDuration = SimTime(length.get<b>() / rate.get<bps>());
    auto endTime = startTime + preambleDuration + headerDuration + dataDuration;
    auto mobility = radio->getAntenna()->getMobility();
    auto position = mobility->getCurrentPosition();
    auto orientation = mobility->getCurrentAngularPosition();
    auto analogModel = getAnalogModel()->createAnalogModel(preambleDuration,headerDuration,dataDuration,
            centerFrequency,bandwidth,computeTransmissionPower(packet));
    return new Ieee802154StandardTransmission(radio,packet,startTime,endTime,preambleDuration,headerDuration,dataDuration,
            position,position,orientation,orientation,nullptr,nullptr,nullptr,nullptr,analogModel,
            headerLength,length,modulation,bandwidth,rate);
}

} // namespace physicallayer
} // namespace inet
