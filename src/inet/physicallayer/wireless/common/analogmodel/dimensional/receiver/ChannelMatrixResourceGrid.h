//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXRESOURCEGRID_H
#define __INET_CHANNELMATRIXRESOURCEGRID_H

#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioSignal.h"

namespace inet {
namespace physicallayer {

using namespace inet::units::values;

/** Immutable ordered resource cells at which matrix reception is evaluated. */
class INET_API ChannelMatrixResourceGrid final
{
  public:
    class INET_API Cell final
    {
      private:
        simtime_t startTime;
        simtime_t endTime;
        Hz lowerFrequency;
        Hz upperFrequency;
        IRadioSignal::SignalPart signalPart;
        double powerSpectralDensityScale;

      public:
        Cell(simtime_t startTime, simtime_t endTime, Hz lowerFrequency, Hz upperFrequency,
            IRadioSignal::SignalPart signalPart, double powerSpectralDensityScale = 1);

        simtime_t getStartTime() const { return startTime; }
        simtime_t getEndTime() const { return endTime; }
        Hz getLowerFrequency() const { return lowerFrequency; }
        Hz getUpperFrequency() const { return upperFrequency; }
        IRadioSignal::SignalPart getSignalPart() const { return signalPart; }
        double getPowerSpectralDensityScale() const { return powerSpectralDensityScale; }
        simtime_t getCenterTime() const { return startTime + (endTime - startTime) / 2; }
        Hz getCenterFrequency() const { return lowerFrequency + (upperFrequency - lowerFrequency) / 2; }
        bool contains(simtime_t time, Hz frequency) const;
    };

  private:
    std::vector<Cell> cells;

  public:
    explicit ChannelMatrixResourceGrid(const std::vector<Cell>& cells, size_t maximumCellCount);

    const std::vector<Cell>& getCells() const { return cells; }
    const Cell *findCell(simtime_t time, Hz frequency) const;
};

} // namespace physicallayer
} // namespace inet

#endif
