//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEEE80211OFDMSUBCARRIERPLAN_H
#define __INET_IIEEE80211OFDMSUBCARRIERPLAN_H

#include "inet/physicallayer/wireless/common/base/packetlevel/PhysicalLayerDefs.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmissionSpectrum.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IModulation.h"

#include <vector>

namespace inet {
namespace physicallayer {

enum class Ieee80211SubcarrierRole {
    DATA,
    PILOT
};

struct INET_API Ieee80211OfdmSubcarrier
{
    int index;
    Ieee80211SubcarrierRole role;
    Hz lowerFrequencyOffset;
    Hz centerFrequencyOffset;
    Hz upperFrequencyOffset;
};

/** Capability exposing the authoritative IEEE 802.11 OFDM carrier geometry. */
class INET_API IIeee80211OfdmSubcarrierPlan
{
  public:
    virtual ~IIeee80211OfdmSubcarrierPlan() {}
    virtual const std::vector<Ieee80211OfdmSubcarrier>& getSubcarriers() const = 0;
    virtual const ITransmissionSpectrum& getEqualPowerSpectrum() const = 0;
    virtual int getNumberOfDataSubcarriers() const = 0;
    virtual int getNumberOfPilotSubcarriers() const = 0;
    virtual int getNumberOfTotalSubcarriers() const = 0;
    virtual Hz getSubcarrierFrequencySpacing() const = 0;
};

/**
 * Capability for the HT data-mode properties needed by an error model.
 *
 * Keeping these properties beside the carrier plan lets consumers detect the
 * contract without depending on Ieee80211HtDataMode's concrete C++ type.
 */
class INET_API IIeee80211HtDataMode : public IIeee80211OfdmSubcarrierPlan
{
  public:
    /** Authoritative data-subcarrier modulation and Gray labeling. */
    virtual const IModulation *getModulation() const = 0;
    virtual unsigned int getMcsIndex() const = 0;
    virtual Hz getBandwidth() const = 0;
    virtual int getNumberOfSpatialStreams() const = 0;
    virtual bool isBcc() const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
