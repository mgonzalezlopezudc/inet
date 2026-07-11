//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211PHYHEADERSERIALIZER_H
#define __INET_IEEE80211PHYHEADERSERIALIZER_H

#include "inet/common/packet/serializer/FieldsChunkSerializer.h"

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
 * Converts the INET packet-level representation of a HE MU PHY header,
 * including common PPDU settings and per-user RU allocations.
 */
class INET_API Ieee80211HeMuPhyHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211HeMuPhyHeaderSerializer() : FieldsChunkSerializer() {}
};

/**
 * Serializes the 12-byte INET-internal per-RU payload delimiter prepended to
 * each MPDU inside an HE MU DL or TB PPDU container. The encoded fields are
 * ruIndex (4 B LE int), ruToneSize (2 B LE), ruToneOffset (2 B LE),
 * staId (2 B LE), one packed mcs/nss byte, and one flags byte (dcm|muMimo).
 * Internal PPDU-building state (mpduLength, spatialConfiguration, totalNsts,
 * streamStartIndex, leakageSum) is intentionally NOT serialized.
 */
class INET_API Ieee80211HeMuRuPayloadHeaderSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    Ieee80211HeMuRuPayloadHeaderSerializer() : FieldsChunkSerializer() {}
};

/**
 * Serializes the 12-byte INET-internal per-RU payload delimiter prepended to
 * each MPDU inside an EHT MU PPDU container. Identical layout to
 * Ieee80211HeMuRuPayloadHeaderSerializer but operates on mruIndex/mruToneSize/
 * mruToneOffset fields.
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
