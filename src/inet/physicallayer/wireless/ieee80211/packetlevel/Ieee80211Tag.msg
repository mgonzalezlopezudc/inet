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

struct Ieee80211HeMuRxAllocationInfo
{
    int ruIndex;
    uint16_t staId;
    uint16_t ruToneSize;
    uint16_t ruToneOffset;
    uint8_t mcs = 0;
    uint8_t numberOfSpatialStreams = 1;
    bool dcm = false;
    simtime_t duration;
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

class Ieee80211HeMuReq extends TagBase
{
    uint8_t ppduFormat = 0;
    uint32_t triggerId = 0;
    int ruIndex = -1;
    uint16_t ruToneSize;
    uint16_t ruToneOffset;
    uint16_t staId;
    uint8_t mcs = 0;
    uint8_t numberOfSpatialStreams = 1;
    bool dcm = false;
    uint8_t guardInterval = 2;
    uint8_t coding = 0;
    simtime_t commonDuration;
    W transmitPower = W(NaN);
}

class Ieee80211HeMuCommonReq extends TagBase
{
    uint8_t guardInterval = 2;
    uint8_t coding = 0;
}

class Ieee80211HeMuRxTag extends TagBase
{
    uint8_t ppduFormat = 0;
    uint32_t triggerId = 0;
    int ruIndex;
    uint8_t guardInterval = 2;
    uint8_t coding = 0;
    Ieee80211HeMuRxAllocationInfo allocations[];
}

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
