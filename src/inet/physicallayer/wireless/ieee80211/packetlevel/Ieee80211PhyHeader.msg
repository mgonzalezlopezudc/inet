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
    // Common packet-level PSDU length metadata used by the radio model. Some
    // PHY variants serialize it as an on-wire LENGTH field, while others do not.
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
    // The legacy IR PHY PLCP header is not fully modeled at bit level here.
    // The inherited lengthField remains packet-level metadata, not serialized
    // as an IR on-wire LENGTH field by the current serializer.
    uint16_t fcs;
    ChecksumMode fcsMode = FCS_MODE_UNDEFINED;  // should be FcsMode, but OMMeT++ bug #1351 prevents that (fixed in OMNeT++ 6.2)
}

//
// IEEE Std 802.11-2024 Clause 15 DSSS PHY
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
    // IEEE Std 802.11-2024 15.3.3.6 defines PLCP LENGTH in microseconds.
    // INET keeps the TXVECTOR LENGTH octet count here; see the serializer
    // comment for the non-bit-exact DSSS/HR-DSSS conversion gap.
    uint16_t fcs;
    ChecksumMode fcsMode = FCS_MODE_UNDEFINED;  // should be FcsMode, but OMMeT++ bug #1351 prevents that (fixed in OMNeT++ 6.2)
}

//
// IEEE Std 802.11-2024 Clause 16 High Rate DSSS PHY (HR/DSSS)
//
class Ieee80211HrDsssPhyPreamble extends Ieee80211DsssPhyPreamble
{
}

class Ieee80211HrDsssPhyHeader extends Ieee80211DsssPhyHeader
{
}

//
// IEEE Std 802.11-2024 Clause 17 Orthogonal Frequency Division Multiplexing (OFDM) PHY
//
class Ieee80211OfdmPhyPreamble extends Ieee80211PhyPreamble
{
}

// The length field is a 12-bit integer indicates the number of
// octets in the PSDU that the MAC is currently requesting the PHY to
// transmit.
//
// The rate field (17.3.4.2 RATE field) is a 4-bit integer carries
// information about the modulation and coding.
//
// There are other fields which are not explicitly represented here,
// for e.g. Parity, Reserved, SERVICE, Pad Bits.
//
// The PSDU field must be contained as an encapsulated packet.
//
// This packet is intended to represent the whole PPDU frame in the
// IEEE 802.11 PHY. For detailed description, see IEEE Std 802.11-2024
// 17.3.2 PLCP frame format and Figure 17-1 PPDU frame format.
//
class Ieee80211OfdmPhyHeader extends Ieee80211PhyHeader
{
    // The PLCP header is composed of RATE (4), Reserved (1), LENGTH (12), Parity (1),
    // Tail (6) and SERVICE (16) fields.
    chunkLength = B(5);
    uint8_t rate;
    bool reserved = false;
    // The inherited lengthField is serialized as the 12-bit SIGNAL LENGTH field.
    bool parity = false;
    uint8_t tail = 0;
    uint16_t service;
}

//
// IEEE Std 802.11-2024 Clause 18 Extended Rate PHY (ERP) OFDM
//
class Ieee80211ErpOfdmPhyPreamble extends Ieee80211OfdmPhyPreamble
{
}

class Ieee80211ErpOfdmPhyHeader extends Ieee80211OfdmPhyHeader
{
}

//
// IEEE Std 802.11-2024 Clause 19 High Throughput (HT) PHY
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

// IEEE Std 802.11-2024 Clause 21 Very High Throughput (VHT) PHY
//
class Ieee80211VhtPhyPreamble extends Ieee80211PhyPreamble
{
}

// Canonical per-user logical VHT MU signaling. Model-only receiver identity,
// gain, leakage, and PSDU ranges remain in Ieee80211VhtTxVector; this struct
// carries the VHT-SIG-A/B fields needed by the packet-level serializer.
struct Ieee80211VhtMuUserInfo
{
    uint16_t associationId;
    uint8_t userPosition;
    uint8_t numberOfSpatialStreams = 1;
    uint8_t mcs = 0;
    uint8_t coding = 0;
    B psduLength;
    simtime_t duration;
}

//
// VHT PHY header modelled at packet level. coding selects BCC (0) or LDPC (1)
// for the associated VHT data field.
//
class Ieee80211VhtPhyHeader extends Ieee80211PhyHeader
{
    chunkLength = b(50); // Ieee80211VhtSignalMode packet-level field budget
    bool signalingValid = false;
    bool ndp = false;
    bool stbc = false;
    uint8_t bandwidth = 0; // Table 21-11: 0=20, 1=40, 2=80, 3=160 or 80+80 MHz
    uint8_t groupId = 63;
    uint8_t numberOfSpaceTimeStreams = 1;
    uint16_t partialAid = 0;
    uint8_t mcs = 0;
    uint8_t coding = 0; // 0: BCC, 1: LDPC
    bool shortGi = false; // Table 21-12: common GI for all VHT MU users
    bool ldpcExtraOfdmSymbol = false; // VHT-SIG-A LDPC Extra OFDM Symbol
    bool beamformed = false;
    simtime_t commonDuration;
    Ieee80211VhtMuUserInfo users[];
}

//
// IEEE 802.11 22. High Efficiency (HE)
//
//
// Per-user HE allocation carried by an Ieee80211HePhyHeader. The RU
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
    uint8_t coding = 0; // HE-SIG-B per-user BCC/LDPC field
    bool ndpFeedbackReport = false;
    uint8_t ndpFeedbackStatus = 0;
    uint8_t ndpRuToneSetIndex = 0;
    uint8_t ndpStartingStsNumber = 0;
    bool muMimo = false;
    uint8_t spatialConfiguration = 0;
    B psduLength;
    simtime_t duration;
    int streamStartIndex = 0;
}

// Validated logical L-SIG and HE-SIG-A values for an HE MU PPDU. HE-SIG-B
// Common/User blocks are derived from these fields and the ordered users[]
// allocation by the logical codec; model-only PSDU ranges are not serialized.
struct Ieee80211HeMuSignalingFields
{
    bool signalingValid = false;
    uint16_t lSigLength = 0;
    uint8_t heSigBMcs = 0;
    bool heSigBDcm = false;
    uint8_t bandwidth = 0;
    bool heSigBCompression = false;
    uint8_t numberOfHeSigBSymbols = 0;
    bool numberOfHeSigBSymbolsIsSaturated = false;
    uint8_t numberOfMuMimoUsers = 0;
    uint8_t giLtfSize = 0;
    bool doppler = false;
    uint8_t midamblePeriodicity = 0;
    uint8_t txop = 127;
    uint8_t numberOfHeLtfSymbols = 1;
    bool ldpcExtraSymbolSegment = false;
    bool stbc = false;
    uint8_t preFecPaddingFactor = 0;
    bool peDisambiguity = false;
}

// Validated logical L-SIG and HE-SIG-A values for an HE TB PPDU. RU, MCS,
// FEC, DCM, NSTS, and Doppler remain Trigger-derived context and are not
// invented as HE-SIG-A fields.
struct Ieee80211HeTbSignalingFields
{
    bool signalingValid = false;
    uint16_t lSigLength = 0;
    uint8_t spatialReuse1 = 0;
    uint8_t spatialReuse2 = 0;
    uint8_t spatialReuse3 = 0;
    uint8_t spatialReuse4 = 0;
    uint8_t bandwidth = 0;
    uint8_t txop = 127;
    uint16_t triggerReserved = 511;
}

//
// Validated logical L-SIG and HE-SIG-A values retained by concrete HE SU and
// HE ER SU headers. These are wire facts, not transmitter policy inputs. The
// serializer emits the exact 100 logical L-SIG/RL-SIG/HE-SIG-A bits only when
// signalingValid is true; false distinguishes the transitional untagged mode
// path that retains the legacy packet-level encoding.
//
struct Ieee80211HeSuErSignalingFields
{
    bool signalingValid = false;
    uint16_t lSigLength = 0;
    bool beamChange = false;
    bool uplink = false;
    uint8_t mcs = 0;
    bool dcm = false;
    uint8_t bssColor = 0;
    uint8_t spatialReuse = 0;
    uint8_t bandwidth = 0;
    uint8_t giLtfSize = 0;
    uint8_t numberOfSpaceTimeStreams = 1;
    uint8_t midamblePeriodicity = 0;
    uint8_t txop = 127;
    bool ldpcCoding = false;
    bool ldpcExtraSymbolSegment = false;
    bool stbc = false;
    bool beamformed = false;
    uint8_t preFecPaddingFactor = 0;
    bool peDisambiguity = false;
    bool doppler = false;
}

//
// Packet-level common header of an HE PPDU. The serializer emits the HE
// signaling fields only; runtime-only model
// state such as trigger correlation and resolved per-user durations stays in
// the chunk object and is not part of the serialized PHY header.
//
class Ieee80211HePhyHeader extends Ieee80211PhyHeader
{
    uint8_t bssColor = 0;
    uint8_t spatialReuse = 0; // HE-SIG-A Spatial Reuse field
    bool nonSrgObssPdDisallowed = false;
    bool srgObssPdDisallowed = false;
    bool psrDisallowed = false;
    uint8_t guardInterval = 2; // 0: 0.8 us, 1: 1.6 us, 2: 3.2 us
    uint8_t coding = 0; // 0: BCC, 1: LDPC
    uint8_t packetExtensionDurationUs = 0;
    uint8_t puncturedSubchannelMask = 0; // 20 MHz subchannels, LSB is primary
    bool puncturedSubchannelMaskKnown = true; // false when wire bandwidth code/common allocation is ambiguous
    uint32_t triggerId = 0;
    simtime_t commonDuration;
    bool ndp = false; // runtime-only: an NDP has no Data field
    Ieee80211HeMuUserInfo users[];
    bool muMimo = false;
    uint8_t spatialConfiguration = 0;
    uint8_t totalNsts = 0;
}

class Ieee80211HeSuPhyHeader extends Ieee80211HePhyHeader
{
    Ieee80211HeSuErSignalingFields signaling;
}

class Ieee80211HeErSuPhyHeader extends Ieee80211HePhyHeader
{
    Ieee80211HeSuErSignalingFields signaling;
}

class Ieee80211HeMuPhyHeader extends Ieee80211HePhyHeader
{
    Ieee80211HeMuSignalingFields signaling;
}

class Ieee80211HeTbPhyHeader extends Ieee80211HePhyHeader
{
    Ieee80211HeTbSignalingFields signaling;
}

//
// IEEE 802.11 36. Extremely High Throughput (EHT) PHY
//
class Ieee80211EhtPhyPreamble extends Ieee80211PhyPreamble
{
}

struct Ieee80211EhtUserInfo
{
    int mruIndex;
    uint16_t mruToneSize;
    uint16_t mruToneOffset;
    uint16_t staId;
    uint8_t mcs = 0;
    uint8_t numberOfSpatialStreams = 1;
    B psduLength;
    simtime_t duration;
    int streamStartIndex = 0;
    double leakageSum = 0;
}

//
// Logical U-SIG fields needed to distinguish an EHT SU transmission while
// retaining the EHT MU PPDU format. IEEE Std 802.11be-2024 Table 36-28 uses
// PPDU Type And Compression Mode value 1 for EHT SU.
//
struct Ieee80211EhtUSigFields
{
    bool signalingValid = false;
    uint8_t ppduTypeAndCompressionMode = 0;
}

class Ieee80211EhtPhyHeader extends Ieee80211PhyHeader
{
    uint8_t bssColor = 0;
    uint8_t ppduFormat = 0; // 0: EHT MU, 1: EHT TB (Trigger-Based)
    uint8_t spatialReuse = 0; 
    uint8_t guardInterval = 2; // 0: 0.8 us, 1: 1.6 us, 2: 3.2 us
    uint8_t coding = 0; // 0: BCC, 1: LDPC
    uint8_t packetExtensionDurationUs = 0;
    uint16_t puncturedSubchannelMask = 0; // 20 MHz subchannels
    uint32_t triggerId = 0;
    Ieee80211EhtUSigFields uSig;
    simtime_t commonDuration;
    Ieee80211EhtUserInfo users[];
    bool muMimo = false;
    uint8_t spatialConfiguration = 0;
    uint8_t totalNsts = 0;
}

class Ieee80211EhtRuPayloadHeader extends FieldsChunk
{
    chunkLength = B(12);
    int mruIndex;
    uint16_t mruToneSize;
    uint16_t mruToneOffset;
    uint16_t staId;
    uint8_t mcs = 0;
    uint8_t numberOfSpatialStreams = 1;
    B mpduLength;
    bool muMimo = false;
    uint8_t spatialConfiguration = 0;
    uint8_t totalNsts = 0;
    int streamStartIndex = 0;
    double leakageSum = 0;
}
