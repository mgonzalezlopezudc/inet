//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELSNAPSHOT_H
#define __INET_CHANNELSNAPSHOT_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelSnapshot.h"

namespace inet {

namespace physicallayer {

class INET_API ChannelSnapshot : public IChannelSnapshot
{
  protected:
    const Ptr<const IFunction<double, Domain<simsec, Hz>>> powerGain;

  public:
    explicit ChannelSnapshot(const Ptr<const IFunction<double, Domain<simsec, Hz>>>& powerGain);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
    virtual Ptr<const IFunction<double, Domain<simsec, Hz>>> getPowerGain() const override;
};

} // namespace physicallayer

} // namespace inet

#endif
