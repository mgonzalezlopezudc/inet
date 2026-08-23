//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211CHANNEL_H
#define __INET_IEEE80211CHANNEL_H

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Band.h"

namespace inet {

namespace physicallayer {

// IEEE Std 802.11-2024, Table 9-134.
enum Ieee80211SecondaryChannelOffset {
    IEEE80211_SECONDARY_CHANNEL_NONE = 0,
    IEEE80211_SECONDARY_CHANNEL_ABOVE = 1,
    IEEE80211_SECONDARY_CHANNEL_BELOW = 3,
};

// IEEE Std 802.11-2024, Table 21-22.  WIDTH_80_PLUS_80 is deliberately a
// distinct value even though its aggregate occupied bandwidth is 160 MHz.
enum Ieee80211ChannelWidth {
    IEEE80211_CHANNEL_WIDTH_20MHZ,
    IEEE80211_CHANNEL_WIDTH_40MHZ,
    IEEE80211_CHANNEL_WIDTH_80MHZ,
    IEEE80211_CHANNEL_WIDTH_160MHZ,
    IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ,
};

struct INET_API Ieee80211ChannelBand
{
    Hz centerFrequency;
    Hz bandwidth;

    Ieee80211ChannelBand() : centerFrequency(Hz(NaN)), bandwidth(Hz(NaN)) {}
    Ieee80211ChannelBand(Hz centerFrequency, Hz bandwidth) : centerFrequency(centerFrequency), bandwidth(bandwidth) {}
    Hz getLowerFrequency() const { return centerFrequency - bandwidth / 2; }
    Hz getUpperFrequency() const { return centerFrequency + bandwidth / 2; }
};

class INET_API Ieee80211Channel : public IPrintableObject
{
  protected:
    const IIeee80211Band *band;
    int channelNumber;
    Ieee80211SecondaryChannelOffset secondaryChannelOffset;
    Ieee80211ChannelWidth channelWidth = IEEE80211_CHANNEL_WIDTH_20MHZ;
    int centerFrequencyIndex0 = -1;
    int centerFrequencyIndex1 = 0;
    bool explicitGeometry = false;

  protected:
    virtual void validateGeometry() const;

  public:
    Ieee80211Channel(const IIeee80211Band *band, int channelNumber);
    Ieee80211Channel(const IIeee80211Band *band, int channelNumber, Ieee80211SecondaryChannelOffset secondaryChannelOffset);
    Ieee80211Channel(const IIeee80211Band *band, int primaryChannelNumber, Ieee80211ChannelWidth channelWidth, int centerFrequencyIndex0, int centerFrequencyIndex1 = 0);

    static Ieee80211SecondaryChannelOffset parseSecondaryChannelOffset(const char *text);
    static const char *getSecondaryChannelOffsetName(Ieee80211SecondaryChannelOffset offset);
    static Ieee80211ChannelWidth parseChannelWidth(const char *text);
    static const char *getChannelWidthName(Ieee80211ChannelWidth width);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;

    virtual const IIeee80211Band *getBand() const { return band; }
    virtual int getChannelNumber() const { return channelNumber; }
    virtual Ieee80211SecondaryChannelOffset getSecondaryChannelOffset() const { return secondaryChannelOffset; }
    virtual Ieee80211ChannelWidth getChannelWidth() const { return channelWidth; }
    virtual bool isExplicitGeometry() const { return explicitGeometry; }
    virtual int getCenterFrequencyIndex0() const { return centerFrequencyIndex0 == -1 ? channelNumber : centerFrequencyIndex0; }
    virtual int getCenterFrequencyIndex1() const { return centerFrequencyIndex1; }
    virtual Hz getCenterFrequencyForIndex(int index) const;
    virtual int getSecondaryChannelNumber() const;
    virtual Hz getCenterFrequency() const { return getCenterFrequencyForIndex(channelNumber); }
    virtual Hz getSecondaryCenterFrequency() const;
    virtual Hz getBondedCenterFrequency() const;
    virtual Hz getPrimary20CenterFrequency() const { return getCenterFrequency(); }
    virtual Hz getSecondary20CenterFrequency() const;
    virtual Hz getPrimary40CenterFrequency() const;
    virtual Hz getPrimary80CenterFrequency() const;
    virtual Hz getOperatingCenterFrequency() const;
    virtual Hz getSecondary40CenterFrequency() const;
    virtual Hz getSecondary80CenterFrequency() const;
    virtual Hz getOperatingBandwidth() const;
    virtual Ieee80211ChannelBand getFrequencyBand(Ieee80211ChannelWidth width) const;
    virtual Ieee80211ChannelBand getFrequencyBand(int centerFrequencyIndex, Ieee80211ChannelWidth width) const;
    virtual std::vector<Ieee80211ChannelBand> getOccupiedBands() const;
    virtual Ieee80211ChannelBand getPrimary20Band() const { return Ieee80211ChannelBand(getPrimary20CenterFrequency(), MHz(20)); }
    virtual Ieee80211ChannelBand getSecondary20Band() const;
    virtual Ieee80211ChannelBand getPrimary40Band() const;
    virtual Ieee80211ChannelBand getSecondary40Band() const;
    virtual Ieee80211ChannelBand getPrimary80Band() const;
    virtual Ieee80211ChannelBand getSecondary80Band() const;
    virtual bool is80Plus80() const { return channelWidth == IEEE80211_CHANNEL_WIDTH_80_PLUS_80MHZ; }
    virtual void validatePpduWidth(Hz ppduBandwidth) const;
};

} // namespace physicallayer

} // namespace inet

#endif
