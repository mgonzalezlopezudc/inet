//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEHCF_H
#define __INET_HEHCF_H

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>
#include <ostream>

#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfFeatureSet.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeFrameDecorationPolicy.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfTxRxInterceptor.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlTriggerService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTxopCoordinatorService.h"
#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HeLinkPhyContext.h"
#include "inet/linklayer/ieee80211/mac/contract/IHeDlMuSnapshotSource.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/queueing/contract/IPacketQueue.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

/**
 * Returns the soliciting HE PPDU's decoded TXOP duration, if the incoming
 * packet was received in an HE PPDU.
 */
INET_API std::optional<physicallayer::Ieee80211HeTxopDuration>
getIeee80211HeSolicitingTxopDuration(const Packet *packet);

/**
 * IEEE 802.11-2024 26.11.5 response protection derived from the soliciting
 * frame Duration, SIFS, and the actual response PPDU TXTIME.
 */
INET_API HeTbResponseProtection deriveIeee80211HeTbResponseProtection(
        const std::optional<physicallayer::Ieee80211HeTxopDuration>& solicitingTxopDuration,
        simtime_t triggerDuration, simtime_t sifsTime, simtime_t responseTxTime);

/** Builds the compressed Block Ack bitmap requested by an MU-BAR User Info field. */
INET_API Ptr<Ieee80211CompressedBlockAck> buildHeMuBarCompressedBlockAck(
        const Ieee80211HeTriggerUserInfo& user, RecipientBlockAckAgreement *agreement,
        const MacAddress& receiverAddress, const MacAddress& transmitterAddress);

/** Computes DL path loss from Trigger AP power and total received PPDU power. */
INET_API double computeIeee80211HeTriggerPathLossDb(int apTxPowerDbm20Mhz,
        W receivedPower, Hz receivedBandwidth);

/** Applies HE-TB target-receive-power control and the local maximum-power cap. */
INET_API W computeIeee80211HeTbTransmitPower(W maximumPower, int targetReceivePowerDbm,
        double pathLossDb, bool useMaximumTransmitPower);

/**
 * Extends Hcf to support IEEE 802.11ax Downlink OFDMA multi-user scheduling.
 *
 * When the winning EDCAF's pending queue contains packets for two or more
 * distinct destination STAs and the "ax" modeSet is active, HeHcf replaces
 * the standard HcfFs frame sequence with HeDlMuTxOpFs, which:
 *   1. Calls the DL OFDMA scheduler to obtain per-STA RU assignments.
 *   2. Dequeues one packet per selected STA.
 *   3. Assembles a container packet with explicit HE MU RU payload sections.
 *   4. Passes the container to the existing Tx pipeline where the packet-level
 *      PHY models the PPDU as a single transmission with per-RU reception.
 *
 * When fewer than two unique destination STAs are queued (or the modeSet is
 * not "ax"), HeHcf falls back transparently to the standard Hcf::startFrameSequence().
 */
/** Registered compatibility/configuration facade; Hcf owns HE execution. */
class INET_API HeHcf : public Hcf
{
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HEHCF_H
