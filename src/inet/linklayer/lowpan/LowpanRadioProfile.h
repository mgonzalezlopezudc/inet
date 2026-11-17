// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANRADIOPROFILE_H
#define __INET_LOWPANRADIOPROFILE_H
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
namespace inet { namespace lowpan {
// Fixed stationary PHY envelope shared by compatibility and native link domains.
class INET_API LowpanRadioProfile
{
  public:
    static simtime_t maximumPropagation(const physicallayer::IRadio *sender,
            const std::vector<const physicallayer::IRadio *>& peers);
    static simtime_t transmissionTail(const physicallayer::IRadio *radio, simtime_t turnaround, simtime_t propagation);
    static void validateInputPath(const physicallayer::IRadio *radio);
};
} }
#endif
