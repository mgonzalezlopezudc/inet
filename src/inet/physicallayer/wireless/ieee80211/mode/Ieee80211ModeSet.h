//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211MODESET_H
#define __INET_IEEE80211MODESET_H

#include <map>
#include <tuple>

#include "inet/common/DelayedInitializer.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211Band.h"

namespace inet {
namespace physicallayer {

class Ieee80211Channel;

enum class Ieee80211PhyFamily {
    UNSPECIFIED,
    DSSS,
    ERP_OFDM,
    OFDM,
    HT,
    VHT,
    HE,
    EHT,
};

enum class Ieee80211SupportRequirement {
    UNSPECIFIED,
    COMPATIBILITY,
    MANDATORY,
    CONDITIONALLY_MANDATORY,
    OPTIONAL,
};

enum class Ieee80211OperatingBand {
    BAND_2_4_GHZ,
    BAND_5_GHZ,
    BAND_6_GHZ,
};

enum class Ieee80211Primary80ChannelPosition {
    UNSPECIFIED,
    LOWER,
    UPPER,
};

enum Ieee80211ChannelWidthMask {
    IEEE80211_WIDTH_20 = 1 << 0,
    IEEE80211_WIDTH_40 = 1 << 1,
    IEEE80211_WIDTH_80 = 1 << 2,
    IEEE80211_WIDTH_160 = 1 << 3,
    IEEE80211_WIDTH_80P80 = 1 << 4,
    IEEE80211_WIDTH_320 = 1 << 5,
};

class INET_API Ieee80211ModeSet : public IPrintableObject, public cObject
{
  protected:
    class INET_API Entry {
      public:
        bool isMandatory;
        const IIeee80211Mode *mode;
        Ieee80211PhyFamily phyFamily = Ieee80211PhyFamily::UNSPECIFIED;
        Ieee80211SupportRequirement supportRequirement = Ieee80211SupportRequirement::UNSPECIFIED;
    };

    struct EntryNetBitrateComparator {
        bool operator()(const Entry& left, const Entry& right) { return left.mode->getDataMode()->getNetBitrate() < right.mode->getDataMode()->getNetBitrate(); }
    };

    struct ModeKey {
        Ieee80211PhyFamily phyFamily;
        int mcs;
        int numberOfSpatialStreams;
        double bandwidth;
        int guardInterval;
        int preamble;
        int band;
        bool ldpc;

      private:
        auto asTuple() const { return std::tie(phyFamily, mcs, numberOfSpatialStreams, bandwidth, guardInterval, preamble, band, ldpc); }

      public:
        bool operator==(const ModeKey& other) const { return asTuple() == other.asTuple(); }
        bool operator<(const ModeKey& other) const { return asTuple() < other.asTuple(); }
    };

    struct FamilyModeKey {
        Ieee80211PhyFamily phyFamily;
        int mcs;
        int numberOfSpatialStreams;
        double bandwidth;
        bool ldpc;

      private:
        auto asTuple() const { return std::tie(phyFamily, mcs, numberOfSpatialStreams, bandwidth, ldpc); }

      public:
        bool operator==(const FamilyModeKey& other) const { return asTuple() == other.asTuple(); }
        bool operator<(const FamilyModeKey& other) const { return asTuple() < other.asTuple(); }
    };

  protected:
    std::string name;
    std::string profileName;
    const std::vector<Entry> entries;
    const std::map<const IIeee80211Mode *, int, std::less<const IIeee80211Mode *>> pointerModeIndex;
    const std::map<ModeKey, int> compatibleModeIndex;
    const std::map<FamilyModeKey, int> familyModeIndex;
    Ieee80211OperatingBand operatingBand = Ieee80211OperatingBand::BAND_5_GHZ;
    uint8_t supportedChannelWidths = 0;
    bool channelWidthScopedBasicRates = true;
    bool bandAware = false;

    static Ieee80211ModeSet createHeProfile(const char *profileName, Ieee80211OperatingBand operatingBand,
            const std::vector<Ieee80211ModeSet>& baseModeSets);
    static std::vector<Entry> completeGuardIntervalVariants(const char *profileName,
            const std::vector<Entry>& entries);
    static std::vector<Entry> prepareEntries(const char *profileName, const std::vector<Entry>& entries);
    static bool makeModeKey(const IIeee80211Mode *mode, ModeKey& key);
    static bool makeFamilyModeKey(const IIeee80211Mode *mode, FamilyModeKey& key);
    static std::map<const IIeee80211Mode *, int, std::less<const IIeee80211Mode *>> buildPointerModeIndex(
            const std::vector<Entry>& entries);
    static std::map<ModeKey, int> buildCompatibleModeIndex(const std::vector<Entry>& entries);
    static std::map<FamilyModeKey, int> buildFamilyModeIndex(const std::vector<Entry>& entries);
    static const DelayedInitializer<std::vector<Ieee80211ModeSet>> modeSets;

  protected:
    int findModeIndex(const IIeee80211Mode *mode) const;
    int getModeIndex(const IIeee80211Mode *mode) const;

  public:
    Ieee80211ModeSet(const char *name, const std::vector<Entry> entries);
    Ieee80211ModeSet(const char *profileName, const char *name, Ieee80211OperatingBand operatingBand,
            const std::vector<Entry> entries);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override { return stream << "Ieee80211ModeSet, name = " << name; }

    const char *getName() const override { return name.c_str(); }
    const char *getProfileName() const { return profileName.c_str(); }
    static bool isHtOrVhtMode(const IIeee80211Mode *mode);
    static bool isHighThroughputMode(const IIeee80211Mode *mode);
    static bool isHtProfileName(const char *profileName);
    static bool isPeerNegotiatedFecMode(const IIeee80211Mode *mode);
    Ieee80211OperatingBand getOperatingBand() const { return operatingBand; }
    uint8_t getSupportedChannelWidths() const { return supportedChannelWidths; }
    bool isBandAware() const { return bandAware; }

    int getNumModes() const { return entries.size(); }
    const IIeee80211Mode *getMode(int index) const { return entries[index].mode; }
    bool isMandatory(int index) const { return entries[index].isMandatory; }
    Ieee80211PhyFamily getPhyFamily(int index) const { return entries[index].phyFamily; }
    Ieee80211SupportRequirement getSupportRequirement(int index) const { return entries[index].supportRequirement; }
    bool hasPhyFamily(Ieee80211PhyFamily family) const {
        for (const auto& entry : entries)
            if (entry.phyFamily == family)
                return true;
        return false;
    }

    bool containsMode(const IIeee80211Mode *mode) const { return findModeIndex(mode) != -1; }
    bool getIsMandatory(const IIeee80211Mode *mode) const;
    Ieee80211PhyFamily getPhyFamily(const IIeee80211Mode *mode) const { return mode == nullptr ? Ieee80211PhyFamily::UNSPECIFIED : entries[getModeIndex(mode)].phyFamily; }
    Ieee80211SupportRequirement getSupportRequirement(const IIeee80211Mode *mode) const { return entries[getModeIndex(mode)].supportRequirement; }

    const IIeee80211Mode *findHeMode(int mcs, int numSpatialStreams, Hz bandwidth, bool ldpc) const;
    const IIeee80211Mode *findHtMode(int mcs, int numSpatialStreams, Hz bandwidth, bool ldpc) const;
    const IIeee80211Mode *findVhtMode(int mcs, int numSpatialStreams, Hz bandwidth, bool ldpc) const;
    const IIeee80211Mode *getHtNdpMode(const IIeee80211Mode *referenceMode,
            int numberOfSpaceTimeStreams) const;
    const IIeee80211Mode *getVhtSuNdpMode(const IIeee80211Mode *referenceMode,
            int numberOfSpaceTimeStreams) const;

    const IIeee80211Mode *findMode(bps bitrate, Hz bandwidth = Hz(NaN), int numSpatialStreams = -1) const;
    const IIeee80211Mode *findMode(bps minBitrate, bps maxBitrate, Hz bandwidth = Hz(NaN), int numSpatialStreams = -1) const;
    const IIeee80211Mode *getMode(bps bitrate, Hz bandwidth = Hz(NaN), int numSpatialStreams = -1) const;
    const IIeee80211Mode *getMode(bps minBitrate, bps maxBitrate, Hz bandwidth = Hz(NaN), int numSpatialStreams = -1) const;
    const IIeee80211Mode *getSlowestMode(Hz bandwidth = Hz(NaN)) const;
    const IIeee80211Mode *getFastestMode(Hz bandwidth = Hz(NaN)) const;
    const IIeee80211Mode *getSlowerMode(const IIeee80211Mode *mode) const;
    const IIeee80211Mode *getFasterMode(const IIeee80211Mode *mode) const;
    const IIeee80211Mode *getSlowestMandatoryMode(Hz bandwidth = Hz(NaN)) const;
    const IIeee80211Mode *getFastestMandatoryMode(Hz bandwidth = Hz(NaN)) const;
    const IIeee80211Mode *getFastestBasicMode(Hz operatingChannelWidth = Hz(NaN)) const;
    const IIeee80211Mode *getFastestLegacyBasicMode() const;
    bps getNonHtReferenceRate(const IIeee80211Mode *mode) const;
    const IIeee80211Mode *getSlowerMandatoryMode(const IIeee80211Mode *mode) const;
    const IIeee80211Mode *getFasterMandatoryMode(const IIeee80211Mode *mode) const;

    static const Ieee80211ModeSet *findModeSet(const char *mode);
    static const Ieee80211ModeSet *getModeSet(const char *mode);
    static const Ieee80211ModeSet *findModeSet(const char *mode, const IIeee80211Band *band);
    static const Ieee80211ModeSet *getModeSet(const char *mode, const IIeee80211Band *band);
    static Hz getChannelWidth(const IIeee80211Band *band, Hz configuredBandwidth = Hz(NaN));
    Hz getModeBandwidth(const IIeee80211Band *band, Hz configuredBandwidth = Hz(NaN)) const;
    bool supportsChannel(const IIeee80211Band *band, Hz configuredBandwidth = Hz(NaN)) const;
    void validateChannel(const IIeee80211Band *band, Hz configuredBandwidth = Hz(NaN)) const;

    simtime_t getSifsTime() const { return entries[0].mode->getSifsTime(); }
    simtime_t getSlotTime() const { return entries[0].mode->getSlotTime(); }
    simtime_t getPhyRxStartDelay() const { return entries[0].mode->getPhyRxStartDelay(); }
    int getCwMin() const { return entries[0].mode->getLegacyCwMin(); }
    int getCwMax() const { return entries[0].mode->getLegacyCwMax(); }

    IIeee80211Mode *_getSlowestMode() const { return const_cast<IIeee80211Mode *>(getSlowestMode()); }
    IIeee80211Mode *_getFastestMode() const { return const_cast<IIeee80211Mode *>(getFastestMode()); }
    IIeee80211Mode *_getSlowestMandatoryMode() const { return const_cast<IIeee80211Mode *>(getSlowestMandatoryMode()); }
    IIeee80211Mode *_getFastestMandatoryMode() const { return const_cast<IIeee80211Mode *>(getFastestMandatoryMode()); }
};

/** Read-only owner-neutral access to the radio's resolved 802.11 mode profile. */
class INET_API IIeee80211ModeSetProvider
{
  public:
    virtual ~IIeee80211ModeSetProvider() = default;
    virtual const Ieee80211ModeSet *getModeSet() const = 0;
    virtual const IIeee80211Band *getBand() const = 0;
    virtual const Ieee80211Channel *getChannel() const = 0;
    virtual Ieee80211Primary80ChannelPosition getPrimary80ChannelPosition() const = 0;
    virtual Hz getChannelWidth() const = 0;
    virtual Hz getModeBandwidth() const = 0;
};

} // namespace physicallayer
} // namespace inet

#endif
