// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154GtsManager.h"

#include <algorithm>

namespace inet {

bool Ieee802154GtsManager::sameKey(const Key& a, const Key& b)
{
    return a.panId == b.panId && a.extendedAddress == b.extendedAddress && a.shortAddress == b.shortAddress && a.direction == b.direction;
}

bool Ieee802154GtsManager::available(const Key& key) const
{
    std::vector<Key> keys;
    auto check = [&](const Key& existing) {
        if (sameKey(existing,key))
            return true;
        if (existing.direction == key.direction &&
                (existing.shortAddress == key.shortAddress || (existing.panId == key.panId && existing.extendedAddress == key.extendedAddress)))
            return false;
        if (std::none_of(keys.begin(),keys.end(),[&](const Key& candidate) { return sameKey(candidate,existing); }))
            keys.push_back(existing);
        return true;
    };
    bool present = false;
    for (const auto& grant : grants) {
        if (!check(grant.key)) return false;
        present |= sameKey(grant.key,key);
    }
    for (const auto& advertisement : advertisements) {
        if (!check(advertisement.key)) return false;
        present |= sameKey(advertisement.key,key);
    }
    return present || keys.size() < 7;
}

const Ieee802154GtsManager::Grant *Ieee802154GtsManager::findGrant(const Key& key) const
{
    auto it = std::find_if(grants.begin(),grants.end(),[&](const Grant& value) { return sameKey(value.key,key); });
    return it == grants.end() ? nullptr : &*it;
}

unsigned int Ieee802154GtsManager::inactivityLimit(uint8_t beaconOrder)
{
    return beaconOrder <= 8 ? 2u << (8 - beaconOrder) : 2u;
}

void Ieee802154GtsManager::armActivity(const Key& key, int64_t previousEpoch, bool rebaseline)
{
    for (auto& grant : grants)
        if (sameKey(grant.key,key) && (!grant.activityArmed || rebaseline)) {
            grant.activityArmed = true;
            grant.inactive = 0;
            grant.activeInOpportunity = false;
            grant.lastOpportunity = previousEpoch;
        }
}

bool Ieee802154GtsManager::noteActivity(const Key& key)
{
    for (auto& grant : grants)
        if (sameKey(grant.key,key) && grant.activityArmed && !grant.activitySuspended) {
            grant.inactive = 0;
            grant.activeInOpportunity = true;
            return true;
        }
    return false;
}

void Ieee802154GtsManager::rephaseActivity(const Key& key, int64_t previousEpoch)
{
    for (auto& grant : grants)
        if (sameKey(grant.key,key) && grant.activityArmed) {
            grant.lastOpportunity = previousEpoch;
            grant.activeInOpportunity = false;
        }
}

void Ieee802154GtsManager::setActivitySuspended(const Key& key, bool suspended)
{
    for (auto& grant : grants)
        if (sameKey(grant.key,key) && grant.activityArmed) {
            if (grant.activitySuspended && !suspended) {
                grant.inactive = 0;
                grant.activeInOpportunity = false;
            }
            grant.activitySuspended = suspended;
        }
}

bool Ieee802154GtsManager::finishOpportunity(const Key& key, uint8_t beaconOrder, int64_t epoch)
{
    for (auto& grant : grants)
        if (sameKey(grant.key,key) && grant.activityArmed && (!grant.lastOpportunity || epoch > *grant.lastOpportunity)) {
            grant.lastOpportunity = epoch;
            if (grant.activitySuspended)
                return false;
            if (grant.activeInOpportunity)
                grant.activeInOpportunity = false;
            else if (++grant.inactive >= inactivityLimit(beaconOrder))
                return true;
        }
    return false;
}

void Ieee802154GtsManager::copyActivityFrom(const Ieee802154GtsManager& source)
{
    for (auto& grant : grants)
        if (auto current = source.findGrant(grant.key)) {
            grant.inactive = current->inactive;
            grant.activityArmed = current->activityArmed;
            grant.activitySuspended = current->activitySuspended;
            grant.activeInOpportunity = current->activeInOpportunity;
            grant.lastOpportunity = current->lastOpportunity;
        }
}

size_t Ieee802154GtsManager::reservedCount() const
{
    std::vector<Key> keys;
    auto add = [&](const Key& key) {
        if (std::none_of(keys.begin(),keys.end(),[&](const Key& existing) { return sameKey(existing,key); }))
            keys.push_back(key);
    };
    for (const auto& grant : grants) add(grant.key);
    for (const auto& advertisement : advertisements) add(advertisement.key);
    return keys.size();
}

bool Ieee802154GtsManager::putGrant(const Grant& grant)
{
    if (!grant.start || !grant.length || grant.start > 15 || grant.length > 15 || grant.start + grant.length > 16 ||
            grant.key.shortAddress >= 0xfffe || uint8_t(grant.key.direction) > 1 || !available(grant.key))
        return false;
    auto it = std::find_if(grants.begin(),grants.end(),[&](const Grant& value) { return sameKey(value.key,grant.key); });
    if (it == grants.end()) grants.push_back(grant);
    else *it = grant;
    return true;
}

bool Ieee802154GtsManager::removeGrant(const Key& key)
{
    auto it = std::find_if(grants.begin(),grants.end(),[&](const Grant& value) { return sameKey(value.key,key); });
    if (it == grants.end()) return false;
    grants.erase(it);
    return true;
}

bool Ieee802154GtsManager::removeAdvertisement(const Key& key)
{
    auto it = std::find_if(advertisements.begin(),advertisements.end(),[&](const Advertisement& value) { return sameKey(value.key,key); });
    if (it == advertisements.end()) return false;
    advertisements.erase(it);
    return true;
}

bool Ieee802154GtsManager::advertise(const Key& key, uint8_t start, uint8_t length)
{
    if (start > 15 || length > 15 || (start && (!length || start + length > 16)) ||
            key.shortAddress >= 0xfffe || uint8_t(key.direction) > 1 || !available(key))
        return false;
    auto it = std::find_if(advertisements.begin(),advertisements.end(),[&](const Advertisement& value) { return sameKey(value.key,key); });
    if (nextRevision == UINT64_MAX)
        throw cRuntimeError("IEEE 802.15.4 GTS advertisement revision exhausted");
    Advertisement value{key,start,length,4,++nextRevision,false};
    if (it == advertisements.end()) advertisements.push_back(value);
    else *it = value;
    return true;
}

void Ieee802154GtsManager::markPublished(const std::vector<Advertisement>& snapshot)
{
    for (const auto& sent : snapshot)
        for (auto& current : advertisements)
            if (sent.revision == current.revision)
                current.published = true;
}

void Ieee802154GtsManager::ageAdvertisements()
{
    for (auto& advertisement : advertisements)
        if (advertisement.published)
            --advertisement.remaining;
    advertisements.erase(std::remove_if(advertisements.begin(),advertisements.end(),[](const Advertisement& value) { return value.remaining == 0; }),advertisements.end());
}

} // namespace inet
