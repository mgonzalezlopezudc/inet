//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HTCAPABILITIES_H
#define __INET_IEEE80211HTCAPABILITIES_H

#include <algorithm>
#include <array>
#include <set>

#include "inet/common/Units.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

// IEEE Std 802.11-2024, Table 9-230 and 10.27.3.
enum class Ieee80211HtProtectionMode : uint8_t {
    NO_PROTECTION = 0,
    NONMEMBER_PROTECTION = 1,
    TWENTY_MHZ_PROTECTION = 2,
    NON_HT_MIXED = 3,
};

// IEEE Std 802.11-2024, Table 9-227. Value 1 is reserved.
enum class Ieee80211HtMcsFeedback : uint8_t {
    NONE = 0,
    UNSOLICITED = 2,
    BOTH = 3,
};

enum class Ieee80211HtExplicitFeedback : uint8_t {
    NONE = 0,
    DELAYED = 1,
    IMMEDIATE = 2,
    BOTH = 3,
};

struct Ieee80211HtMcsNssMap
{
    std::array<int, 4> maxMcsPerNss;

    Ieee80211HtMcsNssMap() { maxMcsPerNss.fill(7); }
};

/** Model-backed subset of the HT Capabilities element (IEEE Std 802.11-2024, 9.4.2.54). */
struct Ieee80211HtCapabilities
{
    std::set<Hz> supportedChannelWidths = {MHz(20), MHz(40)};
    Ieee80211HtMcsNssMap rxMcsNss;
    Ieee80211HtMcsNssMap txMcsNss;
    bool ldpc = false;
    bool shortGi20 = false;
    bool shortGi40 = false;
    int maxAmpduLengthExponent = 3;
    Ieee80211HtMcsFeedback mcsFeedback = Ieee80211HtMcsFeedback::NONE;
    bool htcSupport = false;
    bool receiveNdp = false;
    bool transmitNdp = false;
    Ieee80211HtExplicitFeedback explicitCsiFeedback = Ieee80211HtExplicitFeedback::NONE;
    Ieee80211HtExplicitFeedback explicitNoncompressedFeedback = Ieee80211HtExplicitFeedback::NONE;
    Ieee80211HtExplicitFeedback explicitCompressedFeedback = Ieee80211HtExplicitFeedback::NONE;
};

/** Model-backed subset of the HT Operation element (IEEE Std 802.11-2024, 9.4.2.55). */
struct Ieee80211HtOperation
{
    Hz operatingChannelWidth = MHz(20);
    int primaryChannel = 0;
    int secondaryChannelOffset = 0;
    Ieee80211HtProtectionMode protectionMode = Ieee80211HtProtectionMode::NO_PROTECTION;
    Ieee80211HtMcsNssMap basicMcsNss;
};

struct Ieee80211HtDirectionalCapabilities
{
    bool valid = false;
    std::set<Hz> supportedChannelWidths;
    Ieee80211HtMcsNssMap mcsNss;
    bool ldpc = false;
    bool shortGi20 = false;
    bool shortGi40 = false;
    int receiverMaxAmpduLengthExponent = 0;
    bool mcsRequestAllowed = false;
    bool mcsFeedbackAllowed = false;
    bool htcSupported = false;
    bool transmitterCanSendNdp = false;
    bool receiverCanReceiveNdp = false;
    Ieee80211HtExplicitFeedback explicitCsiFeedback = Ieee80211HtExplicitFeedback::NONE;
    Ieee80211HtExplicitFeedback explicitNoncompressedFeedback = Ieee80211HtExplicitFeedback::NONE;
    Ieee80211HtExplicitFeedback explicitCompressedFeedback = Ieee80211HtExplicitFeedback::NONE;
};

struct Ieee80211NegotiatedHtCapabilities
{
    Ieee80211HtCapabilities localAdvertisement;
    Ieee80211HtCapabilities peerAdvertisement;
    Ieee80211HtDirectionalCapabilities localTxPeerRx;
    Ieee80211HtDirectionalCapabilities localRxPeerTx;
    Ieee80211HtOperation operation;
};

inline Ieee80211NegotiatedHtCapabilities negotiateHtCapabilities(const Ieee80211HtCapabilities& local,
        const Ieee80211HtCapabilities& peer, const Ieee80211HtOperation& operation)
{
    Ieee80211NegotiatedHtCapabilities negotiated;
    negotiated.localAdvertisement = local;
    negotiated.peerAdvertisement = peer;
    negotiated.operation = operation;
    negotiated.localTxPeerRx.supportedChannelWidths.clear();
    negotiated.localRxPeerTx.supportedChannelWidths.clear();
    for (const auto& width : local.supportedChannelWidths)
        if (peer.supportedChannelWidths.count(width)) {
            negotiated.localTxPeerRx.supportedChannelWidths.insert(width);
            negotiated.localRxPeerTx.supportedChannelWidths.insert(width);
        }
    negotiated.localTxPeerRx.mcsNss.maxMcsPerNss.fill(-1);
    negotiated.localRxPeerTx.mcsNss.maxMcsPerNss.fill(-1);
    for (size_t i = 0; i < 4; i++) {
        negotiated.localTxPeerRx.mcsNss.maxMcsPerNss[i] = local.txMcsNss.maxMcsPerNss[i] < 0 || peer.rxMcsNss.maxMcsPerNss[i] < 0 ? -1 : std::min(local.txMcsNss.maxMcsPerNss[i], peer.rxMcsNss.maxMcsPerNss[i]);
        negotiated.localRxPeerTx.mcsNss.maxMcsPerNss[i] = local.rxMcsNss.maxMcsPerNss[i] < 0 || peer.txMcsNss.maxMcsPerNss[i] < 0 ? -1 : std::min(local.rxMcsNss.maxMcsPerNss[i], peer.txMcsNss.maxMcsPerNss[i]);
    }
    negotiated.localTxPeerRx.ldpc = local.ldpc && peer.ldpc;
    negotiated.localRxPeerTx.ldpc = negotiated.localTxPeerRx.ldpc;
    negotiated.localTxPeerRx.shortGi20 = local.shortGi20 && peer.shortGi20;
    negotiated.localRxPeerTx.shortGi20 = negotiated.localTxPeerRx.shortGi20;
    negotiated.localTxPeerRx.shortGi40 = local.shortGi40 && peer.shortGi40;
    negotiated.localRxPeerTx.shortGi40 = negotiated.localTxPeerRx.shortGi40;
    negotiated.localTxPeerRx.receiverMaxAmpduLengthExponent = peer.maxAmpduLengthExponent;
    negotiated.localRxPeerTx.receiverMaxAmpduLengthExponent = local.maxAmpduLengthExponent;
    negotiated.localTxPeerRx.mcsRequestAllowed = local.htcSupport && peer.htcSupport &&
            peer.mcsFeedback == Ieee80211HtMcsFeedback::BOTH;
    negotiated.localRxPeerTx.mcsRequestAllowed = local.htcSupport && peer.htcSupport &&
            local.mcsFeedback == Ieee80211HtMcsFeedback::BOTH;
    negotiated.localTxPeerRx.mcsFeedbackAllowed = local.htcSupport && peer.htcSupport &&
            local.mcsFeedback != Ieee80211HtMcsFeedback::NONE;
    negotiated.localRxPeerTx.mcsFeedbackAllowed = local.htcSupport && peer.htcSupport &&
            peer.mcsFeedback != Ieee80211HtMcsFeedback::NONE;
    negotiated.localTxPeerRx.htcSupported = local.htcSupport && peer.htcSupport;
    negotiated.localRxPeerTx.htcSupported = negotiated.localTxPeerRx.htcSupported;
    negotiated.localTxPeerRx.transmitterCanSendNdp = local.transmitNdp;
    negotiated.localTxPeerRx.receiverCanReceiveNdp = peer.receiveNdp;
    negotiated.localTxPeerRx.explicitCsiFeedback = peer.explicitCsiFeedback;
    negotiated.localTxPeerRx.explicitNoncompressedFeedback = peer.explicitNoncompressedFeedback;
    negotiated.localTxPeerRx.explicitCompressedFeedback = peer.explicitCompressedFeedback;
    negotiated.localRxPeerTx.transmitterCanSendNdp = peer.transmitNdp;
    negotiated.localRxPeerTx.receiverCanReceiveNdp = local.receiveNdp;
    negotiated.localRxPeerTx.explicitCsiFeedback = local.explicitCsiFeedback;
    negotiated.localRxPeerTx.explicitNoncompressedFeedback = local.explicitNoncompressedFeedback;
    negotiated.localRxPeerTx.explicitCompressedFeedback = local.explicitCompressedFeedback;
    negotiated.localTxPeerRx.valid = negotiated.localTxPeerRx.supportedChannelWidths.count(operation.operatingChannelWidth) && negotiated.localTxPeerRx.mcsNss.maxMcsPerNss[0] >= 0;
    negotiated.localRxPeerTx.valid = negotiated.localRxPeerTx.supportedChannelWidths.count(operation.operatingChannelWidth) && negotiated.localRxPeerTx.mcsNss.maxMcsPerNss[0] >= 0;
    return negotiated;
}

} // namespace ieee80211
} // namespace inet

#endif
