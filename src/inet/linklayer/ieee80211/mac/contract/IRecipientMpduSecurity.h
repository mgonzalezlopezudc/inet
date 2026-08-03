//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IRECIPIENTMPDUSECURITY_H
#define __INET_IRECIPIENTMPDUSECURITY_H

#include "inet/common/packet/Packet.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {
namespace ieee80211 {

/**
 * Optional recipient-side MPDU integrity and replay-checking contract.
 *
 * The caller retains ownership of the packet. Implementations must neither
 * take nor delete it; they only classify the received MPDU.
 */
class INET_API IRecipientMpduSecurity
{
  public:
    enum class Outcome {
        ACCEPT,
        INTEGRITY_FAILED,
        REPLAY_DETECTED,
    };

    enum class Stage {
        INTEGRITY_AND_DECRYPTION,
        REPLAY_DETECTION,
    };

  public:
    virtual ~IRecipientMpduSecurity() {}

    virtual Outcome checkReceivedMpdu(const Packet *packet,
            const Ptr<const Ieee80211DataOrMgmtHeader>& header,
            Stage stage) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
