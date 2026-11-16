// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef INET_IEE_E802154_GTSMANAGER_H
#define INET_IEE_E802154_GTSMANAGER_H

#include <vector>
#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee802154/Ieee802154GtsValues.h"

namespace inet {

/** Value ledger; the MAC owns the superframe clock and calls ageAdvertisements. */
class INET_API Ieee802154GtsManager
{
  public:
    struct Key { uint16_t panId; uint64_t extendedAddress; uint16_t shortAddress; Ieee802154GtsDirection direction; };
    struct Grant { Key key; uint8_t start; uint8_t length; };
    struct Advertisement { Key key; uint8_t start; uint8_t length; unsigned int remaining; uint64_t revision; bool published; };

  private:
    std::vector<Grant> grants;
    std::vector<Advertisement> advertisements;
    uint64_t nextRevision = 0;
    static bool sameKey(const Key& a, const Key& b);
    [[nodiscard]] bool available(const Key& key) const;

  public:
    [[nodiscard]] const std::vector<Grant>& getGrants() const { return grants; }
    [[nodiscard]] const std::vector<Advertisement>& getAdvertisements() const { return advertisements; }
    [[nodiscard]] const Grant *findGrant(const Key& key) const;
    [[nodiscard]] bool canReserve(const Key& key) const { return available(key); }
    [[nodiscard]] size_t reservedCount() const;
    bool putGrant(const Grant& grant);
    bool removeGrant(const Key& key);
    bool advertise(const Key& key, uint8_t start, uint8_t length);
    void markPublished(const std::vector<Advertisement>& snapshot);
    void ageAdvertisements();
    void clearAdvertisements() { advertisements.clear(); }
    void clear() { grants.clear(); advertisements.clear(); }
};

} // namespace inet
#endif
