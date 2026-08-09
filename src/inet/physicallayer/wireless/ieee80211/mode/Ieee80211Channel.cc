//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Channel.h"

#include <cmath>

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {

namespace physicallayer {

Ieee80211Channel::Ieee80211Channel(const IIeee80211Band *band, int channelNumber) :
    Ieee80211Channel(band, channelNumber, IEEE80211_SECONDARY_CHANNEL_NONE)
{
}

Ieee80211Channel::Ieee80211Channel(const IIeee80211Band *band, int channelNumber, Ieee80211SecondaryChannelOffset secondaryChannelOffset) :
    Ieee80211Channel(band, channelNumber, secondaryChannelOffset,
            secondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_NONE ? MHz(40) : MHz(20),
            Ieee80211Primary80ChannelPosition::UNSPECIFIED, Ieee80211Primary80ChannelPosition::UNSPECIFIED)
{
}

Ieee80211Channel::Ieee80211Channel(const IIeee80211Band *band, int channelNumber, Ieee80211SecondaryChannelOffset secondaryChannelOffset,
        Hz operatingChannelWidth, Ieee80211Primary80ChannelPosition primary80ChannelPosition,
        Ieee80211Primary80ChannelPosition primary160ChannelPosition, int primary20SubchannelIndex) :
    band(band),
    channelNumber(channelNumber),
    secondaryChannelOffset(secondaryChannelOffset),
    operatingChannelWidth(operatingChannelWidth),
    primary80ChannelPosition(primary80ChannelPosition),
    primary160ChannelPosition(primary160ChannelPosition),
    primary20SubchannelIndex(primary20SubchannelIndex)
{
    // IEEE Std 802.11-2024, Table 9-134 reserves wire value 2; 19.2.3 and
    // 19.3.15.4 place the secondary channel four channel numbers away.
    if (secondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_NONE &&
            secondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_ABOVE &&
            secondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_BELOW)
        throw cRuntimeError("Invalid IEEE 802.11 secondary channel offset: %d", (int)secondaryChannelOffset);
    int numSubchannels = (int)std::round(operatingChannelWidth.get<MHz>() / 20.0);
    if (numSubchannels != 1 && numSubchannels != 2 && numSubchannels != 4 && numSubchannels != 8 && numSubchannels != 16)
        throw cRuntimeError("Invalid IEEE 802.11 operating channel width: %g MHz", operatingChannelWidth.get() / 1e6);
    bool nonContiguous = band != nullptr && band->getChannelTopology() == Ieee80211ChannelTopology::NONCONTIGUOUS;
    if (numSubchannels == 1 && secondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_NONE)
        throw cRuntimeError("IEEE 802.11 20 MHz operation does not have a secondary channel");
    if (numSubchannels >= 2 && secondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_NONE && !nonContiguous)
        throw cRuntimeError("IEEE 802.11 %d MHz operation requires a secondary channel offset of above or below", numSubchannels * 20);
    if (secondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_NONE) {
        if (band == nullptr)
            throw cRuntimeError("Cannot resolve an IEEE 802.11 secondary channel without a band");
        (void)getCenterFrequency();
        (void)getSecondaryCenterFrequency();
    }
    if (numSubchannels >= 4 && !nonContiguous) {
        // Generic wide channels use the 5 MHz standard grid. Width-specific
        // contiguous bands already identify the bonded operating center.
        bool widthSpecificBand = band != nullptr &&
                band->getChannelTopology() == Ieee80211ChannelTopology::CONTIGUOUS &&
                band->getChannelWidth() == operatingChannelWidth;
        if (band == nullptr || (!widthSpecificBand && band->getSpacing() != MHz(5)) ||
                (band->getBandFamily() != Ieee80211BandFamily::BAND_5_GHZ && band->getBandFamily() != Ieee80211BandFamily::BAND_6_GHZ))
            throw cRuntimeError("IEEE 802.11 %d MHz operation requires a contiguous 5 GHz or 6 GHz band", numSubchannels * 20);
        // Computing the primary subchannel index validates the channel grid
        // alignment and the secondary channel offset parity.
        (void)getPrimarySubchannelIndex();
    }
    if (numSubchannels >= 8) {
        // IEEE Std 802.11-2024, 21.2.2: the primary 80 MHz channel is the half
        // of the 160 MHz operating channel that contains the primary 40 MHz channel.
        auto derived = getPrimarySubchannelIndex() % 8 < 4 ? Ieee80211Primary80ChannelPosition::LOWER : Ieee80211Primary80ChannelPosition::UPPER;
        if (primary80ChannelPosition == Ieee80211Primary80ChannelPosition::UNSPECIFIED)
            this->primary80ChannelPosition = derived;
        else if (primary80ChannelPosition != derived)
            throw cRuntimeError("The primary80ChannelPosition parameter is inconsistent with the primary channel number");
    }
    if (numSubchannels == 16) {
        // The primary 160 MHz channel is the half of the 320 MHz operating
        // channel that contains the primary 80 MHz channel.
        auto derived = getPrimarySubchannelIndex() < 8 ? Ieee80211Primary80ChannelPosition::LOWER : Ieee80211Primary80ChannelPosition::UPPER;
        if (primary160ChannelPosition == Ieee80211Primary80ChannelPosition::UNSPECIFIED)
            this->primary160ChannelPosition = derived;
        else if (primary160ChannelPosition != derived)
            throw cRuntimeError("The primary160ChannelPosition parameter is inconsistent with the primary channel number");
    }
}

Ieee80211SecondaryChannelOffset Ieee80211Channel::parseSecondaryChannelOffset(const char *text)
{
    if (!strcmp(text, "none"))
        return IEEE80211_SECONDARY_CHANNEL_NONE;
    if (!strcmp(text, "above"))
        return IEEE80211_SECONDARY_CHANNEL_ABOVE;
    if (!strcmp(text, "below"))
        return IEEE80211_SECONDARY_CHANNEL_BELOW;
    throw cRuntimeError("Unknown IEEE 802.11 secondary channel offset '%s'", text);
}

Hz Ieee80211Channel::getSecondaryCenterFrequency() const
{
    int direction = secondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_ABOVE ? 1 :
            secondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_BELOW ? -1 : 0;
    if (direction == 0)
        throw cRuntimeError("IEEE 802.11 channel has no secondary channel");
    bool widthSpecificBand = band->getChannelTopology() == Ieee80211ChannelTopology::CONTIGUOUS &&
            band->getChannelWidth() == operatingChannelWidth && getNumSubchannels() >= 2;
    Hz secondaryCenterFrequency = widthSpecificBand ? getCenterFrequency() + MHz(20 * direction) :
            band->getCenterFrequency(channelNumber + 4 * direction);
    if (secondaryCenterFrequency != getCenterFrequency() + MHz(20 * direction))
        throw cRuntimeError("IEEE 802.11 secondary channel is not 20 MHz from the primary channel");
    return secondaryCenterFrequency;
}

int Ieee80211Channel::getPrimarySubchannelIndex() const
{
    int numSubchannels = getNumSubchannels();
    if (numSubchannels == 1)
        return 0;
    if (band->getChannelTopology() == Ieee80211ChannelTopology::NONCONTIGUOUS)
        return 0;
    bool widthSpecificBand = band->getChannelTopology() == Ieee80211ChannelTopology::CONTIGUOUS &&
            band->getChannelWidth() == operatingChannelWidth;
    if (primary20SubchannelIndex >= 0) {
        if (primary20SubchannelIndex >= numSubchannels)
            throw cRuntimeError("Primary 20 MHz subchannel index %d is out of range for %d MHz operation",
                    primary20SubchannelIndex, numSubchannels * 20);
        bool lowerPrimary = (primary20SubchannelIndex & 1) == 0;
        if (lowerPrimary != (secondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_ABOVE))
            throw cRuntimeError("The secondary channel offset is inconsistent with primary20SubchannelIndex %d",
                    primary20SubchannelIndex);
        if (!widthSpecificBand && numSubchannels >= 4) {
            Hz anchor = band->getCenterFrequency(band->getBandFamily() == Ieee80211BandFamily::BAND_5_GHZ ? 36 : 1);
            double subchannels = (band->getCenterFrequency(channelNumber) - anchor).get<MHz>() / 20.0;
            int gridIndex = (int)std::round(subchannels);
            if (std::fabs(subchannels - gridIndex) > 1e-9 || gridIndex < 0 || gridIndex % numSubchannels != primary20SubchannelIndex)
                throw cRuntimeError("primary20SubchannelIndex %d is inconsistent with channel number %d on the generic channel grid",
                        primary20SubchannelIndex, channelNumber);
        }
        return primary20SubchannelIndex;
    }
    if (numSubchannels == 2)
        // The secondary 20 MHz channel is adjacent to the primary channel, so
        // the offset alone determines the position within the 40 MHz channel.
        return secondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_ABOVE ? 0 : 1;
    if (widthSpecificBand) {
        int index = 0;
        if (numSubchannels == 16 && primary160ChannelPosition == Ieee80211Primary80ChannelPosition::UPPER)
            index += 8;
        if (numSubchannels >= 8 && primary80ChannelPosition == Ieee80211Primary80ChannelPosition::UPPER)
            index += 4;
        index = (index & ~1) + (secondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_BELOW ? 1 : 0);
        return index;
    }
    // Wider operating channels are aligned to the standard channel grid: the
    // 5 GHz grid is anchored at channel 36 and the 6 GHz grid at channel 1.
    Hz anchor = band->getCenterFrequency(band->getBandFamily() == Ieee80211BandFamily::BAND_5_GHZ ? 36 : 1);
    double subchannels = (getCenterFrequency() - anchor).get<MHz>() / 20.0;
    int index = (int)std::round(subchannels);
    if (std::fabs(subchannels - index) > 1e-9 || index < 0)
        throw cRuntimeError("IEEE 802.11 channel number %d is not a valid primary 20 MHz channel for %d MHz operation",
                channelNumber, numSubchannels * 20);
    index %= numSubchannels;
    // Within each 40 MHz channel of the grid, an above offset means the
    // primary channel is the lower 20 MHz subchannel, and vice versa.
    bool lowerPrimary = (index & 1) == 0;
    if (lowerPrimary != (secondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_ABOVE))
        throw cRuntimeError("The secondary channel offset is inconsistent with the position of channel %d within the %d MHz operating channel",
                channelNumber, numSubchannels * 20);
    return index;
}

Hz Ieee80211Channel::getSegmentStartCenterFrequency() const
{
    bool widthSpecificBand = band->getChannelTopology() == Ieee80211ChannelTopology::CONTIGUOUS &&
            band->getChannelWidth() == operatingChannelWidth && getNumSubchannels() > 1;
    return widthSpecificBand ? band->getCenterFrequency(channelNumber) - MHz(10 * (getNumSubchannels() - 1)) :
            getCenterFrequency() - MHz(20 * getPrimarySubchannelIndex());
}

Hz Ieee80211Channel::getBondedCenterFrequency() const
{
    int numSubchannels = getNumSubchannels();
    if (numSubchannels == 1)
        return getCenterFrequency();
    if (band->getChannelTopology() == Ieee80211ChannelTopology::CONTIGUOUS &&
            band->getChannelWidth() == operatingChannelWidth)
        return band->getCenterFrequency(channelNumber);
    return getSegmentStartCenterFrequency() + MHz(10 * (numSubchannels - 1));
}

Hz Ieee80211Channel::getCenterFrequency() const
{
    if (band == nullptr)
        throw cRuntimeError("IEEE 802.11 channel has no band");
    if (getNumSubchannels() > 1 && band->getChannelTopology() == Ieee80211ChannelTopology::CONTIGUOUS &&
            band->getChannelWidth() == operatingChannelWidth)
        return band->getCenterFrequency(channelNumber) - MHz(10 * (getNumSubchannels() - 1)) +
                MHz(20 * getPrimarySubchannelIndex());
    return band->getCenterFrequency(channelNumber);
}

Hz Ieee80211Channel::getSubchannelCenterFrequency(int index) const
{
    if (index < 0 || index >= getNumSubchannels())
        throw cRuntimeError("IEEE 802.11 subchannel index %d is out of range", index);
    if (band->getChannelTopology() == Ieee80211ChannelTopology::NONCONTIGUOUS) {
        auto segmentCenter = index < 4 ? getCenterFrequency() : getSecondary80CenterFrequency();
        return segmentCenter + MHz(20 * (index % 4 - 1.5));
    }
    return getSegmentStartCenterFrequency() + MHz(20 * index);
}

Hz Ieee80211Channel::getSecondary40CenterFrequency() const
{
    int numSubchannels = getNumSubchannels();
    if (numSubchannels < 4)
        throw cRuntimeError("IEEE 802.11 channel has no secondary 40 MHz channel");
    // The secondary 40 MHz channel is the other 40 MHz half of the primary 80 MHz channel.
    int primaryIndex = getPrimarySubchannelIndex();
    int firstSubchannel = (primaryIndex & ~3) + ((((primaryIndex >> 1) & 1) ^ 1) << 1);
    return getSegmentStartCenterFrequency() + MHz(20 * firstSubchannel) + MHz(10);
}

Hz Ieee80211Channel::getSecondary80CenterFrequency() const
{
    int numSubchannels = getNumSubchannels();
    if (numSubchannels < 8)
        throw cRuntimeError("IEEE 802.11 channel has no secondary 80 MHz channel");
    if (band->getChannelTopology() == Ieee80211ChannelTopology::NONCONTIGUOUS) {
        if (band->getNumChannels() != 2)
            throw cRuntimeError("IEEE 802.11 non-contiguous operation requires two 80 MHz segments");
        return band->getCenterFrequency(channelNumber == 0 ? 1 : 0);
    }
    // The secondary 80 MHz channel is the other 80 MHz half of the primary 160 MHz channel.
    int primaryIndex = getPrimarySubchannelIndex();
    int firstSubchannel = (primaryIndex & ~7) + ((((primaryIndex >> 2) & 1) ^ 1) << 2);
    return getSegmentStartCenterFrequency() + MHz(20 * firstSubchannel) + MHz(30);
}

Hz Ieee80211Channel::getSecondary160CenterFrequency() const
{
    int numSubchannels = getNumSubchannels();
    if (numSubchannels < 16)
        throw cRuntimeError("IEEE 802.11 channel has no secondary 160 MHz channel");
    // The secondary 160 MHz channel is the half of the 320 MHz operating
    // channel that does not contain the primary 160 MHz channel.
    int firstSubchannel = (getPrimarySubchannelIndex() < 8) ? 8 : 0;
    return getSegmentStartCenterFrequency() + MHz(20 * firstSubchannel) + MHz(70);
}

Hz Ieee80211Channel::getCenterFrequencyForBandwidth(Hz bandwidth) const
{
    int numSubchannels = (int)(bandwidth.get<MHz>() / 20.0);
    if (numSubchannels != 1 && numSubchannels != 2 && numSubchannels != 4 && numSubchannels != 8 && numSubchannels != 16)
        throw cRuntimeError("Invalid IEEE 802.11 PPDU bandwidth: %g MHz", bandwidth.get<MHz>());
    if (numSubchannels > getNumSubchannels())
        throw cRuntimeError("IEEE 802.11 PPDU bandwidth (%g MHz) exceeds the operating channel width (%g MHz)",
                bandwidth.get() / 1e6, operatingChannelWidth.get() / 1e6);
    if (band->getChannelTopology() == Ieee80211ChannelTopology::NONCONTIGUOUS)
        return getCenterFrequency();
    int firstSubchannel = getPrimarySubchannelIndex() & ~(numSubchannels - 1);
    return getSegmentStartCenterFrequency() + MHz(20 * firstSubchannel) + MHz(10 * (numSubchannels - 1));
}

std::vector<FrequencySegment> Ieee80211Channel::getFrequencySegments(Hz bandwidth) const
{
    if (bandwidth <= MHz(0) || bandwidth > operatingChannelWidth)
        throw cRuntimeError("Requested PPDU bandwidth %s is invalid for operating channel %s", bandwidth.str().c_str(), operatingChannelWidth.str().c_str());
    if (band->getChannelTopology() == Ieee80211ChannelTopology::NONCONTIGUOUS && bandwidth >= MHz(160))
        return {{getCenterFrequency(), MHz(80)}, {getSecondary80CenterFrequency(), MHz(80)}};
    return {{getCenterFrequencyForBandwidth(bandwidth), bandwidth}};
}

std::ostream& Ieee80211Channel::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "Ieee80211Channel";
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(band, printFieldToString(band, level + 1, evFlags));
    if (level <= PRINT_LEVEL_INFO)
        stream << EV_FIELD(channelNumber)
               << EV_FIELD(secondaryChannelOffset)
               << EV_FIELD(operatingChannelWidth);
    return stream;
}

} // namespace physicallayer

} // namespace inet
