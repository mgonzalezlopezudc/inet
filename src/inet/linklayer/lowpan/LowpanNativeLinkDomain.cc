// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanNativeLinkDomain.h"
#include "inet/linklayer/lowpan/LowpanNativeMac.h"
#include "inet/linklayer/lowpan/LowpanRadioProfile.h"
#include "inet/common/ModuleAccess.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
namespace inet { namespace lowpan {
Define_Module(LowpanNativeLinkDomain);

void LowpanNativeLinkDomain::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        int configuredPan = par("panId");
        if (configuredPan < 0 || configuredPan >= 0xffff)
            throw cRuntimeError("Native LoWPAN domain requires a non-broadcast PAN ID");
        panId = configuredPan;
        medium = getModuleFromPar<physicallayer::IRadioMedium>(par("radioMediumModule"), this);
        auto root = getSimulation()->getSystemModule();
        auto attribute = [](cXMLElement *element, const char *name) {
            auto value = element->getAttribute(name);
            if (value == nullptr || *value == '\0')
                throw cRuntimeError("Native LoWPAN domain requires attribute '%s'", name);
            return value;
        };
        auto config = par("config").xmlValue();
        for (auto element : config->getChildrenByTagName("peer")) {
            auto interface = check_and_cast<NetworkInterface *>(root->getModuleByPath(attribute(element, "interface")));
            auto radio = check_and_cast<physicallayer::IRadio *>(interface->getSubmodule("radio"));
            Ieee802154Address address(attribute(element, "native"));
            if (address.getMode() != Ieee802154Address::EXTENDED || address.getInt() == UINT64_MAX || contains(address) || peers.count(interface->getId()))
                throw cRuntimeError("Invalid or duplicate native LoWPAN domain identity/interface");
            if (element->getAttribute("alias") != nullptr)
                throw cRuntimeError("Native LoWPAN peers must not configure compatibility aliases");
            peers.emplace(interface->getId(), Peer{interface, radio, address});
        }
        if (peers.empty())
            throw cRuntimeError("Native LoWPAN domain has no peers");
        for (auto element : config->getChildrenByTagName("neighbor")) {
            auto interface = check_and_cast<NetworkInterface *>(root->getModuleByPath(attribute(element, "interface")));
            getPeer(interface);
            Ipv6Address nextHop(attribute(element, "ipv6"));
            Ieee802154Address address(attribute(element, "native"));
            if (nextHop.isUnspecified() || nextHop.isMulticast() || !contains(address) ||
                !neighbors.emplace(std::make_pair(interface->getId(), nextHop), address).second)
                throw cRuntimeError("Invalid or duplicate native LoWPAN next-hop binding");
        }
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        for (const auto& entry : peers) {
            auto& peer = entry.second;
            auto mac = dynamic_cast<LowpanNativeMac *>(peer.networkInterface->getSubmodule("mac"));
            if (peer.radio->getMedium() != medium || mac == nullptr || mac->getLinkDomain() != this)
                throw cRuntimeError("Native LoWPAN peer MAC/radio belongs to a different domain");
            LowpanRadioProfile::validateInputPath(peer.radio);
        }
        validateParticipants(getSimulation()->getSystemModule());
        for (const auto& entry : peers)
            getMaximumPropagationDelay(entry.second.networkInterface);
    }
}

void LowpanNativeLinkDomain::validateParticipants(cModule *module) const
{
    if (auto radio = dynamic_cast<physicallayer::IRadio *>(module)) {
        if (radio->getMedium() == medium) {
            bool found = false;
            for (const auto& entry : peers) found |= entry.second.radio == radio;
            if (!found) throw cRuntimeError("Unlisted radio shares native LoWPAN domain medium");
        }
    }
    if (auto domain = dynamic_cast<LowpanNativeLinkDomain *>(module))
        if (domain != this && domain->medium == medium)
            throw cRuntimeError("Multiple native LoWPAN domains share a dedicated medium");
    for (cModule::SubmoduleIterator it(module); !it.end(); ++it)
        validateParticipants(*it);
}

const LowpanNativeLinkDomain::Peer& LowpanNativeLinkDomain::getPeer(const NetworkInterface *interface) const
{
    auto it = peers.find(interface->getId());
    if (it == peers.end()) throw cRuntimeError("Interface is not a native LoWPAN domain peer");
    return it->second;
}

bool LowpanNativeLinkDomain::contains(const Ieee802154Address& address) const
{
    for (const auto& entry : peers)
        if (entry.second.address == address) return true;
    return false;
}

const Ieee802154Address *LowpanNativeLinkDomain::resolveNeighbor(int interfaceId, const Ipv6Address& nextHop) const
{
    if (auto fixed = resolveStaticNeighbor(interfaceId, nextHop))
        return fixed;
    auto dynamic = dynamicNeighbors.find({interfaceId, nextHop});
    return dynamic == dynamicNeighbors.end() || dynamic->second.validUntil <= simTime() ? nullptr : &dynamic->second.address;
}

const Ieee802154Address *LowpanNativeLinkDomain::resolveStaticNeighbor(int interfaceModuleId, const Ipv6Address& nextHop) const
{
    auto it = neighbors.find({interfaceModuleId, nextHop});
    return it == neighbors.end() ? nullptr : &it->second;
}

bool LowpanNativeLinkDomain::installDynamicNeighbor(int interfaceModuleId, const Ipv6Address& nextHop,
        const Ieee802154Address& address, simtime_t validUntil, const cObject *owner)
{
    Enter_Method("installDynamicNeighbor");
    if (owner == nullptr || peers.count(interfaceModuleId) == 0 || nextHop.isUnspecified() || nextHop.isMulticast() ||
            !contains(address) || validUntil <= simTime())
        throw cRuntimeError("Invalid dynamic native LoWPAN next-hop binding");
    auto key = std::make_pair(interfaceModuleId, nextHop);
    auto fixed = neighbors.find(key);
    if (fixed != neighbors.end() && fixed->second != address)
        return false;
    auto existing = dynamicNeighbors.find(key);
    if (existing != dynamicNeighbors.end() && existing->second.validUntil > simTime() && existing->second.owner != owner)
        return false;
    dynamicNeighbors[key] = {address, validUntil, owner};
    return true;
}

bool LowpanNativeLinkDomain::removeDynamicNeighbor(int interfaceModuleId, const Ipv6Address& nextHop, const cObject *owner)
{
    Enter_Method("removeDynamicNeighbor");
    auto it = dynamicNeighbors.find({interfaceModuleId, nextHop});
    if (it == dynamicNeighbors.end() || it->second.owner != owner)
        return false;
    dynamicNeighbors.erase(it);
    return true;
}

simtime_t LowpanNativeLinkDomain::getMaximumPropagationDelay(const NetworkInterface *sender) const
{
    std::vector<const physicallayer::IRadio *> radios;
    for (const auto& entry : peers) radios.push_back(entry.second.radio);
    return LowpanRadioProfile::maximumPropagation(getPeer(sender).radio, radios);
}

void LowpanNativeLinkDomain::handleMessage(cMessage *message)
{
    throw cRuntimeError("Native LoWPAN domain has no runtime message protocol");
}
} }
