//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_SNIRRECEIVERBASE_H
#define __INET_SNIRRECEIVERBASE_H

#include "inet/physicallayer/wireless/common/base/packetlevel/ReceiverBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ISnir.h"

namespace inet {

namespace physicallayer {

// REFACTOR TODO
class INET_API SnirReceiverBase : public ReceiverBase
{
  protected:
    enum class SnirThresholdMode {
        STM_UNDEFINED = -1,
        STM_MIN,
        STM_MEAN
    };

    double snirThreshold = NaN;
    SnirThresholdMode snirThresholdMode = SnirThresholdMode::STM_UNDEFINED;

  protected:
    virtual void initialize(int stage) override;

    /**
     * Returns true if the SNIR in the provided interval of the reception is above
     * the threshold, according to the threshold mode. A part or a region of the
     * signal uses the interval of its own time.
     */
    virtual bool computeIsSnirAboveThreshold(const ISnir *snir, simtime_t startTime, simtime_t endTime) const;

  public:
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual double getSnirThreshold() const { return snirThreshold; }

    virtual bool computeIsReceptionSuccessful(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part, const IInterference *interference, const ISnir *snir) const override;
};

} // namespace physicallayer

} // namespace inet

#endif

