//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211MUTUALINFORMATIONMAPPING_H
#define __INET_IEEE80211MUTUALINFORMATIONMAPPING_H

#include <vector>

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

class ApskModulationBase;

/**
 * Deterministic mutual-information mappings for HT subcarrier modulation.
 *
 * The symbol metric is the normalized symbol-constrained mutual information
 * used by MIESM/RBIR.  The bit metric is the average Gray-labelled bit-channel
 * mutual information used by MMIB.  These mappings are implementation
 * policies, not IEEE 802.11 normative procedures.  The modulation and
 * labeling authority remains the IEEE 802.11 HT data-mode object (Clauses
 * 19.3.5, 19.3.11.9.1 and 17.3.5.8 of IEEE Std 802.11-2024).
 *
 * The mapping integral uses deterministic Gauss-Hermite quadrature and the
 * effective-SNR inversion uses a bracket bounded by the input carrier SNRs.
 * All SNR arguments are linear Es/N0 values and beta is a dimensionless
 * linear-domain scale factor.  Patidar et al., DOI
 * 10.1145/3067665.3067671, Section 3.1 Eq. 4 and Appendix B Eq. 11, and the
 * IEEE 802.16m evaluation methodology 802.16m-07/037r2, Sections 4.3.1-4.3.2
 * provide the mapping-family background; they do not make these mappings
 * normative for 802.11.
 */
class INET_API Ieee80211MutualInformationMapping
{
  public:
    /** Returns normalized symbol mutual information in [0,1]. */
    static double computeSymbolMutualInformation(const ApskModulationBase *modulation, double snr);

    /** Returns normalized Gray-labelled bit mutual information in [0,1]. */
    static double computeBitMutualInformation(const ApskModulationBase *modulation, double snr);

    /** Returns the beta-scaled symbol-MI effective SNR in linear Es/N0. */
    static double computeSymbolEffectiveSnr(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta);

    /** Returns the beta-scaled bit-MI effective SNR in linear Es/N0. */
    static double computeBitEffectiveSnr(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta);

    static double computeSymbolEffectiveSnrDb(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta);
    static double computeBitEffectiveSnrDb(const std::vector<double>& carrierSnr, const ApskModulationBase *modulation, double beta);
};

} // namespace physicallayer
} // namespace inet

#endif
