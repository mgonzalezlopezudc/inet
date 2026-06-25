//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.common.checksum.ChecksumMode;
import inet.linklayer.common.FcsMode;

namespace inet::physicallayer;

class Ieee80211PhyPreamble extends FieldsChunk
{
}

class Ieee80211PhyHeader extends FieldsChunk
{
    // TODO remove this, because not all subtypes have lengthField
    B lengthField;
}

//
// IEEE 802.11 14. Frequency-Hopping spread spectrum (FHSS)
//
class Ieee80211FhssPhyPreamble extends Ieee80211PhyPreamble
{
    chunkLength = b(96);
}

class Ieee80211FhssPhyHeader extends Ieee80211PhyHeader
{
    chunkLength = b(32);
    uint16_t plw;
    uint8_t psf;
    uint16_t fcs;
    ChecksumMode fcsMode = FCS_MODE_UNDEFINED;  // should be FcsMode, but OMMeT++ bug #1351 prevents that (fixed in OMNeT++ 6.2)
}

//
// IEEE 802.11 15. Infrared (IR) PHY
//
class Ieee80211IrPhyPreamble extends Ieee80211PhyPreamble
{
}

class Ieee80211IrPhyHeader extends Ieee80211PhyHeader
{
    // TODO B lengthField;
    uint16_t fcs;
    ChecksumMode fcsMode = FCS_MODE_UNDEFINED;  // should be FcsMode, but OMMeT++ bug #1351 prevents that (fixed in OMNeT++ 6.2)
}

//
// IEEE 802.11 16. DSSS PHY
//
class Ieee80211DsssPhyPreamble extends Ieee80211PhyPreamble
{
    chunkLength = b(144);
}

class Ieee80211DsssPhyHeader extends Ieee80211PhyHeader
{
    chunkLength = B(6);
    uint8_t signal;
    uint8_t service;
    // TODO B lengthField;
    uint16_t fcs;
    ChecksumMode fcsMode = FCS_MODE_UNDEFINED;  // should be FcsMode, but OMMeT++ bug #1351 prevents that (fixed in OMNeT++ 6.2)
}

//
// IEEE 802.11 17. High Rate direct sequence spread spectrum (HR/DSSS)
//
class Ieee80211HrDsssPhyPreamble extends Ieee80211DsssPhyPreamble
{
}

class Ieee80211HrDsssPhyHeader extends Ieee80211DsssPhyHeader
{
}

//
// IEEE 802.11 18. Orthogonal frequency division multiplexing (OFDM)
//
class Ieee80211OfdmPhyPreamble extends Ieee80211PhyPreamble
{
}

// The length field is a 12-bit integer indicates the number of
// octets in the PSDU that the MAC is currently requesting the PHY to
// transmit.
//
// The rate field (18.3.4.2 RATE field) is a 4-bit integer carries
// information about the modulation and coding.
//
// There are other fields which are not explicitly represented here,
// for e.g. Parity, Reserved, SERVICE, Pad Bits.
//
// The PSDU field must be contained as an encapsulated packet.
//
// This packet is intended to represent the whole PPDU frame in the
// IEEE 802.11 PHY. For detailed description, you may refer to 18.3.2 PLCP
// frame format, Figure 18-1—PPDU frame format in IEEE Std. 802.11-2012.
//
class Ieee80211OfdmPhyHeader extends Ieee80211PhyHeader
{
    // The PLCP header is composed of RATE (4), Reserved (1), LENGTH (12), Parity (1),
    // Tail (6) and SERVICE (16) fields.
    chunkLength = B(5);
    uint8_t rate;
    bool reserved = false;
    // TODO B lengthField;
    bool parity = false;
    uint8_t tail = 0;
    uint16_t service;
}

//
// IEEE 802.11 19. Extended Rate PHY (ERP) OFDM
//
class Ieee80211ErpOfdmPhyPreamble extends Ieee80211OfdmPhyPreamble
{
}

class Ieee80211ErpOfdmPhyHeader extends Ieee80211OfdmPhyHeader
{
}

//
// IEEE 802.11 20. High Throughput (HT)
//
class Ieee80211HtPhyPreamble extends Ieee80211PhyPreamble
{
}

//
// HT PHY header modelled at packet level. coding selects BCC (0) or LDPC (1)
// for the associated HT data field.
//
class Ieee80211HtPhyHeader extends Ieee80211PhyHeader
{
    uint8_t coding = 0; // 0: BCC, 1: LDPC
}

// IEEE 802.11 21. High Throughput (VHT)
//
class Ieee80211VhtPhyPreamble extends Ieee80211PhyPreamble
{
}

//
// VHT PHY header modelled at packet level. coding selects BCC (0) or LDPC (1)
// for the associated VHT data field.
//
class Ieee80211VhtPhyHeader extends Ieee80211PhyHeader
{
    uint8_t coding = 0; // 0: BCC, 1: LDPC
}

//
// IEEE 802.11 22. High Efficiency (HE) Multi-User (MU)
//
//
// Per-user HE MU allocation carried by an Ieee80211HeMuPhyHeader. The RU
// geometry and station ID identify the intended receiver; MCS, NSS, DCM and
// PSDU length drive its independent reception calculation.
//
struct Ieee80211HeMuUserInfo
{
    int ruIndex;
    uint16_t ruToneSize;
    uint16_t ruToneOffset;
    uint16_t staId;
    uint8_t mcs = 0;
    uint8_t numberOfSpatialStreams = 1;
    bool dcm = false;
    B psduLength;
    simtime_t duration;
    int streamStartIndex = 0;
    double leakageSum = 0;
}

//
// Packet-level common header of a HE downlink MU or trigger-based uplink
// PPDU. The serializer emits the HE signaling fields only; runtime-only model
// state such as trigger correlation and resolved per-user durations stays in
// the chunk object and is not part of the serialized PHY header.
//
class Ieee80211HeMuPhyHeader extends Ieee80211PhyHeader
{
    uint8_t bssColor = 0;
    uint8_t ppduFormat = 0; // 0: HE MU downlink, 1: HE trigger-based uplink
    uint8_t guardInterval = 2; // 0: 0.8 us, 1: 1.6 us, 2: 3.2 us
    uint8_t coding = 0; // 0: BCC, 1: LDPC
    uint8_t packetExtensionDurationUs = 0;
    uint8_t puncturedSubchannelMask = 0; // 20 MHz subchannels, LSB is primary
    uint32_t triggerId = 0;
    simtime_t commonDuration;
    Ieee80211HeMuUserInfo users[];
    bool muMimo = false;
    uint8_t spatialConfiguration = 0;
    uint8_t totalNsts = 0;
}

//
// Delimits the MPDU payload assigned to one RU inside the simulated HE MU
// container packet. It is consumed by the packet-level transmitter/receiver.
//
class Ieee80211HeMuRuPayloadHeader extends FieldsChunk
{
    chunkLength = B(12);
    int ruIndex;
    uint16_t ruToneSize;
    uint16_t ruToneOffset;
    uint16_t staId;
    uint8_t mcs = 0;
    uint8_t numberOfSpatialStreams = 1;
    bool dcm = false;
    B mpduLength;
    bool muMimo = false;
    uint8_t spatialConfiguration = 0;
    uint8_t totalNsts = 0;
    int streamStartIndex = 0;
    double leakageSum = 0;
}
