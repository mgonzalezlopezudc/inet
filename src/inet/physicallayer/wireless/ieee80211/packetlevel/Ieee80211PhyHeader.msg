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

class Ieee80211HtPhyHeader extends Ieee80211PhyHeader
{
    chunkLength = b(48);
    uint8_t mcs;
    bool channelWidth40 = false;
    bool smoothing = true;
    bool notSounding = true;
    bool reserved = true;
    bool aggregation = false;
    uint8_t stbc = 0;
    bool fecCoding = false; // HT-SIG2 B6: false=BCC, true=LDPC
    bool shortGi = false;
    uint8_t numberOfExtensionSpatialStreams = 0;
    uint8_t crc;
}

// IEEE 802.11 21. High Throughput (VHT)
//
class Ieee80211VhtPhyPreamble extends Ieee80211PhyPreamble
{
}

class Ieee80211VhtPhyHeader extends Ieee80211PhyHeader
{
    // VHT-SIG-A is 48 bits. VHT-SIG-B is 26, 27, or 29 bits for
    // 20, 40, or 80/160 MHz respectively (IEEE Std 802.11-2024 Table 21-14).
    chunkLength = b(74);
    uint8_t bandwidth;
    bool reserved1 = true;
    bool stbc = false;
    uint8_t groupId = 63;
    uint8_t numberOfSpaceTimeStreams = 0; // NSTS-1 for the no-STBC SU path
    uint16_t partialAid = 0;
    bool txopPsNotAllowed = true;
    bool reserved2 = true;
    bool shortGi = false;
    bool shortGiNsymDisambiguation = false;
    bool coding = false; // VHT-SIG-A2 B2: false=BCC, true=LDPC
    bool ldpcExtraOfdmSymbol = false;
    uint8_t mcs;
    bool beamformed = false;
    bool reserved3 = true;
    uint8_t crc;
    // VHT-SIG-B Length is in units of four octets and is encoded as
    // ceil(lengthField / 4). On receive, lengthField is reconstructed as
    // four times this value, as specified for RXVECTOR APEP_LENGTH.
    uint32_t vhtSigBLength;
    uint8_t vhtSigBReserved = 7;
    uint8_t vhtSigBTail = 0;
}
