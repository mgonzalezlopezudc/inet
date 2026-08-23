//
// Copyright (C) 2026
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211PHYMODERESOLVER_H
#define __INET_IEEE80211PHYMODERESOLVER_H

#include <memory>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtPpduDescription.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"

namespace inet {
namespace physicallayer {

/** Pure, receiver-independent validation and canonicalization of HT-SIG. */
class INET_API Ieee80211PhyModeResolver final
{
  public:
    enum class Status {
        SUCCESS,
        FORMAT_VIOLATION,
        RESERVED_HT_SIG
    };

    struct INET_API Result
    {
        Status status;
        std::shared_ptr<const Ieee80211HtPpduDescription> description;

        bool isSuccess() const { return status == Status::SUCCESS; }
        Status getStatus() const { return status; }
        const std::shared_ptr<const Ieee80211HtPpduDescription>& getDescription() const { return description; }
    };

    static Result resolve(const Ieee80211HtPhyHeader& header, const Ieee80211HtPpduContext& context);
};

} // namespace physicallayer
} // namespace inet

#endif
