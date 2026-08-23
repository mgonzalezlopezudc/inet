//
// Copyright (C) 2014 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_IEEE80211MODESET_H
#define __INET_IEEE80211MODESET_H

#include "inet/common/DelayedInitializer.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"

namespace inet {
namespace physicallayer {

class INET_API Ieee80211ModeSet : public IPrintableObject, public cObject
{
  protected:
    class INET_API Entry {
      public:
        bool isMandatory;
        const IIeee80211Mode *mode;
    };

    struct EntryNetBitrateComparator {
        bool operator()(const Entry& left, const Entry& right) { return left.mode->getDataMode()->getNetBitrate() < right.mode->getDataMode()->getNetBitrate(); }
    };

  protected:
    std::string name;
    const std::vector<Entry> entries;

  public:
    static const DelayedInitializer<std::vector<Ieee80211ModeSet>> modeSets;

  protected:
    int findModeIndex(const IIeee80211Mode *mode) const;
    int findCompatibleMetadataIndex(const IIeee80211Mode *mode) const;
    const IIeee80211Mode *resolveEntryMode(const IIeee80211Mode *entryMode, const IIeee80211Mode *referenceMode) const;

  public:
    Ieee80211ModeSet(const char *name, const std::vector<Entry> entries);

    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override { return stream << "Ieee80211ModeSet, name = " << name; }

    const char *getName() const override { return name.c_str(); }

    int getNumModes() const { return entries.size(); }
    const IIeee80211Mode *getMode(int index) const { return entries[index].mode; }
    bool isMandatory(int index) const { return entries[index].isMandatory; }

    bool containsMode(const IIeee80211Mode *mode) const;
    bool getIsMandatory(const IIeee80211Mode *mode) const;

    const IIeee80211Mode *getFecVariant(const IIeee80211Mode *mode, Ieee80211FecType fecType) const;
    const IIeee80211Mode *findMode(bps bitrate, Hz bandwidth = Hz(NaN), int numSpatialStreams = -1, Ieee80211FecType fecType = Ieee80211FecType::BCC) const;
    const IIeee80211Mode *findMode(bps minBitrate, bps maxBitrate, Hz bandwidth = Hz(NaN), int numSpatialStreams = -1, Ieee80211FecType fecType = Ieee80211FecType::BCC) const;
    const IIeee80211Mode *getMode(bps bitrate, Hz bandwidth = Hz(NaN), int numSpatialStreams = -1, Ieee80211FecType fecType = Ieee80211FecType::BCC) const;
    const IIeee80211Mode *getMode(bps minBitrate, bps maxBitrate, Hz bandwidth = Hz(NaN), int numSpatialStreams = -1, Ieee80211FecType fecType = Ieee80211FecType::BCC) const;
    const IIeee80211Mode *getSlowestMode(Ieee80211FecType fecType = Ieee80211FecType::BCC) const;
    const IIeee80211Mode *getFastestMode(Ieee80211FecType fecType = Ieee80211FecType::BCC) const;
    const IIeee80211Mode *getSlowerMode(const IIeee80211Mode *mode) const;
    const IIeee80211Mode *getFasterMode(const IIeee80211Mode *mode) const;
    const IIeee80211Mode *getSlowestMandatoryMode(Ieee80211FecType fecType = Ieee80211FecType::BCC) const;
    const IIeee80211Mode *getFastestMandatoryMode(Ieee80211FecType fecType = Ieee80211FecType::BCC) const;
    const IIeee80211Mode *getSlowerMandatoryMode(const IIeee80211Mode *mode) const;
    const IIeee80211Mode *getFasterMandatoryMode(const IIeee80211Mode *mode) const;

    static const Ieee80211ModeSet *findModeSet(const char *mode);
    static const Ieee80211ModeSet *getModeSet(const char *mode);
    /** Resolves an exact canonical HT/VHT mode tuple, or returns nullptr when the tuple is not legal. */
    static const IIeee80211Mode *findMode(Ieee80211PhyFormat phyFormat, unsigned int mcsIndex,
            Hz bandwidth, unsigned int numberOfSpatialStreams,
            Ieee80211FecType fecType = Ieee80211FecType::BCC);
    /**
     * Resolves an exact canonical HT/VHT mode tuple including the received GI.
     * The capability check is made against the canonical operation-mode set,
     * while the returned mode is constructed by the canonical HT/VHT mode
     * factory so a mode-set entry's incidental GI does not override the
     * received PHY header.
     */
    static const IIeee80211Mode *findMode(Ieee80211PhyFormat phyFormat, unsigned int mcsIndex,
            Hz bandwidth, unsigned int numberOfSpatialStreams,
            Ieee80211FecType fecType, bool shortGi);

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

} // namespace physicallayer
} // namespace inet

#endif
