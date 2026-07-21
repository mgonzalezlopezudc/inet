//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211PHYHEADERSERIALIZER_H
#define __INET_IEEE80211PHYHEADERSERIALIZER_H

#include <optional>

#include "inet/common/packet/serializer/FieldsChunkSerializer.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"

namespace inet {

namespace physicallayer {

/**
 * Converts between Ieee80211FhssPhyHeader and binary network byte order IEEE 802.11 FHSS PHY header.
 */
class INET_API Ieee80211FhssPhyHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211FhssPhyHeaderSerializer() : FieldsChunkSerializer() {}
};

/**
 * Converts between Ieee80211IrPhyHeader and binary network byte order IEEE 802.11 IR PHY header.
 */
class INET_API Ieee80211IrPhyHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211IrPhyHeaderSerializer() : FieldsChunkSerializer() {}
};

/**
 * Converts between Ieee80211DsssPhyHeader and binary network byte order IEEE 802.11 DSSS PHY header.
 */
class INET_API Ieee80211DsssPhyHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211DsssPhyHeaderSerializer() : FieldsChunkSerializer() {}
};

/**
 * Converts between Ieee80211HrDsssPhyHeader and binary network byte order IEEE 802.11 HR/DSSS PHY header.
 */
class INET_API Ieee80211HrDsssPhyHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211HrDsssPhyHeaderSerializer() : FieldsChunkSerializer() {}
};

/**
 * Converts between Ieee80211OfdmPhyHeader and binary network byte order IEEE 802.11 OFDM PHY header.
 */
class INET_API Ieee80211OfdmPhyHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211OfdmPhyHeaderSerializer() : FieldsChunkSerializer() {}
};

/**
 * Converts between Ieee80211ErpOfdmPhyHeader and binary network byte order IEEE 802.11 ERP OFDM PHY header.
 */
class INET_API Ieee80211ErpOfdmPhyHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211ErpOfdmPhyHeaderSerializer() : FieldsChunkSerializer() {}
};

/**
 * Converts the packet-level HT PHY header and its serialized representation,
 * including the BCC/LDPC coding selector.
 */
class INET_API Ieee80211HtPhyHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211HtPhyHeaderSerializer() : FieldsChunkSerializer() {}
};

/**
 * Converts the packet-level VHT PHY header and its serialized representation,
 * including the BCC/LDPC coding selector.
 */
class INET_API Ieee80211VhtPhyHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211VhtPhyHeaderSerializer() : FieldsChunkSerializer() {}
};

/**
 * Converts the INET packet-level representation of an HE PHY header,
 * including the PPDU format, common settings, and per-user RU allocations.
 */
class INET_API Ieee80211HePhyHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    std::optional<Ieee80211HePpduFormat> expectedPpduFormat;

    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211HePhyHeaderSerializer() : FieldsChunkSerializer() {}
    explicit Ieee80211HePhyHeaderSerializer(Ieee80211HePpduFormat expectedPpduFormat) : FieldsChunkSerializer(), expectedPpduFormat(expectedPpduFormat) {}
};

/**
 * Serializes the 12-byte INET-internal per-RU payload delimiter prepended to
 * each MPDU inside an EHT MU PPDU container.
 */
class INET_API Ieee80211EhtRuPayloadHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211EhtRuPayloadHeaderSerializer() : FieldsChunkSerializer() {}
};

} // namespace physicallayer

} // namespace inet

#endif
