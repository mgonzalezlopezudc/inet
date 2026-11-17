// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154INTERFACEDATA_H
#define __INET_IEEE802154INTERFACEDATA_H

#include "inet/common/Protocol.h"
#include "inet/linklayer/ieee802154/Ieee802154Address.h"
#include "inet/networklayer/common/NetworkInterface.h"

namespace inet {

/** Published native identity; the MAC PIB remains the authoritative owner. */
class INET_API Ieee802154InterfaceData : public InterfaceProtocolData
{
  protected:
    Ieee802154Address extendedAddress;
    Ieee802154Address shortAddress;
    uint16_t panId = 0xffff;

  public:
    Ieee802154InterfaceData() : InterfaceProtocolData(Protocol::ieee802154.getId()) {}
    virtual Ieee802154InterfaceData *dup() const override { return new Ieee802154InterfaceData(*this); }
    const Ieee802154Address& getExtendedAddress() const { return extendedAddress; }
    const Ieee802154Address& getShortAddress() const { return shortAddress; }
    uint16_t getPanId() const { return panId; }
    void setIdentity(const Ieee802154Address& extended, const Ieee802154Address& shortValue, uint16_t pan);
    virtual std::string str() const override;
};

} // namespace inet
#endif
