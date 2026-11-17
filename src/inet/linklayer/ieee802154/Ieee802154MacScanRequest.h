// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACSCANREQUEST_H
#define __INET_IEEE802154MACSCANREQUEST_H

#include "inet/linklayer/ieee802154/Ieee802154MacStartRequest.h"
#include "inet/linklayer/ieee802154/Ieee802154BeaconCodec.h"

namespace inet {

enum class Ieee802154ScanType { ED, ACTIVE, PASSIVE, ORPHAN };

struct Ieee802154MacScanRequest
{
    Ieee802154ScanType type = Ieee802154ScanType::PASSIVE;
    std::vector<physicallayer::Ieee802154PhyChannel> channels;
    uint32_t duration = 0; ///< Symbol periods, not the pre-2024 exponent encoding.
};

/** Selected legacy unsecured fields of Table 8-3. No enhanced IE/ranging claim. */
struct Ieee802154PanDescriptor
{
    uint16_t coordinatorPanId;
    Ieee802154Address coordinatorAddress;
    physicallayer::Ieee802154PhyChannel channel;
    Ieee802154BeaconCodec::Content beacon;
    uint8_t lqi;
    uint32_t timestamp; ///< Raw 24-bit symbol timestamp (6.5.3).
    Ieee802154MacServiceResult securityStatus = Ieee802154MacStatus::SUCCESS;
    uint8_t sequenceNumber = 0;
    bool contentAvailable = true;
    std::vector<uint8_t> protectedBody; ///< Opaque failed-unsecuring input; never plaintext beacon content.
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
