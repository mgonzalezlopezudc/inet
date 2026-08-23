//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEEE80211PEERCAPABILITIES_H
#define __INET_IIEEE80211PEERCAPABILITIES_H

#include <array>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"

namespace inet {
namespace ieee80211 {

enum class Ieee80211CapabilityStatus {
    UNKNOWN,
    UNSUPPORTED,
    SUPPORTED
};

struct Ieee80211PeerLdpcStatus {
    Ieee80211CapabilityStatus htRxLdpc = Ieee80211CapabilityStatus::UNKNOWN;
    Ieee80211CapabilityStatus vhtRxLdpc = Ieee80211CapabilityStatus::UNKNOWN;
    Ieee80211CapabilityStatus operatingModeNotification = Ieee80211CapabilityStatus::UNKNOWN;
    bool htRxMcsSetKnown = false;
    std::array<uint8_t, 10> htRxMcsSet = {};
    int maximumHtRxBandwidthMhz = -1;
    bool vhtRxMcsMapKnown = false;
    uint16_t vhtRxMcsMap = 0xFFFF;
    bool vhtTxMcsMapKnown = false;
    uint16_t vhtTxMcsMap = 0xFFFF;
    int maximumVhtRxBandwidthMhz = -1;
    bool hasOperatingMode = false;
    bool noLdpcPreferred = false;
    bool operatingModeType0Valid = false;
    int operatingModeMaximumBandwidthMhz = -1;
    int operatingModeMaximumSpatialStreams = -1;
};

struct Ieee80211IntendedReceiverSet {
    bool complete = false;
    std::vector<MacAddress> receivers;
};

struct Ieee80211VhtSigAParameters {
    bool known = false;
    uint8_t groupId = 63;
    uint16_t partialAid = 0;
};

/**
 * Read-only management-owned view of learned peer LDPC capabilities.
 */
class INET_API IIeee80211PeerCapabilities
{
  public:
    virtual ~IIeee80211PeerCapabilities() {}

    virtual Ieee80211PeerLdpcStatus getPeerLdpcStatus(const MacAddress& peer) const = 0;
    virtual Ieee80211IntendedReceiverSet resolveIntendedReceivers(const MacAddress& receiverAddress) const = 0;
    virtual Ieee80211VhtSigAParameters getVhtSigAParameters(const MacAddress& receiverAddress) const = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
