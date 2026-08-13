//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IQOSCOORDINATIONFUNCTION_H
#define __INET_IQOSCOORDINATIONFUNCTION_H

#include "inet/linklayer/ieee80211/mac/contract/ICoordinationFunction.h"

namespace inet {

class MacAddress;

namespace ieee80211 {

class IIeee80211MgmtExchangeResultHandler;

/**
 * C++ contract paired with the IHcf NED module interface.
 */
class INET_API IQosCoordinationFunction : public ICoordinationFunction
{
  public:
    virtual ~IQosCoordinationFunction() {}

    virtual void setMgmtExchangeResultHandler(IIeee80211MgmtExchangeResultHandler *handler) = 0;
    virtual bool isAllowedToProcessIntactHtAmpdu() const = 0;
    virtual bool isExpectingIntactAmpduResponse() const { return false; }
    /**
     * Notifies the coordination function about a legacy preamble. The packet
     * is borrowed for the duration of this call and must not be retained or
     * deleted by the implementation.
     */
    virtual void legacyPreambleReceived(const Packet *packet) = 0;
    virtual void twtServicePeriodChanged() = 0;
    virtual void invalidatePeerDerivedState(const MacAddress& peer) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
