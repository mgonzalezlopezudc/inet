//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TRANSMISSIONSPECTRUM_H
#define __INET_TRANSMISSIONSPECTRUM_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmissionSpectrum.h"

namespace inet {
namespace physicallayer {

/** Immutable, validated implementation of ITransmissionSpectrum. */
class INET_API TransmissionSpectrum : public ITransmissionSpectrum
{
  protected:
    const std::vector<PowerSpectralBand> bands;

    static void validate(const std::vector<PowerSpectralBand>& bands);

  public:
    explicit TransmissionSpectrum(const std::vector<PowerSpectralBand>& bands, Hz nominalBandwidth = Hz(NaN));

    virtual const std::vector<PowerSpectralBand>& getBands() const override { return bands; }
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
