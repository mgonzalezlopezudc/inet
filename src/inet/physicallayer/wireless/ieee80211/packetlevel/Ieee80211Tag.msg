//
// Copyright (C) 2006 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

import inet.common.INETDefs;
import inet.common.TagBase;
import inet.physicallayer.wireless.ieee80211.mode.IIeee80211Mode;
import inet.physicallayer.wireless.ieee80211.mode.Ieee80211Channel;

namespace inet::physicallayer;

//
// Receive-side description of one intended HE MU allocation. The tag is used
// by the radio to retain the per-RU context after the common PPDU is decoded.
//
struct Ieee80211HeMuRxAllocationInfo
{
    int ruIndex;
    uint16_t staId;
    uint16_t ruToneSize;
    uint16_t ruToneOffset;
    uint8_t mcs = 0;
    uint8_t numberOfSpatialStreams = 1;
    bool dcm = false;
    bool ndpFeedbackReport = false;
    uint8_t ndpFeedbackStatus = 0;
    uint8_t ndpRuToneSetIndex = 0;
    uint8_t ndpStartingStsNumber = 0;
    simtime_t duration;
}

//
// Transmitter-local description of one PSDU in a downlink HE MU PPDU. The
// radio copies these ordered allocations into the PHY header; this metadata is
// not part of the PSDU and is not delivered through the radio medium.
//
struct Ieee80211HeMuTxAllocationInfo
{
    int ruIndex;
    uint16_t staId;
    uint16_t ruToneSize;
    uint16_t ruToneOffset;
    uint8_t mcs = 0;
    uint8_t numberOfSpatialStreams = 1;
    bool dcm = false;
    bool ndpFeedbackReport = false;
    uint8_t ndpFeedbackStatus = 0;
    uint8_t ndpRuToneSetIndex = 0;
    uint8_t ndpStartingStsNumber = 0;
    B psduLength;
    bool muMimo = false;
    uint8_t spatialConfiguration = 0;
    uint8_t totalNsts = 0;
    int streamStartIndex = 0;
    double leakageSum = 0;
}

enum Ieee80211MpduReceiveStatus
{
    MPDU_SUCCESS = 0;
    MPDU_DELIMITER_ERROR = 1;
    MPDU_HEADER_ERROR = 2;
    MPDU_PAYLOAD_ERROR = 3;
    MPDU_FCS_ERROR = 4;
    MPDU_NOT_EVALUATED = 5;
}

struct Ieee80211MpduReceiveResult
{
    int sequenceNumber = -1;
    int fragmentNumber = -1;
    int tid = -1;
    B offset;
    B length;
    Ieee80211MpduReceiveStatus status = MPDU_NOT_EVALUATED;
}

class Ieee80211MpduReceiveInd extends TagBase
{
    Ieee80211MpduReceiveResult results[];
}

//
// Request for transmitting one user's part of a HE MU or trigger-based PPDU.
// The transmitter combines these tags into a common packet-level PHY header.
//
class Ieee80211HeMuReq extends TagBase
{
    uint8_t ppduFormat = 0;
    uint32_t triggerId = 0;
    uint16_t lSigLength = 0; // Trigger Common Info UL Length for HE TB
    uint16_t channelBandwidthMhz = 20; // authoritative Trigger Common Info UL BW
    bool noSignalExtension = false; // explicit HE-TB TXVECTOR NO_SIG_EXTN
    int ruIndex = -1;
    uint16_t ruToneSize;
    uint16_t ruToneOffset;
    uint16_t staId;
    uint8_t mcs = 0;
    uint8_t numberOfSpatialStreams = 1;
    uint8_t streamStartIndex = 0;
    uint8_t totalNsts = 1;
    bool muMimo = false;
    bool dcm = false;
    bool ndpFeedbackReport = false;
    uint8_t ndpFeedbackStatus = 0;
    uint8_t ndpRuToneSetIndex = 0;
    uint8_t ndpStartingStsNumber = 0;
    uint8_t guardInterval = 2;
    uint8_t ltfType = 4; // authoritative Trigger Common Info HE-LTF type for HE TB
    uint8_t coding = 0;
    uint8_t triggerMethod = 0; // 0: none, 1: Trigger frame, 2: TRS
    bool ldpcExtraSymbolSegment = false; // Trigger Common Info B27 for HE TB
    uint8_t preFecPaddingFactor = 0; // semantic Trigger value a, 1..4
    bool peDisambiguity = false;
    uint8_t numberOfHeLtfSymbols = 0;
    uint8_t packetExtensionDurationUs = 0;
    uint8_t nominalPacketPaddingDurationUs = 0;
    uint8_t puncturedSubchannelMask = 0;
    uint8_t spatialReuse = 0;
    bool txopUnspecified = true;
    uint16_t txopDurationUs = 0;
    bool doppler = false; // HE MU wire field; HE TB Trigger-derived context
    uint8_t midamblePeriodicity = 0;
    bool nonSrgObssPdDisallowed = false;
    bool srgObssPdDisallowed = false;
    bool psrDisallowed = false;
    B psduLength = B(-1);
    simtime_t commonDuration;
    bool commonDurationExact = false;
    W transmitPower = W(NaN);
}

//
// Transmitter-local metadata for a downlink HE MU PPDU. The packet data is
// the concatenation of the per-user PSDUs in allocations[] order. An NDP has
// explicit ndp state and zero packet data; a zero-length non-NDP allocation is
// not interpreted as an NDP.
//
class Ieee80211HeMuTxTag extends TagBase
{
    bool ndp = false;
    simtime_t durationField;
    Ieee80211HeMuTxAllocationInfo allocations[];
}

//
// Common HE MU settings supplied separately from the per-user request when a
// caller only needs to configure guard interval, HE-LTF, coding, extension, or
// puncturing.
//
class Ieee80211HeMuCommonReq extends TagBase
{
    uint8_t guardInterval = 2;
    uint8_t ltfType = 0; // 1, 2, or 4; zero retains the guard-interval default
    uint8_t coding = 0;
    uint8_t packetExtensionDurationUs = 0;
    uint8_t nominalPacketPaddingDurationUs = 0;
    uint8_t puncturedSubchannelMask = 0;
    uint8_t spatialReuse = 0;
    bool txopUnspecified = true;
    uint16_t txopDurationUs = 0;
    bool doppler = false;
    uint8_t midamblePeriodicity = 0;
    bool nonSrgObssPdDisallowed = false;
    bool srgObssPdDisallowed = false;
    bool psrDisallowed = false;
}

//
// Complete transmitter-side policy and calculated outcomes that are missing
// from the ordinary HE SU/ER mode objects. complete=true attests that the
// producer calculated the DATA-field outcomes and total TXTIME according to
// the selected PHY parameters. The radio cross-checks the mode-derived fields,
// validates the logical signaling contract, and stores only the resulting wire
// facts in the concrete PHY header. An incomplete tag is rejected; absence
// retains the transitional legacy path.
//
class Ieee80211HeSuErTxVectorReq extends TagBase
{
    bool complete = false;
    uint8_t ppduFormat = 255; // 2: HE SU, 3: HE ER SU
    B psduLength = B(-1);
    uint8_t suBandwidth = 255; // 0:20, 1:40, 2:80, 3:160, 4:80+80 MHz
    uint8_t erSuRuMode = 255; // 0: primary 242-tone, 1: primary upper 106-tone
    uint8_t mcs = 255;
    uint8_t numberOfSpaceTimeStreams = 0;
    uint8_t guardInterval = 255;
    uint8_t coding = 255;
    simtime_t txTime; // complete standards-derived PPDU TXTIME
    bool uplink = false;
    bool noSignalExtension = false;
    uint8_t ltfType = 0; // 1, 2, or 4
    bool dcm = false;
    bool stbc = false;
    bool txopUnspecified = true;
    uint16_t txopDurationUs = 0;
    bool hasLdpcExtraSymbolSegment = false;
    bool ldpcExtraSymbolSegment = false;
    uint8_t preFecPaddingFactor = 0; // semantic final factor a, 1..4
    uint8_t packetExtensionDurationUs = 0;
    bool doppler = false;
    uint8_t midamblePeriodicity = 0; // 0 without Doppler, otherwise 10 or 20
    bool beamChange = false;
    bool beamformed = false;
    uint8_t bssColor = 0;
    uint8_t spatialReuse = 0;
    uint8_t nominalPacketPaddingDurationUs = 255; // 0, 8, or 16
}

//
// Reception indication containing the HE MU allocations decoded for the
// receiver. The packet-level error model uses this to select the relevant RU.
//
class Ieee80211HeMuRxTag extends TagBase
{
    uint8_t ppduFormat = 0;
    uint32_t triggerId = 0;
    int ruIndex;
    uint8_t guardInterval = 2;
    uint8_t coding = 0;
    uint8_t packetExtensionDurationUs = 0;
    uint8_t puncturedSubchannelMask = 0;
    bool puncturedSubchannelMaskKnown = true;
    Ieee80211HeMuRxAllocationInfo allocations[];
}

//
// Indicates the legacy duration field observed before an HE MU transmission.
// MAC logic uses it to maintain NAV compatibility with HE traffic.
//
class Ieee80211HeMuLegacyPreambleInd extends TagBase
{
    simtime_t durationField;
}


//
// This is an abstract base class that should not be directly added as a tag.
//
class Ieee80211ModeTagBase extends TagBase
{
    const IIeee80211Mode *mode; // IEEE 802.11 transmission mode
}

//
// This request determines the IEEE 802.11 mode that should be used to transmit the packet.
// It may be present on a packet from the application to the phyisical layer.
//
class Ieee80211ModeReq extends Ieee80211ModeTagBase
{
}

//
// This indication specifies the IEEE 802.11 mode that was used to receive the packet.
// It may be present on a packet from the phyiscal layer to the application.
//
class Ieee80211ModeInd extends Ieee80211ModeTagBase
{
}

//
// This is an abstract base class that should not be directly added as a tag.
//
class Ieee80211ChannelTagBase extends TagBase
{
    const Ieee80211Channel *channel; // IEEE 802.11 band and channel
}

//
// This request determines the IEEE 802.11 channel that should be used to transmit the packet.
// It may be present on a packet from the application to the phyisical layer.
//
class Ieee80211ChannelReq extends Ieee80211ChannelTagBase
{
}

//
// This indication specifies the IEEE 802.11 channel that was used to receive the packet.
// It may be present on a packet from the phyiscal layer to the application.
//
class Ieee80211ChannelInd extends Ieee80211ChannelTagBase
{
}

//
// Optional VHT transmission metadata used by the packet-level error model to
// apply the configured beamforming gain or MU-MIMO penalty.
//
class Ieee80211VhtTransmissionTag extends TagBase
{
    bool beamformed = false;
    bool muMimo = false;
    double beamformingGainDb = 3.0;
    double muMimoPenaltyDb = 2.0;
}
