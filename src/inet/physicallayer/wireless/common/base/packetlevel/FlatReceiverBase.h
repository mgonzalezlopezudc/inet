//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_FLATRECEIVERBASE_H
#define __INET_FLATRECEIVERBASE_H

#include "inet/physicallayer/wireless/common/base/packetlevel/NarrowbandReceiverBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IErrorModel.h"

namespace inet {

namespace physicallayer {

// REFACTOR TODO
class INET_API FlatReceiverBase : public NarrowbandReceiverBase
{
  protected:
    const IErrorModel *errorModel;
    W energyDetection;
    W sensitivity;

  protected:
    virtual void initialize(int stage) override;

    using NarrowbandReceiverBase::computeIsReceptionPossible;

    virtual Packet *computeReceivedPacket(const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const override;

    /** @name Error decisions of the signal regions */
    //@{
    /**
     * Gives the range of the transmitted packet that carries the bits of the
     * header part or the data part. The header range comes before the data range.
     * The offset is -1 if the packet does not hold the bits of the part as content,
     * for example a header that the radio does not represent as a chunk; the length
     * is then the number of bits of the part. Returns false if the packet does not
     * hold the complete range. This implementation throws an error.
     */
    virtual bool computePartRange(const ITransmission *transmission, IRadioSignal::SignalPart part, b& offset, b& length) const;

    /**
     * Gives the interval in which the transmitter sends the bits [offset, offset +
     * length) of a part that has partLength bits. This implementation divides the
     * time of the part in proportion to the bits.
     */
    virtual void computeRegionInterval(const IReception *reception, IRadioSignal::SignalPart part, b offset, b length, b partLength, simtime_t& startTime, simtime_t& endTime) const;

    /**
     * Returns true if the SNIR of the preamble is above the threshold. The
     * preamble has no regions and no error rate.
     */
    virtual bool computeIsPreambleReceived(const IReception *reception, const ISnir *snir) const;

    /**
     * Decides each region of the header part or the data part one time. A region
     * fails if its SNIR is not above the threshold, or else if one draw against
     * its error rate fails. Both use the SNIR of the time of the region.
     */
    virtual PartErrors computePartErrors(const IReception *reception, const ISnir *snir, IRadioSignal::SignalPart part) const;

    /**
     * Returns the errors of the cached decision of a signal part.
     */
    const PartErrors& getPartErrors(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part) const;
    //@}

  public:
    FlatReceiverBase();

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual W getMinReceptionPower() const override { return sensitivity; }

    virtual const IListeningDecision *computeListeningDecision(const IListening *listening, const IInterference *interference) const override;
    virtual const IReceptionDecision *computeReceptionDecision(const IListening *listening, const IReception *reception, IRadioSignal::SignalPart part, const IInterference *interference, const ISnir *snir) const override;
    virtual const IReceptionResult *computeReceptionResult(const IListening *listening, const IReception *reception, const IInterference *interference, const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const override;

    virtual const IErrorModel *getErrorModel() const { return errorModel; }

    virtual W getEnergyDetection() const { return energyDetection; }
    virtual void setEnergyDetection(W energyDetection) { this->energyDetection = energyDetection; }

    virtual W getSensitivity() const { return sensitivity; }
    virtual void setSensitivity(W sensitivity) { this->sensitivity = sensitivity; }
};

} // namespace physicallayer

} // namespace inet

#endif

