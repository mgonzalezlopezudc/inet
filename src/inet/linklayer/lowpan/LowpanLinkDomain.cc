// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanLinkDomain.h"

#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee802154/Ieee802154Mac.h"
#include "inet/linklayer/lowpan/LowpanCompatibilityMac.h"
#include "inet/linklayer/lowpan/LowpanRadioProfile.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IAntenna.h"
#include "inet/physicallayer/wireless/common/propagation/ConstantSpeedPropagation.h"
#include "inet/mobility/static/StationaryMobility.h"

namespace inet { namespace lowpan {

Define_Module(LowpanLinkDomain);

static const char *requiredAttribute(cXMLElement *element, const char *name)
{
    auto value = element->getAttribute(name);
    if (value == nullptr || *value == '\0')
        throw cRuntimeError("LoWPAN domain element requires attribute '%s'", name);
    return value;
}

void LowpanLinkDomain::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        int configuredPan = par("panId");
        if (configuredPan < 0 || configuredPan >= 0xffff)
            throw cRuntimeError("LoWPAN domain requires a non-broadcast 16-bit PAN ID");
        panId = configuredPan;
        medium = getModuleFromPar<physicallayer::IRadioMedium>(par("radioMediumModule"), this);
        auto config = par("config").xmlValue();
        auto root = getSimulation()->getSystemModule();
        for (auto element : config->getChildrenByTagName("peer")) {
            auto networkInterface = check_and_cast<NetworkInterface *>(root->getModuleByPath(requiredAttribute(element, "interface")));
            auto radio = check_and_cast<physicallayer::IRadio *>(networkInterface->getSubmodule("radio"));
            Peer peer{networkInterface, radio, Ieee802154Address(requiredAttribute(element, "native")),
                      MacAddress(requiredAttribute(element, "alias"))};
            if (peers.count(networkInterface->getId()))
                throw cRuntimeError("Duplicate LoWPAN domain interface");
            addressMap.addPeer(peer.address, peer.alias);
            peers.emplace(networkInterface->getId(), peer);
        }
        if (peers.empty())
            throw cRuntimeError("LoWPAN domain has no peers");
        for (auto element : config->getChildrenByTagName("neighbor")) {
            auto networkInterface = check_and_cast<NetworkInterface *>(root->getModuleByPath(requiredAttribute(element, "interface")));
            getPeer(networkInterface);
            addressMap.bind(networkInterface->getId(), Ipv6Address(requiredAttribute(element, "ipv6")),
                            Ieee802154Address(requiredAttribute(element, "native")));
        }
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        for (const auto& entry : peers) {
            const auto& peer = entry.second;
            if (peer.radio->getMedium() != medium)
                throw cRuntimeError("LoWPAN domain peers must share the configured radio medium");
            if (peer.networkInterface->getMacAddress() != peer.alias)
                throw cRuntimeError("LoWPAN domain alias differs from initialized interface address");
            auto mac = dynamic_cast<LowpanCompatibilityMac *>(peer.networkInterface->getSubmodule("mac"));
            if (mac == nullptr)
                throw cRuntimeError("LoWPAN domain requires LowpanCompatibilityMac peers");
            if (mac->getLinkDomain() != this)
                throw cRuntimeError("LoWPAN domain peer MAC belongs to a different domain");
            LowpanRadioProfile::validateInputPath(peer.radio);
        }
        validateParticipants(getSimulation()->getSystemModule());
        for (const auto& entry : peers)
            getMaximumPropagationDelay(entry.second.networkInterface);
    }
}

simtime_t LowpanLinkDomain::getMaximumPropagationDelay(const NetworkInterface *sender) const
{
    std::vector<const physicallayer::IRadio *> radios;
    for (const auto& entry : peers)
        radios.push_back(entry.second.radio);
    return LowpanRadioProfile::maximumPropagation(getPeer(sender).radio, radios);
}

void LowpanLinkDomain::validateParticipants(cModule *module) const
{
    if (auto radio = dynamic_cast<physicallayer::IRadio *>(module)) {
        if (radio->getMedium() == medium) {
            bool listed = false;
            for (const auto& entry : peers)
                listed |= entry.second.radio == radio;
            if (!listed)
                throw cRuntimeError("Unlisted radio shares dedicated LoWPAN compatibility medium: %s", module->getFullPath().c_str());
        }
    }
    if (auto domain = dynamic_cast<LowpanLinkDomain *>(module)) {
        if (domain != this && domain->medium == medium)
            throw cRuntimeError("Multiple LoWPAN domains cannot share a compatibility radio medium");
    }
    for (cModule::SubmoduleIterator it(module); !it.end(); ++it)
        validateParticipants(*it);
}

const LowpanLinkDomain::Peer& LowpanLinkDomain::getPeer(const NetworkInterface *networkInterface) const
{
    auto it = peers.find(networkInterface->getId());
    if (it == peers.end())
        throw cRuntimeError("Interface is not a member of this LoWPAN domain");
    return it->second;
}

void LowpanLinkDomain::handleMessage(cMessage *message)
{
    throw cRuntimeError("LoWPAN domain does not receive messages");
}

} } // namespace inet::lowpan
