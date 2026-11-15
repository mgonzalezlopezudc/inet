// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154InterfaceData.h"
#include "inet/common/Simsignals.h"

namespace inet {

Register_Class(Ieee802154InterfaceData);

void Ieee802154InterfaceData::setIdentity(const Ieee802154Address& extended, const Ieee802154Address& shortValue, uint16_t pan)
{
    if (!extended.isExtended() || extended.isExtendedGroup() || !shortValue.isShort())
        throw cRuntimeError("Invalid native IEEE 802.15.4 interface identity");
    if (extendedAddress != extended || shortAddress != shortValue || panId != pan) {
        extendedAddress = extended;
        shortAddress = shortValue;
        panId = pan;
        changed(interfaceConfigChangedSignal, 0);
    }
}

std::string Ieee802154InterfaceData::str() const
{
    return extendedAddress.str() + " / " + shortAddress.str() + " PAN " + std::to_string(panId);
}

} // namespace inet
