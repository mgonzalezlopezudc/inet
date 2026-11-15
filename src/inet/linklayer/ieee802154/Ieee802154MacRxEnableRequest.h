// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IEEE802154MACRXENABLEREQUEST_H
#define __INET_IEEE802154MACRXENABLEREQUEST_H

#include <vector>
#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee802154/Ieee802154MacServiceResult.h"

namespace inet {

struct Ieee802154RxInstanceDescriptor
{
    uint32_t onTime = 0; ///< 24-bit symbol counter; zero means ASAP.
    uint32_t duration = 0; ///< Symbols; 0 disables and 0xffffff is indefinite.
    bool autoOff = false;
};

struct Ieee802154MacRxEnableRequest
{
    std::vector<Ieee802154RxInstanceDescriptor> instances;
    bool deferPermit = false;
    bool ranging = false;
};

struct Ieee802154MacRxEnableConfirm
{
    Ieee802154MacServiceResult result;
    std::vector<Ieee802154MacStatus> statusDetail;
};

struct Ieee802154MacRxEnableIndication { uint32_t timestamp; };

} // namespace inet
#endif
