//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEE80211HELINKPHYCONTEXT_H
#define __INET_IIEE80211HELINKPHYCONTEXT_H

#include <cmath>
#include <vector>

#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/contract/Ieee80211HePreamblePuncturing.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

/**
 * Immutable-by-convention projection of the active radio and resolved HE PHY
 * policy. Consumers receive this object by value and must not rediscover the
 * radio module tree or retain pointers into radio/MIB state.
 */
class INET_API Ieee80211HeLinkPhySnapshot
{
  private:
    const int channelNumber;
    const Hz channelCenterFrequency;
    const Hz channelBandwidth;
    const W effectiveTransmitPower;
    const W maximumTransmitPower;
    const W receiveSensitivity;
    const double noiseFigureDb;
    const int antennaCount;
    const physicallayer::Ieee80211HeGuardInterval guardInterval;
    const physicallayer::Ieee80211HeLtfType ltfType;
    const int packetExtensionDurationUs;
    const std::vector<bool> puncturedSubchannels;
    const uint8_t puncturedSubchannelMask;
    const Ieee80211HeCapabilities localHeCapabilities;

  public:
    Ieee80211HeLinkPhySnapshot(int channelNumber, Hz channelCenterFrequency,
            Hz channelBandwidth, W effectiveTransmitPower, W maximumTransmitPower,
            W receiveSensitivity, double noiseFigureDb, int antennaCount,
            physicallayer::Ieee80211HeGuardInterval guardInterval,
            physicallayer::Ieee80211HeLtfType ltfType, int packetExtensionDurationUs,
            const std::vector<bool>& puncturedSubchannels, uint8_t puncturedSubchannelMask,
            const Ieee80211HeCapabilities& localHeCapabilities);

    int getChannelNumber() const { return channelNumber; }
    Hz getChannelCenterFrequency() const { return channelCenterFrequency; }
    Hz getChannelBandwidth() const { return channelBandwidth; }
    W getEffectiveTransmitPower() const { return effectiveTransmitPower; }
    W getMaximumTransmitPower() const { return maximumTransmitPower; }
    W getReceiveSensitivity() const { return receiveSensitivity; }
    double getNoiseFigureDb() const { return noiseFigureDb; }
    int getAntennaCount() const { return antennaCount; }
    physicallayer::Ieee80211HeGuardInterval getGuardInterval() const { return guardInterval; }
    physicallayer::Ieee80211HeLtfType getLtfType() const { return ltfType; }
    int getPacketExtensionDurationUs() const { return packetExtensionDurationUs; }
    const std::vector<bool>& getPuncturedSubchannels() const { return puncturedSubchannels; }
    uint8_t getPuncturedSubchannelMask() const { return puncturedSubchannelMask; }
    const Ieee80211HeCapabilities& getLocalHeCapabilities() const { return localHeCapabilities; }
};

/** A copied peer capability/link projection captured at the same planning instant. */
class INET_API Ieee80211HePeerLinkSnapshot
{
  private:
    const bool hasAdvertisement;
    const Ieee80211HeCapabilities advertisement;
    const bool hasNegotiatedCapabilities;
    const Ieee80211NegotiatedHeCapabilities negotiatedCapabilities;
    const double pathLossDb;
    const bool hasFreshPathLoss;

  public:
    Ieee80211HePeerLinkSnapshot(bool hasAdvertisement,
            const Ieee80211HeCapabilities& advertisement,
            bool hasNegotiatedCapabilities,
            const Ieee80211NegotiatedHeCapabilities& negotiatedCapabilities,
            double pathLossDb, bool hasFreshPathLoss);

    bool getHasAdvertisement() const { return hasAdvertisement; }
    const Ieee80211HeCapabilities& getAdvertisement() const { return advertisement; }
    bool getHasNegotiatedCapabilities() const { return hasNegotiatedCapabilities; }
    const Ieee80211NegotiatedHeCapabilities& getNegotiatedCapabilities() const { return negotiatedCapabilities; }
    double getPathLossDb() const { return pathLossDb; }
    bool getHasFreshPathLoss() const { return hasFreshPathLoss; }
};

/**
 * Narrow, read-only boundary between HE MAC planning and packet-level radios.
 * One adapter owns all concrete radio discovery for both scalar and
 * dimensional IEEE 802.11 radio configurations.
 */
class INET_API IIeee80211HeLinkPhyContext
{
  public:
    virtual ~IIeee80211HeLinkPhyContext() {}

    virtual Ieee80211HeLinkPhySnapshot getSnapshot() const = 0;
    virtual Ieee80211HePeerLinkSnapshot getPeerSnapshot(const MacAddress& address,
            simtime_t maximumLinkEstimateAge) const = 0;
};

inline Ieee80211HeLinkPhySnapshot::Ieee80211HeLinkPhySnapshot(int channelNumber,
        Hz channelCenterFrequency, Hz channelBandwidth, W effectiveTransmitPower,
        W maximumTransmitPower, W receiveSensitivity, double noiseFigureDb,
        int antennaCount, physicallayer::Ieee80211HeGuardInterval guardInterval,
        physicallayer::Ieee80211HeLtfType ltfType, int packetExtensionDurationUs,
        const std::vector<bool>& puncturedSubchannels, uint8_t puncturedSubchannelMask,
        const Ieee80211HeCapabilities& localHeCapabilities) :
    channelNumber(channelNumber),
    channelCenterFrequency(channelCenterFrequency),
    channelBandwidth(channelBandwidth),
    effectiveTransmitPower(effectiveTransmitPower),
    maximumTransmitPower(maximumTransmitPower),
    receiveSensitivity(receiveSensitivity),
    noiseFigureDb(noiseFigureDb),
    antennaCount(antennaCount),
    guardInterval(guardInterval),
    ltfType(ltfType),
    packetExtensionDurationUs(packetExtensionDurationUs),
    puncturedSubchannels(puncturedSubchannels),
    puncturedSubchannelMask(puncturedSubchannelMask),
    localHeCapabilities(localHeCapabilities)
{
    if (channelNumber < 0 || !std::isfinite(channelCenterFrequency.get()) || channelCenterFrequency <= Hz(0) ||
            !std::isfinite(channelBandwidth.get()) || channelBandwidth <= Hz(0))
        throw cRuntimeError("Invalid HE link/PHY channel snapshot");
    if (channelBandwidth != MHz(20) && channelBandwidth != MHz(40) &&
            channelBandwidth != MHz(80) && channelBandwidth != MHz(160))
        throw cRuntimeError("Unsupported HE link/PHY channel bandwidth");
    if (!std::isfinite(effectiveTransmitPower.get()) || effectiveTransmitPower <= W(0) ||
            !std::isfinite(maximumTransmitPower.get()) || maximumTransmitPower <= W(0) ||
            effectiveTransmitPower > maximumTransmitPower ||
            !std::isfinite(receiveSensitivity.get()) || receiveSensitivity <= W(0))
        throw cRuntimeError("Invalid HE link/PHY power snapshot");
    if (!std::isfinite(noiseFigureDb) || antennaCount <= 0)
        throw cRuntimeError("Invalid HE link/PHY receiver snapshot");
    if (guardInterval != physicallayer::HE_GI_0_8_US &&
            guardInterval != physicallayer::HE_GI_1_6_US &&
            guardInterval != physicallayer::HE_GI_3_2_US)
        throw cRuntimeError("Invalid HE guard interval in link/PHY snapshot");
    if (ltfType != physicallayer::HE_LTF_1X && ltfType != physicallayer::HE_LTF_2X &&
            ltfType != physicallayer::HE_LTF_4X)
        throw cRuntimeError("Invalid HE-LTF type in link/PHY snapshot");
    (void)physicallayer::getHeLtfSymbolDuration(ltfType, guardInterval);
    if (packetExtensionDurationUs != 0 && packetExtensionDurationUs != 4 &&
            packetExtensionDurationUs != 8 && packetExtensionDurationUs != 12 &&
            packetExtensionDurationUs != 16)
        throw cRuntimeError("Invalid HE packet extension duration in link/PHY snapshot");
    uint8_t derivedMask = 0;
    if (!puncturedSubchannels.empty() &&
            puncturedSubchannels.size() != (size_t)std::lround(channelBandwidth.get() / 20e6))
        throw cRuntimeError("HE link/PHY puncturing size does not match channel bandwidth");
    for (size_t i = 0; i < puncturedSubchannels.size(); ++i) {
        if (i >= 8)
            throw cRuntimeError("HE link/PHY puncturing snapshot exceeds eight 20 MHz subchannels");
        if (puncturedSubchannels[i])
            derivedMask |= 1U << i;
    }
    if (derivedMask != puncturedSubchannelMask)
        throw cRuntimeError("Inconsistent HE link/PHY puncturing snapshot");
    if (!isValidHePreamblePuncturing(puncturedSubchannels,
            std::lround(channelBandwidth.get() / 1e6)))
        throw cRuntimeError("Illegal HE preamble puncturing pattern in link/PHY snapshot");
}

inline Ieee80211HePeerLinkSnapshot::Ieee80211HePeerLinkSnapshot(bool hasAdvertisement,
        const Ieee80211HeCapabilities& advertisement, bool hasNegotiatedCapabilities,
        const Ieee80211NegotiatedHeCapabilities& negotiatedCapabilities,
        double pathLossDb, bool hasFreshPathLoss) :
    hasAdvertisement(hasAdvertisement),
    advertisement(advertisement),
    hasNegotiatedCapabilities(hasNegotiatedCapabilities),
    negotiatedCapabilities(negotiatedCapabilities),
    pathLossDb(pathLossDb),
    hasFreshPathLoss(hasFreshPathLoss)
{
    if (hasFreshPathLoss && !std::isfinite(pathLossDb))
        throw cRuntimeError("Fresh HE peer link snapshot has invalid path loss");
}

} // namespace ieee80211
} // namespace inet

#endif // __INET_IIEE80211HELINKPHYCONTEXT_H
