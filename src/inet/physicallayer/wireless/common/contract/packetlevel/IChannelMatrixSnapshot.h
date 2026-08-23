//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_ICHANNELMATRIXSNAPSHOT_H
#define __INET_ICHANNELMATRIXSNAPSHOT_H

#include <complex>
#include <memory>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/IPrintableObject.h"
#include "inet/common/Units.h"

namespace inet {
namespace physicallayer {

using namespace inet::units::values;

class INET_API ComplexMatrix
{
  protected:
    int numRows = 0;
    int numColumns = 0;
    std::vector<std::complex<double>> coefficients;

  public:
    ComplexMatrix() = default;
    ComplexMatrix(int numRows, int numColumns);
    ComplexMatrix(int numRows, int numColumns, const std::vector<std::complex<double>>& coefficients);

    int getNumRows() const { return numRows; }
    int getNumColumns() const { return numColumns; }
    const std::vector<std::complex<double>>& getCoefficients() const { return coefficients; }
    const std::complex<double>& get(int row, int column) const;
    std::complex<double>& get(int row, int column);
    bool isFinite() const;
    ComplexMatrix transpose() const;
};

class INET_API IChannelMatrixSnapshot : public virtual IPrintableObject
{
  public:
    virtual ~IChannelMatrixSnapshot() = default;
    virtual int getNumReceiveAntennas() const = 0;
    virtual int getNumTransmitAntennas() const = 0;
    virtual Hz getReferenceFrequency() const = 0;
    virtual simtime_t getStartTime() const = 0;
    virtual simtime_t getEndTime() const = 0;
    virtual double getShadowingPowerGain() const = 0;
    virtual Hz getActualMaximumTemporalFrequency() const = 0;
    virtual simtime_t getMaximumExcessDelay() const = 0;
    virtual ComplexMatrix getResponse(simtime_t absoluteTime, Hz frequency) const = 0;
    virtual std::shared_ptr<const IChannelMatrixSnapshot> transpose() const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
