//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_AMPDUTRANSMISSIONLEDGER_H
#define __INET_AMPDUTRANSMISSIONLEDGER_H

#include <map>
#include <optional>
#include <vector>

#include "inet/common/INETDefs.h"

namespace inet {

class Packet;

namespace ieee80211 {

/** Non-owning ledger correlating original frame-sequence packets with A-MPDU members. */
class INET_API AmpduTransmissionLedger
{
  public:
    struct Entry {
        std::vector<Packet *> subframes;
        bool implicitBlockAck = false;
    };

  private:
    std::map<Packet *, Entry> entries;

  public:
    void record(Packet *packet, const std::vector<Packet *>& subframes,
            bool implicitBlockAck);
    bool hasImplicitBlockAck(Packet *packet) const;
    std::optional<Entry> take(Packet *packet);
    bool discard(Packet *packet);
};

} // namespace ieee80211
} // namespace inet

#endif
