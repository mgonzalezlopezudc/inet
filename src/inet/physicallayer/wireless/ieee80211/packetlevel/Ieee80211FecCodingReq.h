//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211FECCODINGREQ_H
#define __INET_IEEE80211FECCODINGREQ_H

#include "inet/common/TagBase.h"
#include "inet/common/packet/Packet.h"

namespace inet {
namespace physicallayer {

/**
 * Model-only MAC-to-PHY permission for selecting the LDPC variant of an
 * already selected IEEE 802.11 mode. Rate selection initializes the request
 * exactly once; packet replacement paths may only propagate that decision.
 */
class INET_API Ieee80211FecCodingReq final : public TagBase
{
  private:
    bool initialized = false;
    bool ldpcAllowed = false;

  public:
    Ieee80211FecCodingReq() = default;
    Ieee80211FecCodingReq(const Ieee80211FecCodingReq&) = default;
    virtual Ieee80211FecCodingReq *dup() const override { return new Ieee80211FecCodingReq(*this); }

    void setLdpcAllowed(bool value)
    {
        if (initialized && ldpcAllowed != value)
            throw cRuntimeError("IEEE 802.11 FEC coding request is immutable after initialization");
        initialized = true;
        ldpcAllowed = value;
    }

    bool getLdpcAllowed() const
    {
        if (!initialized)
            throw cRuntimeError("IEEE 802.11 FEC coding request is uninitialized");
        return ldpcAllowed;
    }
};

inline void propagateIeee80211FecCodingReq(const Packet *source, Packet *destination)
{
    auto sourceRequest = const_cast<Packet *>(source)->findTag<Ieee80211FecCodingReq>();
    if (sourceRequest != nullptr)
        destination->addTagIfAbsent<Ieee80211FecCodingReq>()->setLdpcAllowed(sourceRequest->getLdpcAllowed());
}

} // namespace physicallayer
} // namespace inet

#endif
