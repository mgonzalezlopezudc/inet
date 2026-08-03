// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef __INET_DURATIONFINALIZEDREQ_H
#define __INET_DURATIONFINALIZEDREQ_H

#include "inet/common/TagBase.h"

namespace inet {
namespace ieee80211 {

/** Marks a frame-sequence-owned Duration field as final. */
class INET_API DurationFinalizedReq final : public TagBase
{
  public:
    virtual DurationFinalizedReq *dup() const override { return new DurationFinalizedReq(*this); }
};

} // namespace ieee80211
} // namespace inet

#endif
