//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211MGMTFRAMESERIALIZER_H
#define __INET_IEEE80211MGMTFRAMESERIALIZER_H

#include "inet/common/packet/serializer/FieldsChunkSerializer.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"

namespace inet {

namespace ieee80211 {

/**
 * Converts between Ieee80211MgmtFrame and binary network byte order IEEE 802.11 mgmt frame.
 */
class INET_API Ieee80211MgmtFrameSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    // The type-aware entry point below is required for management bodies,
    // because the fixed body layout is selected by the requested chunk type.
    // Keep the legacy FieldsChunkSerializer hook implemented for the abstract
    // base contract; callers must use the type-aware entry point.
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211MgmtFrameSerializer() : FieldsChunkSerializer() {}
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream, const std::type_info& typeInfo) const override;
};

} // namespace ieee80211

} // namespace inet

#endif
