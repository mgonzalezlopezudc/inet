// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANEXTENDEDDISPATCHHEADERSERIALIZER_H
#define __INET_LOWPANEXTENDEDDISPATCHHEADERSERIALIZER_H
#include "inet/common/packet/serializer/FieldsChunkSerializer.h"
namespace inet { namespace lowpan {
class INET_API LowpanExtendedDispatchHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serializeFields(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserializeFields(MemoryInputStream& stream, const std::type_info&) const override;
};
} }
#endif
