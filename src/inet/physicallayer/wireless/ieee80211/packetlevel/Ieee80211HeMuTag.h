//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEMUTAG_H
#define __INET_IEEE80211HEMUTAG_H

#include "inet/common/TagBase.h"
#include "inet/common/packet/Packet.h"
#include <vector>

namespace inet {
namespace physicallayer {

struct Ieee80211HeMuRuAllocation {
    int ruIndex;
    Packet *packet; // The packet destined for this RU
};

class INET_API Ieee80211HeMuTag : public TagBase
{
  protected:
    std::vector<Ieee80211HeMuRuAllocation> allocations;

  public:
    Ieee80211HeMuTag() {}
    virtual ~Ieee80211HeMuTag() {
        for (auto& alloc : allocations) {
            delete alloc.packet;
        }
    }

    const std::vector<Ieee80211HeMuRuAllocation>& getAllocations() const { return allocations; }
    void addAllocation(int ruIndex, Packet *packet) { allocations.push_back({ruIndex, packet}); }
    void setAllocations(const std::vector<Ieee80211HeMuRuAllocation>& allocs) { allocations = allocs; }

    virtual Ieee80211HeMuTag *dup() const override {
        auto tag = new Ieee80211HeMuTag();
        std::vector<Ieee80211HeMuRuAllocation> dupAllocs;
        for (const auto& alloc : allocations) {
            Packet *dupPkt = nullptr;
            if (alloc.packet) {
                dupPkt = alloc.packet->dup();
            }
            dupAllocs.push_back({alloc.ruIndex, dupPkt});
        }
        tag->setAllocations(dupAllocs);
        return tag;
    }

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override {
        stream << "Ieee80211HeMuTag: numAllocations = " << allocations.size();
        return stream;
    }
};

} // namespace physicallayer
} // namespace inet

#endif // __INET_IEEE80211HEMUTAG_H
