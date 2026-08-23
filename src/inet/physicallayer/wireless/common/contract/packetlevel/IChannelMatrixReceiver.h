//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_ICHANNELMATRIXRECEIVER_H
#define __INET_ICHANNELMATRIXRECEIVER_H

#include <cstddef>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioSignal.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IReceiver.h"

namespace inet {
namespace physicallayer {

class IChannelMatrixReceptionProcessor;
class IReception;

/** Technology-supplied occupied time/frequency resource relative to a PPDU. */
class INET_API ChannelMatrixResourceCell final
{
  private:
    simtime_t startOffset;
    simtime_t endOffset;
    Hz lowerBasebandFrequency;
    Hz upperBasebandFrequency;
    IRadioSignal::SignalPart signalPart;
    double powerSpectralDensityScale;

  public:
    ChannelMatrixResourceCell(simtime_t startOffset, simtime_t endOffset,
        Hz lowerBasebandFrequency, Hz upperBasebandFrequency,
        IRadioSignal::SignalPart signalPart,
        double powerSpectralDensityScale = 1) :
        startOffset(startOffset), endOffset(endOffset),
        lowerBasebandFrequency(lowerBasebandFrequency),
        upperBasebandFrequency(upperBasebandFrequency), signalPart(signalPart),
        powerSpectralDensityScale(powerSpectralDensityScale) {}

    simtime_t getStartOffset() const { return startOffset; }
    simtime_t getEndOffset() const { return endOffset; }
    Hz getLowerBasebandFrequency() const { return lowerBasebandFrequency; }
    Hz getUpperBasebandFrequency() const { return upperBasebandFrequency; }
    IRadioSignal::SignalPart getSignalPart() const { return signalPart; }
    /** Converts the model's full-band flat PSD to PSD on occupied resources. */
    double getPowerSpectralDensityScale() const { return powerSpectralDensityScale; }
    bool contains(simtime_t offset, Hz basebandFrequency) const {
        return offset >= startOffset && offset < endOffset &&
            basebandFrequency >= lowerBasebandFrequency &&
            basebandFrequency < upperBasebandFrequency;
    }
};

/** Optional receiver capability consumed by matrix-aware analog models. */
class INET_API IChannelMatrixReceiver : public virtual IReceiver
{
  public:
    virtual const IChannelMatrixReceptionProcessor *getChannelMatrixReceptionProcessor() const = 0;
    virtual size_t getMaximumMaterializedResourceCells() const = 0;
    /** Empty means that the generic continuum approximation must be used. */
    virtual std::vector<ChannelMatrixResourceCell> getChannelMatrixResourceCells(
        const IReception& reception) const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
