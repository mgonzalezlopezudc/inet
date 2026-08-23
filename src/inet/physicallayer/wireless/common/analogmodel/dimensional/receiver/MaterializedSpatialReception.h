//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_MATERIALIZEDSPATIALRECEPTION_H
#define __INET_MATERIALIZEDSPATIALRECEPTION_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixDetectionResult.h"
#include "inet/physicallayer/wireless/common/analogmodel/dimensional/receiver/ChannelMatrixResourceGrid.h"

namespace inet {
namespace physicallayer {

/** Eager immutable detector outputs with no evaluator or module references. */
class INET_API MaterializedSpatialReception final
{
  public:
    class INET_API Cell final
    {
      private:
        ChannelMatrixResourceGrid::Cell resource;
        ChannelMatrixDetectionResult detectionResult;
        double scalarSinr;

      public:
        Cell(const ChannelMatrixResourceGrid::Cell& resource,
            const ChannelMatrixDetectionResult& detectionResult);

        const ChannelMatrixResourceGrid::Cell& getResource() const { return resource; }
        const ChannelMatrixDetectionResult& getDetectionResult() const { return detectionResult; }
        double getScalarSinr() const { return scalarSinr; }
    };

  private:
    std::vector<Cell> cells;
    double minimumSinr;
    double maximumSinr;
    double meanSinr;

  public:
    explicit MaterializedSpatialReception(const std::vector<Cell>& cells);

    const std::vector<Cell>& getCells() const { return cells; }
    const Cell *findCell(simtime_t time, Hz frequency) const;
    double getMinimumSinr() const { return minimumSinr; }
    double getMaximumSinr() const { return maximumSinr; }
    double getMeanSinr() const { return meanSinr; }
    double getMinimumSinr(IRadioSignal::SignalPart part) const;
    double getMaximumSinr(IRadioSignal::SignalPart part) const;
    double getMeanSinr(IRadioSignal::SignalPart part) const;
    bool allRequiredOutputPowersMeet(IRadioSignal::SignalPart part, W sensitivity) const;
};

} // namespace physicallayer
} // namespace inet

#endif
