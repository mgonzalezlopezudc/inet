// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IIEEE802154PHYSERVICEPROVIDER_H
#define __INET_IIEEE802154PHYSERVICEPROVIDER_H

#include "inet/physicallayer/wireless/ieee802154/contract/Ieee802154PhyServicePrimitives.h"

namespace inet {

class Packet;

namespace physicallayer {

/**
 * Gate-free PHY role. The module binds one typed clientModule. One active
 * operation is admitted at a time; reset may preempt it. Nonzero request IDs
 * are client-issued and strictly increasing across all attempts and restarts.
 * UINT64_MAX is usable once; IDs never wrap. Refusal spends the ID without
 * transferring ownership or producing a callback. Deferred requests are copied.
 *
 * ACCEPTED transfers a complete PSDU, including MAC FCS, before returning and
 * before any inline callback. The provider consumes/destroys it exactly once
 * before terminal confirmation. A missing client causes cleanup without a call.
 * A ready transceiver starts at request.startAt, or now when absent. A target
 * in the past is an accepted INVALID_PARAMETER failure, not a late transmit.
 *
 * GET/SET may complete inline. A state transition confirms actual readiness,
 * not acceptance. Successful CCA/ED reports its actual observation interval.
 * Channel/power/measurement writes while a transceiver operation or reception
 * is active are refused BUSY, without changing the current configuration.
 */
class INET_API IIeee802154PhyServiceProvider
{
  public:
    virtual ~IIeee802154PhyServiceProvider() = default;

    virtual Ieee802154PhyTimingParameters getTimingParameters() const = 0;
    virtual Ieee802154PhyServiceAdmission submitTransmit(Ieee802154PhyServiceRequestId requestId,
            Packet *psdu, const Ieee802154PhyTransmitRequest& request) = 0;
    virtual Ieee802154PhyServiceAdmission submitCca(Ieee802154PhyServiceRequestId requestId) = 0;
    virtual Ieee802154PhyServiceAdmission submitEd(Ieee802154PhyServiceRequestId requestId) = 0;
    virtual Ieee802154PhyServiceAdmission submitState(Ieee802154PhyServiceRequestId requestId,
            Ieee802154PhyState state) = 0;
    virtual Ieee802154PhyServiceAdmission submitGet(Ieee802154PhyServiceRequestId requestId,
            const Ieee802154PhyGetRequest& request) = 0;
    virtual Ieee802154PhyServiceAdmission submitSet(Ieee802154PhyServiceRequestId requestId,
            const Ieee802154PhySetRequest& request) = 0;
    /**
     * Reset holds a barrier while aborting the active operation with RESET.
     * Reset itself is not cancellable. Reentrant submissions receive BUSY until
     * the barrier clears before reset confirmation. Readiness becomes OFF;
     * setDefaultPib selects default restoration versus configuration retention.
     * Stop/crash wins over reset and reports STOPPED, never later SUCCESS.
     */
    virtual Ieee802154PhyServiceAdmission submitReset(Ieee802154PhyServiceRequestId requestId,
            bool setDefaultPib) = 0;
    /**
     * CANCELLED detaches the operation and delivers one CANCELLED confirmation
     * before returning. Once a PPDU starts, transmit cancellation returns
     * NOT_CANCELLABLE; stop/reset can still abort the radio operation. No start
     * or completion callback may follow a terminal result for the same ID.
     */
    virtual Ieee802154PhyServiceCancelResult cancel(Ieee802154PhyServiceRequestId requestId) = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
