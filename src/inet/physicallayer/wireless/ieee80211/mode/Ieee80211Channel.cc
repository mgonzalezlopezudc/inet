//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Channel.h"

#include <algorithm>
#include <cmath>

namespace inet {

namespace physicallayer {

namespace {

enum class BandFamily {
    OTHER,
    BAND_2_4_GHZ,
    BAND_5_GHZ,
    BAND_5_9_GHZ,
};

BandFamily getBandFamily(const IIeee80211Band *band)
{
    if (band == nullptr)
        return BandFamily::OTHER;
    const char *name = band->getName();
    if (!strncmp(name, "5 GHz", 5))
        return BandFamily::BAND_5_GHZ;
    if (!strncmp(name, "2.4 GHz", 7))
        return BandFamily::BAND_2_4_GHZ;
    if (!strncmp(name, "5.9 GHz", 7))
        return BandFamily::BAND_5_9_GHZ;
    return BandFamily::OTHER;
}

bool isCanonical5GHzVht80CenterIndex(int index)
{
    // IEEE Std 802.11-2024, Table 21-22: 5 GHz VHT 80 MHz center
    // frequency indices.  The ordinary INET 5 GHz band uses the same
    // 5 MHz center-frequency index space.
    return index == 42 || index == 58 || index == 106 || index == 122 || index == 138 || index == 155;
}

bool isCanonical5GHzVht160CenterIndex(int index)
{
    // IEEE Std 802.11-2024, Table 21-22: contiguous 160 MHz centers.
    return index == 50 || index == 114;
}

bool isCanonical5GHzVht20CenterIndex(int index)
{
    // IEEE Std 802.11-2024, Table 21-22, 5 GHz VHT 20 MHz channel indices
    // represented by the INET 5 MHz center-frequency index space.
    static const int indices[] = {
        36, 40, 44, 48, 52, 56, 60, 64,
        100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
        149, 153, 157, 161, 165,
    };
    return std::find(std::begin(indices), std::end(indices), index) != std::end(indices);
}

bool isCanonical5GHzBand(const IIeee80211Band *band)
{
    return getBandFamily(band) == BandFamily::BAND_5_GHZ;
}

void validateCanonicalVht20Subchannels(int centerFrequencyIndex, const std::initializer_list<int>& offsets)
{
    for (int offset : offsets) {
        int index = centerFrequencyIndex + offset;
        if (!isCanonical5GHzVht20CenterIndex(index))
            throw cRuntimeError("Invalid 5 GHz VHT 20 MHz subchannel center frequency index %d", index);
    }
}

} // namespace

Ieee80211Channel::Ieee80211Channel(const IIeee80211Band *band, int channelNumber) :
    Ieee80211Channel(band, channelNumber, IEEE80211_SECONDARY_CHANNEL_NONE)
{
}

Ieee80211Channel::Ieee80211Channel(const IIeee80211Band *band, int channelNumber, Ieee80211SecondaryChannelOffset secondaryChannelOffset) :
    band(band),
    channelNumber(channelNumber),
    secondaryChannelOffset(secondaryChannelOffset),
    channelWidth(secondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_NONE ? IEEE80211_CHANNEL_WIDTH_20MHZ : IEEE80211_CHANNEL_WIDTH_40MHZ),
    centerFrequencyIndex0(-1),
    centerFrequencyIndex1(0),
    explicitGeometry(false)
{
    // IEEE Std 802.11-2024, Table 9-134 reserves wire value 2; 19.2.3 and
    // 19.3.15.4 place the secondary channel four channel numbers away.
    if (secondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_NONE &&
            secondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_ABOVE &&
            secondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_BELOW)
        throw cRuntimeError("Invalid IEEE 802.11 secondary channel offset: %d", (int)secondaryChannelOffset);
    if (secondaryChannelOffset != IEEE80211_SECONDARY_CHANNEL_NONE) {
        if (band == nullptr)
            throw cRuntimeError("Cannot resolve an IEEE 802.11 secondary channel without a band");
        (void)getCenterFrequency();
        (void)getSecondaryCenterFrequency();
    }
}

Ieee80211Channel::Ieee80211Channel(const IIeee80211Band *band, int primaryChannelNumber, Ieee80211ChannelWidth channelWidth, int centerFrequencyIndex0, int centerFrequencyIndex1) :
    band(band),
    channelNumber(primaryChannelNumber),
    secondaryChannelOffset(IEEE80211_SECONDARY_CHANNEL_NONE),
    channelWidth(channelWidth),
    centerFrequencyIndex0(centerFrequencyIndex0 == -1 ? primaryChannelNumber : centerFrequencyIndex0),
    centerFrequencyIndex1(centerFrequencyIndex1),
    explicitGeometry(true)
{
    if (band == nullptr)
        throw cRuntimeError("Cannot construct an IEEE 802.11 channel without a band");
    if (primaryChannelNumber < 0 || this->centerFrequencyIndex0 < 0 || this->centerFrequencyIndex1 < 0)
        throw cRuntimeError("IEEE 802.11 channel indices must be non-negative");
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ && centerFrequencyIndex1 == 0)
        throw cRuntimeError("VHT 80+80 MHz operation requires center frequency index 1");
    validateGeometry();
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_40MHZ)
        secondaryChannelOffset = getCenterFrequency() < getCenterFrequencyForIndex(this->centerFrequencyIndex0) ?
                IEEE80211_SECONDARY_CHANNEL_ABOVE : IEEE80211_SECONDARY_CHANNEL_BELOW;
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

const char *Ieee80211Channel::getSecondaryChannelOffsetName(Ieee80211SecondaryChannelOffset offset)
{
    switch (offset) {
        case IEEE80211_SECONDARY_CHANNEL_NONE:
            return "none";
        case IEEE80211_SECONDARY_CHANNEL_ABOVE:
            return "above";
        case IEEE80211_SECONDARY_CHANNEL_BELOW:
            return "below";
        default:
            throw cRuntimeError("Unknown IEEE 802.11 secondary channel offset: %d", (int)offset);
    }
}

Ieee80211ChannelWidth Ieee80211Channel::parseChannelWidth(const char *text)
{
    if (!strcmp(text, "20MHz"))
        return IEEE80211_CHANNEL_WIDTH_20MHZ;
    if (!strcmp(text, "40MHz"))
        return IEEE80211_CHANNEL_WIDTH_40MHZ;
    if (!strcmp(text, "80MHz"))
        return IEEE80211_CHANNEL_WIDTH_80MHZ;
    if (!strcmp(text, "160MHz"))
        return IEEE80211_CHANNEL_WIDTH_160MHZ;
    if (!strcmp(text, "80+80MHz"))
        return IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ;
    throw cRuntimeError("Unknown IEEE 802.11 channel width '%s'", text);
}

const char *Ieee80211Channel::getChannelWidthName(Ieee80211ChannelWidth width)
{
    switch (width) {
        case IEEE80211_CHANNEL_WIDTH_20MHZ: return "20MHz";
        case IEEE80211_CHANNEL_WIDTH_40MHZ: return "40MHz";
        case IEEE80211_CHANNEL_WIDTH_80MHZ: return "80MHz";
        case IEEE80211_CHANNEL_WIDTH_160MHZ: return "160MHz";
        case IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ: return "80+80MHz";
        default: throw cRuntimeError("Unknown IEEE 802.11 channel width: %d", (int)width);
    }
}

Hz Ieee80211Channel::getCenterFrequencyForIndex(int index) const
{
    if (!explicitGeometry)
        return band->getCenterFrequency(index);

    // The explicit VHT indices are IEEE center-frequency indices (5 MHz
    // spacing), while the legacy band API uses zero-based array indices.
    switch (getBandFamily(band)) {
        case BandFamily::BAND_5_GHZ:
            return GHz(5) + MHz(5 * index);
        case BandFamily::BAND_5_9_GHZ:
            return GHz(5.855) + MHz(10 * index);
        case BandFamily::BAND_2_4_GHZ:
            if (index == 14)
                return GHz(2.484);
            if (index >= 1 && index <= 13)
                return GHz(2.407) + MHz(5 * index);
            break;
        default:
            break;
    }
    return band->getCenterFrequency(index);
}

bool Ieee80211Channel::isFiveGhzBand(const IIeee80211Band *band)
{
    return getBandFamily(band) == BandFamily::BAND_5_GHZ;
}

bool Ieee80211Channel::isVhtCapableBand(const IIeee80211Band *band)
{
    // VHT channelization in this model follows the 5 GHz index family.  The
    // 2.4 GHz and 5.9 GHz families have distinct index/spacing rules and are
    // intentionally not accepted for 80/160 MHz VHT geometry.
    return isFiveGhzBand(band);
}

int Ieee80211Channel::getSecondaryChannelNumber() const
{
    switch (secondaryChannelOffset) {
        case IEEE80211_SECONDARY_CHANNEL_NONE:
            return channelNumber;
        case IEEE80211_SECONDARY_CHANNEL_ABOVE:
            return channelNumber + 4;
        case IEEE80211_SECONDARY_CHANNEL_BELOW:
            return channelNumber - 4;
        default:
            throw cRuntimeError("Unknown secondary channel offset: %d", secondaryChannelOffset);
    }
}

Hz Ieee80211Channel::getSecondaryCenterFrequency() const
{
    return getSecondary20CenterFrequency();
}

Hz Ieee80211Channel::getSecondary20CenterFrequency() const
{
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_20MHZ)
        throw cRuntimeError("IEEE 802.11 channel has no secondary 20 MHz group");
    if (explicitGeometry && channelWidth != IEEE80211_CHANNEL_WIDTH_20MHZ)
        return getPrimary40CenterFrequency() * 2 - getCenterFrequency();
    int direction = secondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_ABOVE ? 1 :
            secondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_BELOW ? -1 : 0;
    if (direction == 0)
        throw cRuntimeError("IEEE 802.11 channel has no secondary channel");
    int secondaryChannelNumber = channelNumber + 4 * direction;
    Hz secondaryCenterFrequency = band->getCenterFrequency(secondaryChannelNumber);
    if (secondaryCenterFrequency != getCenterFrequency() + MHz(20 * direction))
        throw cRuntimeError("IEEE 802.11 secondary channel is not 20 MHz from the primary channel");
    return secondaryCenterFrequency;
}

Hz Ieee80211Channel::getBondedCenterFrequency() const
{
    if (explicitGeometry)
        return getPrimary40CenterFrequency();
    switch (secondaryChannelOffset) {
        case IEEE80211_SECONDARY_CHANNEL_NONE:
            return getCenterFrequency();
        case IEEE80211_SECONDARY_CHANNEL_ABOVE:
            return getCenterFrequency() + MHz(10);
        case IEEE80211_SECONDARY_CHANNEL_BELOW:
            return getCenterFrequency() - MHz(10);
        default:
            throw cRuntimeError("Invalid IEEE 802.11 secondary channel offset: %d", (int)secondaryChannelOffset);
    }
}

Hz Ieee80211Channel::getPrimary40CenterFrequency() const
{
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_20MHZ)
        return explicitGeometry ? getCenterFrequency() :
                secondaryChannelOffset == IEEE80211_SECONDARY_CHANNEL_NONE ? getCenterFrequency() : getBondedCenterFrequency();
    if (explicitGeometry && channelWidth == IEEE80211_CHANNEL_WIDTH_40MHZ)
        return getCenterFrequencyForIndex(centerFrequencyIndex0);
    Hz primary20 = getCenterFrequency();
    if (!explicitGeometry && channelWidth == IEEE80211_CHANNEL_WIDTH_40MHZ)
        return getBondedCenterFrequency();
    Hz primary80 = getPrimary80CenterFrequency();
    return primary20 < primary80 ? primary80 - MHz(20) : primary80 + MHz(20);
}

Hz Ieee80211Channel::getPrimary80CenterFrequency() const
{
    switch (channelWidth) {
        case IEEE80211_CHANNEL_WIDTH_20MHZ:
        case IEEE80211_CHANNEL_WIDTH_40MHZ:
            throw cRuntimeError("IEEE 802.11 channel has no primary 80 MHz group");
        case IEEE80211_CHANNEL_WIDTH_80MHZ:
        case IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ:
            return getCenterFrequencyForIndex(centerFrequencyIndex0);
        case IEEE80211_CHANNEL_WIDTH_160MHZ: {
            Hz operatingCenter = getOperatingCenterFrequency();
            return getCenterFrequency() < operatingCenter ? operatingCenter - MHz(40) : operatingCenter + MHz(40);
        }
        default:
            throw cRuntimeError("Invalid IEEE 802.11 channel width");
    }
}

Hz Ieee80211Channel::getOperatingCenterFrequency() const
{
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_160MHZ)
        return getCenterFrequencyForIndex(centerFrequencyIndex0);
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ || channelWidth == IEEE80211_CHANNEL_WIDTH_80MHZ)
        return getPrimary80CenterFrequency();
    return getPrimary40CenterFrequency();
}

Hz Ieee80211Channel::getSecondary40CenterFrequency() const
{
    if (channelWidth != IEEE80211_CHANNEL_WIDTH_80MHZ && channelWidth != IEEE80211_CHANNEL_WIDTH_160MHZ &&
            channelWidth != IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ)
        throw cRuntimeError("IEEE 802.11 channel has no secondary 40 MHz group");
    Hz primary80 = getPrimary80CenterFrequency();
    Hz primary40 = getPrimary40CenterFrequency();
    return primary40 < primary80 ? primary80 + MHz(20) : primary80 - MHz(20);
}

Hz Ieee80211Channel::getSecondary80CenterFrequency() const
{
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ)
        return getCenterFrequencyForIndex(centerFrequencyIndex1);
    if (channelWidth != IEEE80211_CHANNEL_WIDTH_160MHZ)
        throw cRuntimeError("IEEE 802.11 channel has no secondary 80 MHz group");
    Hz primary80 = getPrimary80CenterFrequency();
    Hz operatingCenter = getOperatingCenterFrequency();
    return operatingCenter < primary80 ? operatingCenter - MHz(40) : operatingCenter + MHz(40);
}

Hz Ieee80211Channel::getOperatingBandwidth() const
{
    switch (channelWidth) {
        case IEEE80211_CHANNEL_WIDTH_20MHZ: return MHz(20);
        case IEEE80211_CHANNEL_WIDTH_40MHZ: return MHz(40);
        case IEEE80211_CHANNEL_WIDTH_80MHZ: return MHz(80);
        case IEEE80211_CHANNEL_WIDTH_160MHZ:
        case IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ: return MHz(160);
        default: throw cRuntimeError("Invalid IEEE 802.11 channel width");
    }
}

Ieee80211ChannelBand Ieee80211Channel::getFrequencyBand(int index, Ieee80211ChannelWidth width) const
{
    if (width != IEEE80211_CHANNEL_WIDTH_20MHZ && width != IEEE80211_CHANNEL_WIDTH_40MHZ && width != IEEE80211_CHANNEL_WIDTH_80MHZ)
        throw cRuntimeError("An indexed IEEE 802.11 frequency band must be 20, 40, or 80 MHz");
    Hz bandwidth = width == IEEE80211_CHANNEL_WIDTH_20MHZ ? MHz(20) :
            width == IEEE80211_CHANNEL_WIDTH_40MHZ ? MHz(40) : MHz(80);
    return Ieee80211ChannelBand(getCenterFrequencyForIndex(index), bandwidth);
}

Ieee80211ChannelBand Ieee80211Channel::getFrequencyBand(Ieee80211ChannelWidth width) const
{
    if (width == IEEE80211_CHANNEL_WIDTH_40MHZ && channelWidth == IEEE80211_CHANNEL_WIDTH_20MHZ)
        throw cRuntimeError("IEEE 802.11 channel has no primary 40 MHz group");
    if (width == IEEE80211_CHANNEL_WIDTH_80MHZ && channelWidth != IEEE80211_CHANNEL_WIDTH_80MHZ &&
            channelWidth != IEEE80211_CHANNEL_WIDTH_160MHZ && channelWidth != IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ)
        throw cRuntimeError("IEEE 802.11 channel has no primary 80 MHz group");
    if (width == IEEE80211_CHANNEL_WIDTH_160MHZ && channelWidth != IEEE80211_CHANNEL_WIDTH_160MHZ)
        throw cRuntimeError("IEEE 802.11 channel has no primary 160 MHz group");
    switch (width) {
        case IEEE80211_CHANNEL_WIDTH_20MHZ: return Ieee80211ChannelBand(getCenterFrequency(), MHz(20));
        case IEEE80211_CHANNEL_WIDTH_40MHZ: return Ieee80211ChannelBand(getPrimary40CenterFrequency(), MHz(40));
        case IEEE80211_CHANNEL_WIDTH_80MHZ: return Ieee80211ChannelBand(getPrimary80CenterFrequency(), MHz(80));
        case IEEE80211_CHANNEL_WIDTH_160MHZ: return Ieee80211ChannelBand(getOperatingCenterFrequency(), MHz(160));
        case IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ: return Ieee80211ChannelBand(getPrimary80CenterFrequency(), MHz(80));
        default: throw cRuntimeError("Invalid IEEE 802.11 channel width");
    }
}

std::vector<Ieee80211ChannelBand> Ieee80211Channel::getOccupiedBands() const
{
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ) {
        auto primary = getFrequencyBand(IEEE80211_CHANNEL_WIDTH_80MHZ);
        auto secondary = Ieee80211ChannelBand(getSecondary80CenterFrequency(), MHz(80));
        if (secondary.centerFrequency < primary.centerFrequency)
            return {secondary, primary};
        return {primary, secondary};
    }
    return {getFrequencyBand(channelWidth)};
}

Ieee80211ChannelBand Ieee80211Channel::getPrimary40Band() const
{
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_20MHZ)
        throw cRuntimeError("IEEE 802.11 channel has no primary 40 MHz group");
    return Ieee80211ChannelBand(getPrimary40CenterFrequency(), MHz(40));
}

Ieee80211ChannelBand Ieee80211Channel::getSecondary20Band() const
{
    return Ieee80211ChannelBand(getSecondary20CenterFrequency(), MHz(20));
}

Ieee80211ChannelBand Ieee80211Channel::getSecondary40Band() const
{
    return Ieee80211ChannelBand(getSecondary40CenterFrequency(), MHz(40));
}

Ieee80211ChannelBand Ieee80211Channel::getPrimary80Band() const
{
    if (channelWidth != IEEE80211_CHANNEL_WIDTH_80MHZ && channelWidth != IEEE80211_CHANNEL_WIDTH_160MHZ &&
            channelWidth != IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ)
        throw cRuntimeError("IEEE 802.11 channel has no primary 80 MHz group");
    return Ieee80211ChannelBand(getPrimary80CenterFrequency(), MHz(80));
}

Ieee80211ChannelBand Ieee80211Channel::getSecondary80Band() const
{
    return Ieee80211ChannelBand(getSecondary80CenterFrequency(), MHz(80));
}

void Ieee80211Channel::validatePpduWidth(Hz ppduBandwidth) const
{
    if (ppduBandwidth != MHz(20) && ppduBandwidth != MHz(40) && ppduBandwidth != MHz(80) && ppduBandwidth != MHz(160))
        throw cRuntimeError("Unsupported IEEE 802.11 PPDU bandwidth: %s", ppduBandwidth.str().c_str());
    Hz maximumBandwidth = getOperatingBandwidth();
    if (ppduBandwidth > maximumBandwidth)
        throw cRuntimeError("PPDU bandwidth %s exceeds operating channel bandwidth %s", ppduBandwidth.str().c_str(), getOperatingBandwidth().str().c_str());
}

void Ieee80211Channel::validateGeometry() const
{
    if (!explicitGeometry)
        return;
    if (channelWidth != IEEE80211_CHANNEL_WIDTH_20MHZ &&
            channelWidth != IEEE80211_CHANNEL_WIDTH_40MHZ &&
            channelWidth != IEEE80211_CHANNEL_WIDTH_80MHZ &&
            channelWidth != IEEE80211_CHANNEL_WIDTH_160MHZ &&
            channelWidth != IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ)
        throw cRuntimeError("Invalid IEEE 802.11 channel width: %d", (int)channelWidth);
    if (channelWidth != IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ && centerFrequencyIndex1 != 0)
        throw cRuntimeError("IEEE 802.11 center frequency index 1 is reserved except for VHT 80+80 MHz");
    if ((channelWidth == IEEE80211_CHANNEL_WIDTH_80MHZ ||
            channelWidth == IEEE80211_CHANNEL_WIDTH_160MHZ ||
            channelWidth == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ) &&
            !Ieee80211Channel::isVhtCapableBand(band))
        throw cRuntimeError("IEEE 802.11 VHT 80/160 MHz geometry requires a declared 5 GHz band");
    int indexDifference = std::abs(channelNumber - centerFrequencyIndex0);
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_20MHZ && indexDifference != 0)
        throw cRuntimeError("The primary 20 MHz channel must equal center frequency index 0");
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_40MHZ && indexDifference != 2)
        throw cRuntimeError("The primary 20 MHz channel is not in the configured 40 MHz channel");
    if ((channelWidth == IEEE80211_CHANNEL_WIDTH_80MHZ ||
            channelWidth == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ) &&
            indexDifference != 2 && indexDifference != 6)
        throw cRuntimeError("The primary 20 MHz channel is not in the configured primary 80 MHz channel");
    if (isCanonical5GHzBand(band) &&
            (channelWidth == IEEE80211_CHANNEL_WIDTH_80MHZ || channelWidth == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ) &&
            !isCanonical5GHzVht80CenterIndex(centerFrequencyIndex0))
        throw cRuntimeError("Invalid 5 GHz VHT 80 MHz center frequency index %d", centerFrequencyIndex0);
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_160MHZ && centerFrequencyIndex1 != 0)
        throw cRuntimeError("Contiguous 160 MHz operation uses center frequency index 0 and index 1 equal to zero");
    if (isCanonical5GHzBand(band) && channelWidth == IEEE80211_CHANNEL_WIDTH_160MHZ &&
            !isCanonical5GHzVht160CenterIndex(centerFrequencyIndex0))
        throw cRuntimeError("Invalid 5 GHz contiguous 160 MHz center frequency index %d", centerFrequencyIndex0);
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_160MHZ &&
            indexDifference != 2 && indexDifference != 6 &&
            indexDifference != 10 && indexDifference != 14)
        throw cRuntimeError("The primary 20 MHz channel is not in the configured contiguous 160 MHz channel");
    if (isCanonical5GHzBand(band)) {
        if (channelWidth == IEEE80211_CHANNEL_WIDTH_20MHZ)
            validateCanonicalVht20Subchannels(centerFrequencyIndex0, {0});
        else if (channelWidth == IEEE80211_CHANNEL_WIDTH_40MHZ)
            validateCanonicalVht20Subchannels(centerFrequencyIndex0, {-2, 2});
        else if (channelWidth == IEEE80211_CHANNEL_WIDTH_80MHZ || channelWidth == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ)
            validateCanonicalVht20Subchannels(centerFrequencyIndex0, {-6, -2, 2, 6});
        else if (channelWidth == IEEE80211_CHANNEL_WIDTH_160MHZ)
            validateCanonicalVht20Subchannels(centerFrequencyIndex0, {-14, -10, -6, -2, 2, 6, 10, 14});
    }
    if (channelWidth == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ) {
        if (isCanonical5GHzBand(band) && !isCanonical5GHzVht80CenterIndex(centerFrequencyIndex1))
            throw cRuntimeError("Invalid 5 GHz VHT 80 MHz secondary center frequency index %d", centerFrequencyIndex1);
        if (isCanonical5GHzBand(band))
            validateCanonicalVht20Subchannels(centerFrequencyIndex1, {-6, -2, 2, 6});
        if (std::abs(centerFrequencyIndex1 - centerFrequencyIndex0) <= 16)
            throw cRuntimeError("VHT 80+80 MHz center frequency segments must be separated by more than 80 MHz");
    }
}

std::ostream& Ieee80211Channel::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "Ieee80211Channel";
    if (level <= PRINT_LEVEL_TRACE)
        stream << EV_FIELD(band, printFieldToString(band, level + 1, evFlags));
    if (level <= PRINT_LEVEL_INFO)
        stream << EV_FIELD(channelNumber)
               << EV_FIELD(secondaryChannelOffset)
               << EV_FIELD(channelWidth)
               << EV_FIELD(centerFrequencyIndex0)
               << EV_FIELD(centerFrequencyIndex1);
    return stream;
}

} // namespace physicallayer

} // namespace inet
