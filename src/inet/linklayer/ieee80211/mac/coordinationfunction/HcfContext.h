//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFCONTEXT_H
#define __INET_HCFCONTEXT_H

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <memory>
#include <set>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211HeOmi.h"
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HtCapabilities.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211VhtCapabilities.h"

namespace inet {
namespace ieee80211 {

using namespace units::values;

/** Fixed semantic exchange classes, ordered by getHcfExchangeClassOrder(). */
enum class HcfExchangeClass {
    FORCED_SINGLE_USER,
    HE_UL_TRIGGER,
    HE_SOUNDING,
    RECOVERY_SINGLE_USER,
    HE_DL_MULTIUSER,
    VHT_GROUP_MANAGEMENT,
    VHT_DL_MULTIUSER,
    SINGLE_USER,
    CHANNEL_RELEASE,
};

constexpr size_t NUM_HCF_EXCHANGE_CLASSES = 9;

/** Opaque identity of a packet; this value never conveys Packet ownership. */
class INET_API HcfPacketIdentity
{
  private:
    int64_t value = -1;

  public:
    HcfPacketIdentity() = default;
    explicit HcfPacketIdentity(int64_t value) : value(value) {}

    bool isValid() const { return value >= 0; }
    int64_t getValue() const { return value; }
    bool operator==(const HcfPacketIdentity& other) const { return value == other.value; }
    bool operator!=(const HcfPacketIdentity& other) const { return !(*this == other); }
    bool operator<(const HcfPacketIdentity& other) const { return value < other.value; }
};

/** Opaque identity of the queue state from which a packet candidate was read. */
class INET_API HcfQueueToken
{
  private:
    uint64_t value = 0;

  public:
    HcfQueueToken() = default;
    explicit HcfQueueToken(uint64_t value) : value(value) {}

    bool isValid() const { return value != 0; }
    uint64_t getValue() const { return value; }
    bool operator==(const HcfQueueToken& other) const { return value == other.value; }
};

enum class HcfStationRole {
    STATION,
    ACCESS_POINT,
};

/** Immutable local state observed at an EDCAF grant boundary. */
class INET_API HcfLocalContext
{
  private:
    MacAddress localAddress;
    HcfStationRole role = HcfStationRole::STATION;
    MacAddress bssid;
    std::string modeSetIdentity;
    simtime_t currentTime = SIMTIME_ZERO;
    std::optional<AccessCategory> winningAccessCategory;
    simtime_t txopStartTime = SIMTIME_ZERO;
    simtime_t txopLimit = SIMTIME_ZERO;

  public:
    HcfLocalContext() = default;
    HcfLocalContext(const MacAddress& localAddress, HcfStationRole role,
            const MacAddress& bssid, const std::string& modeSetIdentity,
            simtime_t currentTime, AccessCategory winningAccessCategory,
            simtime_t txopStartTime, simtime_t txopLimit) :
        localAddress(localAddress), role(role), bssid(bssid),
        modeSetIdentity(modeSetIdentity), currentTime(currentTime),
        winningAccessCategory(winningAccessCategory),
        txopStartTime(txopStartTime), txopLimit(txopLimit) {}

    const MacAddress& getLocalAddress() const { return localAddress; }
    HcfStationRole getRole() const { return role; }
    const MacAddress& getBssid() const { return bssid; }
    const std::string& getModeSetIdentity() const { return modeSetIdentity; }
    simtime_t getCurrentTime() const { return currentTime; }
    std::optional<AccessCategory> getWinningAccessCategory() const { return winningAccessCategory; }
    simtime_t getTxopStartTime() const { return txopStartTime; }
    simtime_t getTxopLimit() const { return txopLimit; }

    bool isComplete() const
    {
        return !localAddress.isUnspecified() && !bssid.isUnspecified() &&
                !modeSetIdentity.empty() && winningAccessCategory.has_value() &&
                *winningAccessCategory >= AC_BK && *winningAccessCategory < AC_NUMCATEGORIES &&
                currentTime >= SIMTIME_ZERO && txopStartTime >= SIMTIME_ZERO &&
                txopStartTime <= currentTime && txopLimit >= SIMTIME_ZERO;
    }
};

/** Immutable negotiated and generation-qualified state for one peer. */
class INET_API HcfPeerSnapshot
{
  private:
    MacAddress address;
    uint64_t associationEpoch = 0;
    uint16_t associationId = 0;
    std::optional<Ieee80211NegotiatedHtCapabilities> htCapabilities;
    std::optional<Ieee80211NegotiatedVhtCapabilities> vhtCapabilities;
    std::optional<Ieee80211NegotiatedHeCapabilities> heCapabilities;
    bool twtEligible = true;
    std::optional<Ieee80211HeOperatingMode> operatingMode;
    uint64_t capabilityGeneration = 0;
    uint64_t operatingModeGeneration = 0;
    uint64_t csiGeneration = 0;

  public:
    HcfPeerSnapshot() = default;
    HcfPeerSnapshot(const MacAddress& address, uint64_t associationEpoch,
            uint16_t associationId,
            const std::optional<Ieee80211NegotiatedHtCapabilities>& htCapabilities,
            const std::optional<Ieee80211NegotiatedVhtCapabilities>& vhtCapabilities,
            const std::optional<Ieee80211NegotiatedHeCapabilities>& heCapabilities,
            bool twtEligible,
            const std::optional<Ieee80211HeOperatingMode>& operatingMode,
            uint64_t capabilityGeneration, uint64_t operatingModeGeneration,
            uint64_t csiGeneration) :
        address(address), associationEpoch(associationEpoch), associationId(associationId),
        htCapabilities(htCapabilities), vhtCapabilities(vhtCapabilities),
        heCapabilities(heCapabilities), twtEligible(twtEligible),
        operatingMode(operatingMode), capabilityGeneration(capabilityGeneration),
        operatingModeGeneration(operatingModeGeneration), csiGeneration(csiGeneration) {}

    const MacAddress& getAddress() const { return address; }
    uint64_t getAssociationEpoch() const { return associationEpoch; }
    uint16_t getAssociationId() const { return associationId; }
    const std::optional<Ieee80211NegotiatedHtCapabilities>& getHtCapabilities() const { return htCapabilities; }
    const std::optional<Ieee80211NegotiatedVhtCapabilities>& getVhtCapabilities() const { return vhtCapabilities; }
    const std::optional<Ieee80211NegotiatedHeCapabilities>& getHeCapabilities() const { return heCapabilities; }
    bool isTwtEligible() const { return twtEligible; }
    const std::optional<Ieee80211HeOperatingMode>& getOperatingMode() const { return operatingMode; }
    uint64_t getCapabilityGeneration() const { return capabilityGeneration; }
    uint64_t getOperatingModeGeneration() const { return operatingModeGeneration; }
    uint64_t getCsiGeneration() const { return csiGeneration; }

    bool isComplete() const
    {
        return !address.isUnspecified() && associationEpoch != 0 &&
                associationId != 0 && associationId <= 2007 && capabilityGeneration != 0;
    }
};

/** Immutable view of one queue candidate, preserving the source queue order. */
class INET_API HcfQueueSnapshot
{
  private:
    HcfPacketIdentity packetIdentity;
    HcfQueueToken sourceQueueToken;
    AccessCategory accessCategory = AC_BE;
    int trafficIdentifier = -1;
    simtime_t enqueueTime = SIMTIME_ZERO;
    bool retry = false;
    bool blockAckEligible = false;
    B byteLength = B(-1);
    uint64_t associationEpoch = 0;

  public:
    HcfQueueSnapshot() = default;
    HcfQueueSnapshot(HcfPacketIdentity packetIdentity, HcfQueueToken sourceQueueToken,
            AccessCategory accessCategory, int trafficIdentifier,
            simtime_t enqueueTime, bool retry, bool blockAckEligible,
            B byteLength, uint64_t associationEpoch) :
        packetIdentity(packetIdentity), sourceQueueToken(sourceQueueToken),
        accessCategory(accessCategory), trafficIdentifier(trafficIdentifier),
        enqueueTime(enqueueTime), retry(retry), blockAckEligible(blockAckEligible),
        byteLength(byteLength), associationEpoch(associationEpoch) {}

    HcfPacketIdentity getPacketIdentity() const { return packetIdentity; }
    HcfQueueToken getSourceQueueToken() const { return sourceQueueToken; }
    AccessCategory getAccessCategory() const { return accessCategory; }
    int getTrafficIdentifier() const { return trafficIdentifier; }
    simtime_t getEnqueueTime() const { return enqueueTime; }
    bool isRetry() const { return retry; }
    bool isBlockAckEligible() const { return blockAckEligible; }
    B getByteLength() const { return byteLength; }
    uint64_t getAssociationEpoch() const { return associationEpoch; }

    bool isComplete() const
    {
        return packetIdentity.isValid() && sourceQueueToken.isValid() &&
                accessCategory >= AC_BK && accessCategory < AC_NUMCATEGORIES &&
                trafficIdentifier >= 0 && trafficIdentifier <= 15 &&
                enqueueTime >= SIMTIME_ZERO && byteLength > B(0);
    }
};

/** Value-only link estimate; the generation identifies the external authority. */
class INET_API HcfLinkEstimate
{
  private:
    MacAddress peer;
    double receivedPowerDbm = NaN;
    double snirDb = NaN;
    uint64_t generation = 0;

  public:
    HcfLinkEstimate() = default;
    HcfLinkEstimate(const MacAddress& peer, double receivedPowerDbm,
            double snirDb, uint64_t generation) :
        peer(peer), receivedPowerDbm(receivedPowerDbm), snirDb(snirDb),
        generation(generation) {}

    const MacAddress& getPeer() const { return peer; }
    double getReceivedPowerDbm() const { return receivedPowerDbm; }
    double getSnirDb() const { return snirDb; }
    uint64_t getGeneration() const { return generation; }
    bool isComplete() const { return !peer.isUnspecified() && std::isfinite(receivedPowerDbm) && std::isfinite(snirDb) && generation != 0; }
};

/** Immutable PHY observations; the PHY remains the legality and timing authority. */
class INET_API HcfPhySnapshot
{
  private:
    Hz centerFrequency = Hz(NaN);
    Hz channelWidth = Hz(NaN);
    int numberOfTransmitAntennas = 0;
    int numberOfReceiveAntennas = 0;
    W transmitPower = W(NaN);
    W receiverSensitivity = W(NaN);
    simtime_t guardInterval = SIMTIME_ZERO;
    int numberOfLtfSymbols = 0;
    std::vector<bool> puncturedSubchannels;
    std::vector<HcfLinkEstimate> linkEstimates;

  public:
    HcfPhySnapshot() = default;
    HcfPhySnapshot(Hz centerFrequency, Hz channelWidth,
            int numberOfTransmitAntennas, int numberOfReceiveAntennas,
            W transmitPower, W receiverSensitivity, simtime_t guardInterval,
            int numberOfLtfSymbols, const std::vector<bool>& puncturedSubchannels,
            const std::vector<HcfLinkEstimate>& linkEstimates) :
        centerFrequency(centerFrequency), channelWidth(channelWidth),
        numberOfTransmitAntennas(numberOfTransmitAntennas),
        numberOfReceiveAntennas(numberOfReceiveAntennas),
        transmitPower(transmitPower), receiverSensitivity(receiverSensitivity),
        guardInterval(guardInterval), numberOfLtfSymbols(numberOfLtfSymbols),
        puncturedSubchannels(puncturedSubchannels), linkEstimates(linkEstimates) {}

    Hz getCenterFrequency() const { return centerFrequency; }
    Hz getChannelWidth() const { return channelWidth; }
    int getNumberOfTransmitAntennas() const { return numberOfTransmitAntennas; }
    int getNumberOfReceiveAntennas() const { return numberOfReceiveAntennas; }
    W getTransmitPower() const { return transmitPower; }
    W getReceiverSensitivity() const { return receiverSensitivity; }
    simtime_t getGuardInterval() const { return guardInterval; }
    int getNumberOfLtfSymbols() const { return numberOfLtfSymbols; }
    const std::vector<bool>& getPuncturedSubchannels() const { return puncturedSubchannels; }
    const std::vector<HcfLinkEstimate>& getLinkEstimates() const { return linkEstimates; }

    bool isComplete() const
    {
        if (!std::isfinite(centerFrequency.get()) || centerFrequency <= Hz(0) ||
                !std::isfinite(channelWidth.get()) || channelWidth <= Hz(0) ||
                numberOfTransmitAntennas <= 0 || numberOfReceiveAntennas <= 0 ||
                !std::isfinite(transmitPower.get()) || transmitPower < W(0) ||
                !std::isfinite(receiverSensitivity.get()) || receiverSensitivity < W(0) ||
                guardInterval <= SIMTIME_ZERO || numberOfLtfSymbols <= 0)
            return false;
        for (const auto& estimate : linkEstimates)
            if (!estimate.isComplete())
                return false;
        return true;
    }
};

/** Value-only HE sounding prerequisite projected by the DL provider. */
struct INET_API HcfHeSoundingCandidateSnapshot
{
    MacAddress address;
    uint16_t associationId = 0;
    int maximumSpatialStreams = 1;
    bool eligible = false;
    bool hasFreshCsi = false;
};

struct INET_API HcfHeSoundingSnapshot
{
    AccessCategory accessCategory = AC_BE;
    Hz channelCenterFrequency = Hz(NaN);
    Hz channelBandwidth = Hz(NaN);
    std::vector<HcfHeSoundingCandidateSnapshot> candidates;

    bool isComplete() const
    {
        return accessCategory >= AC_BK && accessCategory < AC_NUMCATEGORIES &&
                std::isfinite(channelCenterFrequency.get()) &&
                channelCenterFrequency > Hz(0) &&
                std::isfinite(channelBandwidth.get()) && channelBandwidth > Hz(0);
    }
};

enum class HcfContextRejectionCode {
    NONE,
    INCOMPLETE_LOCAL_CONTEXT,
    INCOMPLETE_PEER_SNAPSHOT,
    INCOMPLETE_QUEUE_SNAPSHOT,
    INCOMPLETE_PHY_SNAPSHOT,
    DUPLICATE_PEER,
    DUPLICATE_PACKET,
    FUTURE_ENQUEUE_TIME,
};

class INET_API HcfContext
{
  private:
    HcfLocalContext localContext;
    std::vector<HcfPeerSnapshot> peerSnapshots;
    std::vector<HcfQueueSnapshot> queueSnapshots;
    HcfPhySnapshot phySnapshot;
    std::optional<AccessCategory> selectionAccessCategory;
    std::array<std::optional<bool>, NUM_HCF_EXCHANGE_CLASSES> exchangeEligibility;
    std::unordered_map<std::type_index, std::shared_ptr<const void>> providerSnapshots;

  public:
    HcfContext() = default;
    HcfContext(const HcfLocalContext& localContext,
            const std::vector<HcfPeerSnapshot>& peerSnapshots,
            const std::vector<HcfQueueSnapshot>& queueSnapshots,
            const HcfPhySnapshot& phySnapshot) :
        localContext(localContext), peerSnapshots(peerSnapshots),
        queueSnapshots(queueSnapshots), phySnapshot(phySnapshot) {}
    explicit HcfContext(const HcfHeSoundingSnapshot& heSoundingSnapshot)
        { setProviderSnapshot(heSoundingSnapshot); }
    HcfContext(AccessCategory accessCategory,
            std::initializer_list<HcfExchangeClass> eligibleExchangeClasses) :
        selectionAccessCategory(accessCategory)
    {
        for (auto exchangeClass : eligibleExchangeClasses)
            exchangeEligibility[static_cast<size_t>(exchangeClass)] = true;
    }

    const HcfLocalContext& getLocalContext() const { return localContext; }
    const std::vector<HcfPeerSnapshot>& getPeerSnapshots() const { return peerSnapshots; }
    const std::vector<HcfQueueSnapshot>& getQueueSnapshots() const { return queueSnapshots; }
    const HcfPhySnapshot& getPhySnapshot() const { return phySnapshot; }
    std::optional<AccessCategory> getSelectionAccessCategory() const
        { return selectionAccessCategory; }
    std::optional<bool> getExchangeEligibility(HcfExchangeClass exchangeClass) const
        { return exchangeEligibility[static_cast<size_t>(exchangeClass)]; }
    void setExchangeEligibility(HcfExchangeClass exchangeClass, bool eligible = true)
        { exchangeEligibility[static_cast<size_t>(exchangeClass)] = eligible; }
    template<typename T>
    void setProviderSnapshot(T snapshot)
    {
        providerSnapshots[typeid(T)] = std::make_shared<const T>(std::move(snapshot));
    }
    template<typename T>
    const T *findProviderSnapshot() const
    {
        auto it = providerSnapshots.find(typeid(T));
        return it == providerSnapshots.end() ? nullptr :
                static_cast<const T *>(it->second.get());
    }

    HcfContextRejectionCode validate() const
    {
        if (!localContext.isComplete())
            return HcfContextRejectionCode::INCOMPLETE_LOCAL_CONTEXT;
        if (!phySnapshot.isComplete())
            return HcfContextRejectionCode::INCOMPLETE_PHY_SNAPSHOT;
        std::set<MacAddress> peers;
        for (const auto& peer : peerSnapshots) {
            if (!peer.isComplete())
                return HcfContextRejectionCode::INCOMPLETE_PEER_SNAPSHOT;
            if (!peers.insert(peer.getAddress()).second)
                return HcfContextRejectionCode::DUPLICATE_PEER;
        }
        std::set<HcfPacketIdentity> packets;
        for (const auto& queue : queueSnapshots) {
            if (!queue.isComplete())
                return HcfContextRejectionCode::INCOMPLETE_QUEUE_SNAPSHOT;
            if (queue.getEnqueueTime() > localContext.getCurrentTime())
                return HcfContextRejectionCode::FUTURE_ENQUEUE_TIME;
            if (!packets.insert(queue.getPacketIdentity()).second)
                return HcfContextRejectionCode::DUPLICATE_PACKET;
        }
        return HcfContextRejectionCode::NONE;
    }
};

} // namespace ieee80211
} // namespace inet

#endif
