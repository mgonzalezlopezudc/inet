//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_CHANNELMATRIXSNAPSHOT_H
#define __INET_CHANNELMATRIXSNAPSHOT_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h"

namespace inet {
namespace physicallayer {

class INET_API ConstantChannelMatrixResponse : public IChannelMatrixResponse
{
  protected:
    const ChannelMatrix channelMatrix;

  public:
    explicit ConstantChannelMatrixResponse(ChannelMatrix channelMatrix);

    virtual bool isTimeInvariant() const override { return true; }
    virtual int getNumReceiveAntennas() const override;
    virtual int getNumTransmitAntennas() const override;
    virtual ChannelMatrix getChannelMatrix(simsec time, Hz frequency) const override;
};

class INET_API ChannelMatrixSnapshot : public IChannelMatrixSnapshot
{
  protected:
    const Ptr<const IFunction<double, Domain<simsec, Hz>>> powerGain;
    const std::shared_ptr<const IChannelMatrixResponse> channelMatrixResponse;

  public:
    ChannelMatrixSnapshot(const Ptr<const IFunction<double, Domain<simsec, Hz>>>& powerGain,
            std::shared_ptr<const IChannelMatrixResponse> channelMatrixResponse);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
    virtual Ptr<const IFunction<double, Domain<simsec, Hz>>> getPowerGain() const override;
    virtual std::shared_ptr<const IChannelMatrixResponse> getChannelMatrixResponse() const override;
};

} // namespace physicallayer
} // namespace inet

#endif
