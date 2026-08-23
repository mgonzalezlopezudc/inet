//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TGNMIMOCHANNEL_H
#define __INET_TGNMIMOCHANNEL_H

#include <array>
#include <complex>
#include <vector>

#include "inet/physicallayer/wireless/common/contract/packetlevel/ComplexMatrix.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h"
#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgnChannelProfile.h"

namespace inet {
namespace physicallayer {

struct INET_API TgnLorentzianProcess
{
    std::vector<double> oscillatorFrequenciesHz;
    std::vector<std::complex<double>> coefficients;

    TgnLorentzianProcess() = default;
    TgnLorentzianProcess(const std::vector<double>& oscillatorFrequenciesHz,
        const std::vector<std::complex<double>>& coefficients);
};

struct INET_API TgnWeightedProcess
{
    double amplitude = 0;
    TgnLorentzianProcess process;
};

struct INET_API TgnTemporalProcess
{
    std::vector<TgnWeightedProcess> terms;
};

struct INET_API TgnComponentRealization
{
    int stableComponentIndex = -1;
    simtime_t excessDelay;
    double normalizedLinearPower = NaN;
    ComplexMatrix receiverSquareRoot;
    ComplexMatrix transmitterSquareRoot;
    std::vector<TgnTemporalProcess> temporalProcesses;
    bool fluorescent = false;
};

struct INET_API TgnChannelRealization
{
    int numReceiveAntennas = 0;
    int numTransmitAntennas = 0;
    Hz referenceFrequency = Hz(NaN);
    bool timeVariation = true;
    bool los = false;
    double firstTapKLinear = 0;
    double firstTapDiffusePower = 0;
    // Optional calibration-only normalization of the expected complete
    // small-scale LOS profile. This is a profile-level factor, not a
    // per-realization renormalization.
    double smallScalePowerNormalization = 1;
    double shadowingPowerGain = NaN;
    std::vector<TgnComponentRealization> components;
    ComplexMatrix fixedLosMatrix;
    bool fluorescent = false;
    double fluorescentScale = 0;
    double fluorescentMainsFrequencyHz = 0;
    std::array<double, 3> fluorescentPhases = {{0, 0, 0}};
};

class INET_API TgnMimoChannel
{
  protected:
    static double frobeniusNorm(const ComplexMatrix& matrix);
    static ComplexMatrix identity(int size);
    static ComplexMatrix add(const ComplexMatrix& left, const ComplexMatrix& right);
    static ComplexMatrix scale(const ComplexMatrix& matrix, std::complex<double> factor);

  public:
    // Composite Simpson panels per smooth half of the circular Laplacian PAS.
    // 8192 panels per smooth half are the smallest tested count whose maximum
    // A-F Rx/Tx correlation change against 16384 panels is below 1e-10 for
    // 1-8 element ULAs at 0.25, 0.5, and 1.0 wavelength spacing.
    static const int SPATIAL_QUADRATURE_PANELS = 8192;

    static ComplexMatrix multiply(const ComplexMatrix& left, const ComplexMatrix& right);
    static ComplexMatrix conjugateTranspose(const ComplexMatrix& matrix);
    static ComplexMatrix createSpatialCorrelation(int antennaCount, double spacingInWavelengths,
        double meanAngleDegrees, double angularSpreadDegrees);
    static ComplexMatrix principalSquareRoot(const ComplexMatrix& matrix);
    static ComplexMatrix createFixedLosMatrix(int numReceiveAntennas, int numTransmitAntennas,
        double receiverSpacingInWavelengths, double transmitterSpacingInWavelengths);

    static std::complex<double> evaluate(const TgnLorentzianProcess& process, simtime_t absoluteTime);
    static std::complex<double> evaluate(const TgnTemporalProcess& process, simtime_t absoluteTime);
    static std::complex<double> evaluateFluorescent(const TgnChannelRealization& realization, simtime_t absoluteTime);
    static ComplexMatrix evaluate(const TgnChannelRealization& realization, simtime_t absoluteTime, Hz frequency);
    static Hz getActualMaximumTemporalFrequency(const TgnChannelRealization& realization);
};

} // namespace physicallayer
} // namespace inet

#endif
