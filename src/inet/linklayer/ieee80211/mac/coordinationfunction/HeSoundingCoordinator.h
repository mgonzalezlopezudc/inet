//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HESOUNDINGCOORDINATOR_H
#define __INET_HESOUNDINGCOORDINATOR_H

#include <vector>
#include <ostream>
#include "omnetpp.h"
#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingService.h"

namespace inet {
namespace ieee80211 {

class INET_API HeSoundingCoordinator : public omnetpp::cSimpleModule
{
  public:
    using SoundingTarget = HeSoundingService::SoundingTarget;

  protected:
    virtual void initialize(int stage) override;

    HeSoundingService standaloneService;
    HeSoundingService *service = &standaloneService;

  public:
    HeSoundingCoordinator() {}
    virtual ~HeSoundingCoordinator() {}
    void configure(HeSoundingService *value) { service = value == nullptr ? &standaloneService : value; }
    HeSoundingService& getService() const { return *service; }

    // NED-compatible adapter over the typed sounding service.
    bool processSoundingFrame(Packet *packet,
                              const Ptr<const Ieee80211MacHeader>& header,
                              const HeSoundingService::ReceiveSnapshot& snapshot,
                              const HeSoundingService::ReceiveActions& actions)
        { return service->processReceivedFrame(packet, header, snapshot, actions); }

    // STA: process legacy preamble (NDP)
    void processLegacyPreamble(const Packet *packet) {
        service->processNdpIndication(true);
    }

    void resetStaSoundingState() {
        service->resetStaState();
    }
};

inline std::ostream& operator<<(std::ostream& os, const HeSoundingCoordinator::SoundingTarget& target)
{
    os << "address=" << target.address << " aid=" << target.aid << " maxNss=" << target.maxNss;
    return os;
}

} // namespace ieee80211
} // namespace inet

#endif // __INET_HESOUNDINGCOORDINATOR_H
