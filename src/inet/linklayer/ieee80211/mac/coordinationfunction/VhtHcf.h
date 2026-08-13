//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTHCF_H
#define __INET_VHTHCF_H

#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"

namespace inet {
namespace ieee80211 {

/** Registered compatibility/configuration façade; Hcf owns VHT execution. */
class INET_API VhtHcf : public Hcf
{
};

} // namespace ieee80211
} // namespace inet

#endif
