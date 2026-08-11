//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFAGGREGATIONSERVICE_H
#define __INET_HCFAGGREGATIONSERVICE_H

#include <optional>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/linklayer/common/FcsMode_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/AmpduTransmissionLedger.h"

namespace inet {

class Packet;

namespace ieee80211 {

/**
 * Builds HCF A-MPDUs and keeps the non-owning transmission-to-constituent
 * mapping used after the aggregate has been transmitted.
 *
 * The service does not own packets, decide whether aggregation is allowed, or
 * emit HCF signals. Those responsibilities remain with Hcf and its EDCA/TXOP
 * collaborators.
 */
class INET_API HcfAggregationService
{
  private:
    AmpduTransmissionLedger transmissionLedger;

  public:
    static Packet *buildAmpduPacket(const std::vector<Packet *>& frames,
            FcsMode fcsMode);
    static B calculateAmpduLength(const std::vector<Packet *>& frames);

    void recordTransmission(Packet *packet,
            const std::vector<Packet *>& subframes, bool implicitBlockAck);
    bool hasImplicitBlockAck(Packet *packet) const;
    std::optional<AmpduTransmissionLedger::Entry> takeTransmission(Packet *packet);
    bool discardTransmission(Packet *packet);
};

} // namespace ieee80211
} // namespace inet

#endif
