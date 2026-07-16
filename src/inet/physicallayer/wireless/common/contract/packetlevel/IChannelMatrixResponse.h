//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_ICHANNELMATRIXRESPONSE_H
#define __INET_ICHANNELMATRIXRESPONSE_H

#include <complex>
#include <vector>

#include "inet/common/Units.h"

namespace inet {
namespace physicallayer {

using namespace inet::units::values;

/**
 * Dense dimensionless complex channel matrix. Rows identify receive antennas
 * and columns identify transmit antennas.
 */
class INET_API ChannelMatrix
{
  protected:
    int numReceiveAntennas;
    int numTransmitAntennas;
    std::vector<std::complex<double>> coefficients;

  public:
    ChannelMatrix(int numReceiveAntennas, int numTransmitAntennas,
            std::vector<std::complex<double>> coefficients);

    int getNumReceiveAntennas() const { return numReceiveAntennas; }
    int getNumTransmitAntennas() const { return numTransmitAntennas; }
    const std::vector<std::complex<double>>& getCoefficients() const { return coefficients; }
    const std::complex<double>& getCoefficient(int receiveAntenna, int transmitAntenna) const;

    ChannelMatrix transposed() const;
};

/**
 * Immutable matrix response over absolute receiver simulation time and radio
 * frequency. For a reciprocal over-the-air link, reversing the endpoints uses
 * the ordinary transpose of this response, not its conjugate transpose.
 */
class INET_API IChannelMatrixResponse
{
  public:
    virtual ~IChannelMatrixResponse() {}

    virtual int getNumReceiveAntennas() const = 0;
    virtual int getNumTransmitAntennas() const = 0;
    virtual ChannelMatrix getChannelMatrix(simsec time, Hz frequency) const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
