// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACSCANREQUEST_H
#define __INET_IEEE802154MACSCANREQUEST_H

#include "inet/linklayer/ieee802154/Ieee802154MacStartRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154BeaconCodec.h"
#include "inet/linklayer/ieee802154/Ieee802154InformationElements.h"

namespace inet {

enum class Ieee802154ScanType : uint8_t { ED = 0, ACTIVE = 1, PASSIVE = 2, ORPHAN = 3, ENHANCED_ACTIVE_SCAN = 4 };

struct Ieee802154EnhancedBeaconFilter
{
    bool permitJoining = false;
    std::optional<uint8_t> minimumLqi;
    std::optional<uint8_t> responsePercent;
    std::vector<uint8_t> requestedAttributes;
};

struct Ieee802154RequestedNestedIe
{
    bool longForm = false;
    uint8_t id = 0;
};

struct Ieee802154MacScanRequest
{
    Ieee802154ScanType type = Ieee802154ScanType::PASSIVE;
    std::vector<physicallayer::Ieee802154PhyChannel> channels;
    uint32_t duration = 0; ///< Symbol periods, not the pre-2024 exponent encoding.
    bool sequenceSuppressed = false;
    bool panIdSuppressed = false;
    Ieee802154Address destinationAddress = Ieee802154Address::SHORT_BROADCAST_ADDRESS;
    uint16_t destinationPanId = 0xffff;
    std::vector<Ieee802154InformationElements::Header> headerIes;
    std::vector<Ieee802154InformationElements::Payload> payloadIes;
    std::vector<uint8_t> requestedHeaderIds;
    std::vector<Ieee802154RequestedNestedIe> requestedNestedIds;
    std::optional<Ieee802154EnhancedBeaconFilter> filter;
    bool linkQualityScan = false;
};

/** Selected legacy unsecured fields of Table 8-3. No enhanced IE/ranging claim. */
struct Ieee802154PanDescriptor
{
    std::optional<uint16_t> coordinatorPanId;
    Ieee802154Address coordinatorAddress;
    physicallayer::Ieee802154PhyChannel channel;
    Ieee802154BeaconCodec::Content beacon;
    uint8_t lqi;
    uint32_t timestamp; ///< Raw 24-bit symbol timestamp (6.5.3).
    Ieee802154MacServiceResult securityStatus = Ieee802154MacStatus::SUCCESS;
    std::optional<uint8_t> sequenceNumber;
    bool contentAvailable = true;
    std::vector<uint8_t> protectedBody; ///< Opaque failed-unsecuring input; never plaintext beacon content.
    uint8_t frameVersion = 0;
    std::optional<uint16_t> sourcePanId; ///< Actual wire field, distinct from effective context.
    std::vector<Ieee802154InformationElements::Header> headerIes;
    std::vector<Ieee802154InformationElements::Payload> payloadIes;
    std::vector<uint8_t> enhancedPayload;
};

struct Ieee802154MacScanConfirm
{
    Ieee802154ScanType type;
    std::vector<physicallayer::Ieee802154PhyChannel> unscannedChannels;
    std::vector<uint8_t> energyDetectList;
    std::vector<Ieee802154PanDescriptor> panDescriptors;
    Ieee802154MacServiceResult result = Ieee802154MacStatus::SUCCESS;
};

} // namespace inet
#endif
