//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IERRORMODEL_H
#define __INET_IERRORMODEL_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/ISnir.h"

namespace inet {

namespace physicallayer {

/**
 * A region of the transmitted packet that an error decision found erroneous.
 */
struct ErrorRegion
{
    // the offset of the region in the transmitted packet
    b offset;
    // the length of the region
    b length;
    // the region error rate of the error model, at the SNIR of the time of the region
    double errorRate;
};

/**
 * The error decision of one signal part, the header or the data. The decision
 * of the whole signal combines them: it keeps the failed regions of the data,
 * which the received packet gets.
 */
struct PartErrors
{
    // true if the prerequisites of the part held and the packet holds the complete range of the part
    bool isDecoded = false;
    // true if at least one region failed, also a region without content in the packet
    bool hasErrors = false;
    // the failed regions that the packet holds, in packet order
    std::vector<ErrorRegion> regions;
};

/**
 * The error model describes how the signal to noise ratio affects the amount of
 * errors at the receiver. The main purpose of this model is to determine whether
 * if the received packet has errors or not. It also computes various physical
 * layer indications for higher layers such as packet error rate, bit error rate,
 * and symbol error rate.
 */
class INET_API IErrorModel : public virtual IPrintableObject
{
  public:
    virtual Packet *computeCorruptedPacket(const ISnir *snir) const = 0;

    /**
     * Returns the packet error rate based on SNIR, modulation, FEC encoding
     * and any other physical layer characteristics.
     */
    virtual double computePacketErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const = 0;

    /**
     * Returns the probability that a region of the provided signal part has at
     * least one error. The region has the provided length, and the transmitter
     * sends it from startTime to endTime. Only the header part and the data part
     * have regions.
     */
    virtual double computePacketErrorRate(const ISnir *snir, IRadioSignal::SignalPart part, b length, simtime_t startTime, simtime_t endTime) const = 0;

    /**
     * Returns the bit error rate based on SNIR, modulation, FEC encoding
     * and any other physical layer characteristics.
     */
    virtual double computeBitErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const = 0;

    /**
     * Returns the symbol error rate based on SNIR, modulation, and any other
     * physical layer characteristics.
     */
    virtual double computeSymbolErrorRate(const ISnir *snir, IRadioSignal::SignalPart part) const = 0;
};

} // namespace physicallayer

} // namespace inet

#endif

