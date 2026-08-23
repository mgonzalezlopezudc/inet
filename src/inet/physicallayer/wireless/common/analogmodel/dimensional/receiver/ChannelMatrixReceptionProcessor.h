//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXRECEPTIONPROCESSOR_H
#define __INET_CHANNELMATRIXRECEPTIONPROCESSOR_H

#include <vector>

#include "inet/common/SimpleModule.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixReceptionProcessor.h"

namespace inet {
namespace physicallayer {

/**
 * Stateless composition of receive-row selection, a one-stream combiner, and
 * a multi-stream detector. Configuration is frozen during initialization.
 */
class INET_API ChannelMatrixReceptionProcessor : public SimpleModule,
    public virtual IChannelMatrixReceptionProcessor
{
  public:
    enum class AntennaSelection {
        ALL,
        FIXED,
        OPTIMAL
    };

    enum class OneStreamCombiner {
        MAXIMUM_RATIO,
        SELECTION,
        MAXIMUM_SINR
    };

    enum class SpatialStreamDetector {
        ZERO_FORCING,
        MMSE,
        MMSE_SIC
    };

    struct INET_API Configuration final
    {
        AntennaSelection antennaSelection = AntennaSelection::ALL;
        int activeReceiveAntennaCount = -1;
        std::vector<int> fixedReceiveRows;
        OneStreamCombiner oneStreamCombiner = OneStreamCombiner::MAXIMUM_RATIO;
        SpatialStreamDetector spatialStreamDetector = SpatialStreamDetector::MMSE;
    };

  protected:
    Configuration configuration;

    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *message) override;

    static ChannelMatrixDetectionResult computeForRows(const ChannelMatrixReceptionContext& context,
        const Configuration& configuration, const std::vector<int>& selectedReceiveRows);
    static ChannelMatrixDetectionResult computeSpaceTimeBlockForRows(
        const std::vector<ChannelMatrixReceptionContext>& slotContexts,
        const Configuration& configuration, const std::vector<int>& selectedReceiveRows);

  public:
    static ChannelMatrixDetectionResult compute(const ChannelMatrixReceptionContext& context,
        const Configuration& configuration);
    static ChannelMatrixDetectionResult computeSpaceTimeBlock(
        const std::vector<ChannelMatrixReceptionContext>& slotContexts,
        const Configuration& configuration);

    virtual ChannelMatrixDetectionResult compute(const ChannelMatrixReceptionContext& context) const override {
        return compute(context, configuration);
    }
    virtual ChannelMatrixDetectionResult computeSpaceTimeBlock(
        const std::vector<ChannelMatrixReceptionContext>& slotContexts) const override {
        return computeSpaceTimeBlock(slotContexts, configuration);
    }

    const Configuration& getConfiguration() const { return configuration; }
};

} // namespace physicallayer
} // namespace inet

#endif
