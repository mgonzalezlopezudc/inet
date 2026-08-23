//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_ITRANSMISSIONSPECTRUM_H
#define __INET_ITRANSMISSIONSPECTRUM_H

#include "inet/common/IPrintableObject.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/PhysicalLayerDefs.h"

#include <vector>

namespace inet {
namespace physicallayer {

/** A half-open frequency band and its fraction of the total instantaneous power. */
struct INET_API PowerSpectralBand
{
    Hz lowerFrequencyOffset;
    Hz upperFrequencyOffset;
    double powerFraction;

    PowerSpectralBand(Hz lowerFrequencyOffset, Hz upperFrequencyOffset, double powerFraction) :
        lowerFrequencyOffset(lowerFrequencyOffset),
        upperFrequencyOffset(upperFrequencyOffset),
        powerFraction(powerFraction)
    {
    }
};

/**
 * Immutable description of a transmission's frequency power allocation.
 *
 * Offsets are relative to the transmission center frequency. Bands are
 * interpreted as half-open intervals; adjacent bands may therefore share a
 * boundary without double counting it.
 */
class INET_API ITransmissionSpectrum : public virtual IPrintableObject
{
  public:
    virtual const std::vector<PowerSpectralBand>& getBands() const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
