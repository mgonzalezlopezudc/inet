//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtPpduLayout.h"

#include <cmath>
#include <limits>

namespace inet {
namespace physicallayer {

Ieee80211HtPpduLayout::Ieee80211HtPpduLayout(
    const Ieee80211HtPpduDescription& description, simtime_t ppduDuration) :
    numberOfDataLongTrainingFields(description.getNumberOfDataLongTrainingFields()),
    dataStart(getHtShortTrainingEnd() +
        getHtLongTrainingFieldDuration() * numberOfDataLongTrainingFields),
    dataEnd(ppduDuration),
    dataSymbolDuration(description.getShortGi() ?
        SimTime(3600, SIMTIME_NS) : SimTime(4, SIMTIME_US)),
    numberOfDataSymbols(0)
{
    if (description.getPreambleFormat() != Ieee80211HtPreambleMode::HT_PREAMBLE_MIXED)
        throw cRuntimeError("HT PPDU layout currently supports mixed format only");
    if (!std::isfinite(ppduDuration.dbl()) || ppduDuration <= dataStart)
        throw cRuntimeError("HT PPDU duration %s does not leave a positive data interval after %s",
            ppduDuration.str().c_str(), dataStart.str().c_str());
    const simtime_t dataDuration = dataEnd - dataStart;
    if (dataDuration.raw() % dataSymbolDuration.raw() != 0)
        throw cRuntimeError("HT data interval is not a whole number of OFDM symbols");
    const int64_t symbolCount = dataDuration.raw() / dataSymbolDuration.raw();
    if (symbolCount <= 0 || symbolCount > std::numeric_limits<int>::max())
        throw cRuntimeError("HT data symbol count is outside the supported range");
    numberOfDataSymbols = static_cast<int>(symbolCount);
}

simtime_t Ieee80211HtPpduLayout::getHtLongTrainingFieldStart(int index) const
{
    if (index < 0 || index >= numberOfDataLongTrainingFields)
        throw cRuntimeError("HT-DLTF index %d is outside [0,%d)",
            index, numberOfDataLongTrainingFields);
    return getHtShortTrainingEnd() + getHtLongTrainingFieldDuration() * index;
}

simtime_t Ieee80211HtPpduLayout::getHtLongTrainingFieldEnd(int index) const
{
    return getHtLongTrainingFieldStart(index) + getHtLongTrainingFieldDuration();
}

} // namespace physicallayer
} // namespace inet
