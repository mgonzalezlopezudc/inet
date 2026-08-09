//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211CHANNEL_H
#define __INET_IEEE80211CHANNEL_H

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Band.h"
#include "inet/physicallayer/wireless/common/contract/bitlevel/ISignalAnalogModel.h"

#include <vector>

namespace inet {

namespace physicallayer {

// Forward declaration of the enum defined in Ieee80211ModeSet.h.
enum class Ieee80211Primary80ChannelPosition;

// IEEE Std 802.11-2024, Table 9-134.
enum Ieee80211SecondaryChannelOffset {
    IEEE80211_SECONDARY_CHANNEL_NONE = 0,
    IEEE80211_SECONDARY_CHANNEL_ABOVE = 1,
    IEEE80211_SECONDARY_CHANNEL_BELOW = 3,
};

/**
 * Models an IEEE 802.11 operating channel: the primary 20 MHz channel and,
 * for channel widths greater than 20 MHz, the position of the primary channel
 * within the bonded operating channel. All subchannel center frequencies
 * (secondary 20/40/80/160 MHz) are derived from the band, the primary channel
 * number, the secondary channel offset, and the operating channel width.
 *
 * For 40 MHz (HT), the operating channel comprises the primary 20 MHz channel
 * and the secondary 20 MHz channel given by the offset. For 80 MHz and above
 * (VHT/HE/EHT), the operating channel additionally comprises the secondary
 * 40 MHz channel; for 160 MHz and above, the secondary 80 MHz channel; and
 * for 320 MHz, the secondary 160 MHz channel. Wide (> 40 MHz) operating
 * For a width-specific contiguous band, the band center is the bonded operating
 * center and primary20SubchannelIndex identifies the primary 20 MHz channel.
 * For a generic band, the primary index is derived from the standard 5 MHz grid.
 */
class INET_API Ieee80211Channel : public IPrintableObject
{
  protected:
    const IIeee80211Band *band;
    int channelNumber;
    Ieee80211SecondaryChannelOffset secondaryChannelOffset;
    Hz operatingChannelWidth;
    Ieee80211Primary80ChannelPosition primary80ChannelPosition;
    Ieee80211Primary80ChannelPosition primary160ChannelPosition;
    int primary20SubchannelIndex;

  protected:
    // Center frequency of the first 20 MHz subchannel of the operating channel.
    Hz getSegmentStartCenterFrequency() const;

  public:
    Ieee80211Channel(const IIeee80211Band *band, int channelNumber);
    Ieee80211Channel(const IIeee80211Band *band, int channelNumber, Ieee80211SecondaryChannelOffset secondaryChannelOffset);
    Ieee80211Channel(const IIeee80211Band *band, int channelNumber, Ieee80211SecondaryChannelOffset secondaryChannelOffset,
            Hz operatingChannelWidth, Ieee80211Primary80ChannelPosition primary80ChannelPosition,
            Ieee80211Primary80ChannelPosition primary160ChannelPosition, int primary20SubchannelIndex = -1);

    static Ieee80211SecondaryChannelOffset parseSecondaryChannelOffset(const char *text);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual const IIeee80211Band *getBand() const { return band; }
    virtual int getChannelNumber() const { return channelNumber; }
    virtual Ieee80211SecondaryChannelOffset getSecondaryChannelOffset() const { return secondaryChannelOffset; }
    virtual Hz getOperatingChannelWidth() const { return operatingChannelWidth; }
    virtual Hz getCenterFrequency() const;
    virtual Hz getSecondaryCenterFrequency() const;
    virtual Hz getBondedCenterFrequency() const;

    /** Number of 20 MHz subchannels in the operating channel (1, 2, 4, 8, or 16). */
    virtual int getNumSubchannels() const { return (int)(operatingChannelWidth.get<MHz>() / 20.0); }
    /** Index of the primary 20 MHz subchannel within the operating channel. */
    virtual int getPrimarySubchannelIndex() const;
    /** Center frequency of the index-th 20 MHz subchannel of the operating channel. */
    virtual Hz getSubchannelCenterFrequency(int index) const;
    /** Center frequency of the secondary 40 MHz channel (requires width >= 80 MHz). */
    virtual Hz getSecondary40CenterFrequency() const;
    /** Center frequency of the secondary 80 MHz channel (requires width >= 160 MHz). */
    virtual Hz getSecondary80CenterFrequency() const;
    /** Center frequency of the secondary 160 MHz channel (requires width == 320 MHz). */
    virtual Hz getSecondary160CenterFrequency() const;
    /**
     * Center frequency of the primary-containing segment of the requested width
     * (20/40/80/160 MHz, not exceeding the operating channel width). A PPDU that
     * is narrower than the operating channel is centered on this frequency.
     */
    virtual Hz getCenterFrequencyForBandwidth(Hz bandwidth) const;
    /** Exact occupied intervals for the requested PPDU bandwidth. */
    virtual std::vector<FrequencySegment> getFrequencySegments(Hz bandwidth) const;
    /** Effective position of the primary 80 MHz channel within a 160 MHz operating channel. */
    virtual Ieee80211Primary80ChannelPosition getPrimary80ChannelPosition() const { return primary80ChannelPosition; }
    /** Effective position of the primary 160 MHz channel within a 320 MHz operating channel. */
    virtual Ieee80211Primary80ChannelPosition getPrimary160ChannelPosition() const { return primary160ChannelPosition; }
    virtual int getPrimary20SubchannelIndex() const { return getPrimarySubchannelIndex(); }
};

} // namespace physicallayer

} // namespace inet

#endif
