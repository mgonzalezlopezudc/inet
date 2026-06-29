//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211DSSSOFDMMODE_H
#define __INET_IEEE80211DSSSOFDMMODE_H

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"

namespace inet {

namespace physicallayer {

/**
 * Represents a Direct Sequence Spread Spectrum with Orthogonal Frequency Division
 * Multiplexing PHY mode.
 *
 * IEEE Std 802.11-2024 Clause 18 defines ERP operation in the 2.4 GHz band,
 * including Clause 17 OFDM-based ERP-OFDM PPDU operation. This class is only a
 * placeholder: it returns null header/preamble modes and zero timings, so it is
 * not a standard-conformant transmit path.
 */
class INET_API Ieee80211DsssOfdmMode : public Ieee80211ModeBase
{
  protected:
    const Ieee80211DsssPreambleMode *dsssPreambleMode;
    const Ieee80211DsssHeaderMode *dsssHeaderMode;
    const Ieee80211OfdmPreambleMode *ofdmPreambleMode;
    const Ieee80211OfdmSignalMode *ofdmSignalMode;
    const Ieee80211OfdmDataMode *ofdmDataMode;

  protected:
    virtual int getLegacyCwMin() const override { return -1; }
    virtual int getLegacyCwMax() const override { return -1; }

  public:
    Ieee80211DsssOfdmMode(const char *name, const Ieee80211DsssPreambleMode *dsssPreambleMode, const Ieee80211DsssHeaderMode *dsssHeaderMode, const Ieee80211OfdmPreambleMode *ofdmPreambleMode, const Ieee80211OfdmSignalMode *ofdmSignalMode, const Ieee80211OfdmDataMode *ofdmDataMode);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override { return stream << "Ieee80211DsssOfdmMode"; }

    virtual const IIeee80211PreambleMode *getPreambleMode() const override { return nullptr; }
    virtual const IIeee80211HeaderMode *getHeaderMode() const override { return nullptr; }
    virtual const IIeee80211DataMode *getDataMode() const override { return ofdmDataMode; }

    // Not implemented: Clause 18 ERP/DSSS-OFDM duration would have to combine
    // DSSS-compatible PLCP portions with OFDM DATA timing and signal extension.
    virtual const simtime_t getDuration(b dataLength) const override { return 0; }

    // Not implemented: use Ieee80211DsssMode, Ieee80211HrDsssMode, or
    // Ieee80211ErpOfdmMode for the standard-backed non-HE PHY timings.
    virtual const simtime_t getSlotTime() const override { return 0; }
    virtual const simtime_t getSifsTime() const override { return 0; }
    virtual const simtime_t getRifsTime() const override;
    virtual const simtime_t getCcaTime() const override { return 0; }
    virtual const simtime_t getPhyRxStartDelay() const override { return 0; }
    virtual const simtime_t getRxTxTurnaroundTime() const override { return 0; }
    virtual const simtime_t getPreambleLength() const override { return 0; }
    virtual const simtime_t getPlcpHeaderLength() const override { return 0; }
    virtual int getMpduMaxLength() const override { return -1; }
};

} // namespace physicallayer

} // namespace inet

#endif
