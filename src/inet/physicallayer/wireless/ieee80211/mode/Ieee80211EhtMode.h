//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211EHTMODE_H
#define __INET_IEEE80211EHTMODE_H

#define DI    DelayedInitializer

#include <map>
#include <tuple>

#include "inet/common/DelayedInitializer.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeBase.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"

namespace inet {
namespace physicallayer {

/** EHT OFDM timing constants for the 0.8, 1.6, and 3.2 microsecond guard intervals. */
class INET_API Ieee80211EhtTimingRelatedParametersBase
{
  public:
    const simtime_t getDFTPeriod() const { return 12.8E-6; }
    const simtime_t getGIDuration() const { return 3.2E-6; }
    const simtime_t getMediumGIDuration() const { return 1.6E-6; }
    const simtime_t getShortGIDuration() const { return 0.8E-6; }
    const simtime_t getSymbolInterval() const { return getDFTPeriod() + getGIDuration(); }
    const simtime_t getMediumGISymbolInterval() const { return getDFTPeriod() + getMediumGIDuration(); }
    const simtime_t getShortGISymbolInterval() const { return getDFTPeriod() + getShortGIDuration(); }
};

/** Common EHT bandwidth, guard-interval, MCS, stream-count, and bitrate state. */
class INET_API Ieee80211EhtModeBase
{
  public:
    enum GuardIntervalType {
        EHT_GUARD_INTERVAL_SHORT,
        EHT_GUARD_INTERVAL_MEDIUM,
        EHT_GUARD_INTERVAL_LONG
    };

  protected:
    const Hz bandwidth;
    const GuardIntervalType guardIntervalType;
    const unsigned int mcsIndex;
    const unsigned int numberOfSpatialStreams;

    mutable bps netBitrate;
    mutable bps grossBitrate;

  protected:
    virtual bps computeGrossBitrate() const = 0;
    virtual bps computeNetBitrate() const = 0;

  public:
    Ieee80211EhtModeBase(unsigned int modulationAndCodingScheme, unsigned int numberOfSpatialStreams, const Hz bandwidth, GuardIntervalType guardIntervalType);

    virtual int getNumberOfDataSubcarriers() const;
    virtual int getNumberOfPilotSubcarriers() const;
    virtual int getNumberOfTotalSubcarriers() const { return getNumberOfDataSubcarriers() + getNumberOfPilotSubcarriers(); }
    virtual GuardIntervalType getGuardIntervalType() const { return guardIntervalType; }
    virtual int getNumberOfSpatialStreams() const { return numberOfSpatialStreams; }
    virtual unsigned int getMcsIndex() const { return mcsIndex; }
    virtual Hz getBandwidth() const { return bandwidth; }
    virtual bps getNetBitrate() const;
    virtual bps getGrossBitrate() const;
};

/** EHT signaling-field transmission mode. */
class INET_API Ieee80211EhtSignalMode : public IIeee80211HeaderMode, public Ieee80211EhtModeBase, public Ieee80211EhtTimingRelatedParametersBase
{
  protected:
    const Ieee80211OfdmModulation *modulation;
    const Ieee80211VhtCode *code;

  protected:
    virtual bps computeGrossBitrate() const override;
    virtual bps computeNetBitrate() const override;

  public:
    Ieee80211EhtSignalMode(unsigned int modulationAndCodingScheme, const Ieee80211OfdmModulation *modulation, const Ieee80211VhtCode *code, const Hz bandwidth, GuardIntervalType guardIntervalType);
    Ieee80211EhtSignalMode(unsigned int modulationAndCodingScheme, const Ieee80211OfdmModulation *modulation, const Ieee80211ConvolutionalCode *convolutionalCode, const Hz bandwidth, GuardIntervalType guardIntervalType);
    virtual ~Ieee80211EhtSignalMode();

    virtual unsigned int getModulationAndCodingScheme() const { return mcsIndex; }
    virtual const simtime_t getDuration() const override { return 2 * getSymbolInterval(); }
    virtual b getLength() const override { return b(48); }
    virtual bps getNetBitrate() const override { return Ieee80211EhtModeBase::getNetBitrate(); }
    virtual bps getGrossBitrate() const override { return Ieee80211EhtModeBase::getGrossBitrate(); }
    virtual const simtime_t getSymbolInterval() const override;
    virtual const Ieee80211OfdmModulation *getModulation() const override { return modulation; }
    virtual const Ieee80211VhtCode *getCode() const { return code; }

    virtual Ptr<Ieee80211PhyHeader> createHeader() const override { return makeShared<Ieee80211HtPhyHeader>(); }
};

/** EHT SU preamble mode, including the legacy-compatible preamble portion. */
class INET_API Ieee80211EhtPreambleMode : public IIeee80211PreambleMode, public Ieee80211EhtTimingRelatedParametersBase
{
  public:
    enum ExtremelyHighThroughputPreambleFormat {
        EHT_PREAMBLE_SU
    };

  protected:
    const Ieee80211EhtSignalMode *extremelyHighThroughputSignalMode;
    const Ieee80211OfdmSignalMode *legacySignalMode;
    const ExtremelyHighThroughputPreambleFormat preambleFormat;
    const unsigned int numberOfEhtLongTrainings;

  protected:
    virtual unsigned int computeNumberOfEhtLongTrainings(unsigned int numberOfSpatialStreams) const;

  public:
    Ieee80211EhtPreambleMode(const Ieee80211EhtSignalMode *extremelyHighThroughputSignalMode, const Ieee80211OfdmSignalMode *legacySignalMode, ExtremelyHighThroughputPreambleFormat preambleFormat, unsigned int numberOfSpatialStreams);
    virtual ~Ieee80211EhtPreambleMode() { delete extremelyHighThroughputSignalMode; }

    ExtremelyHighThroughputPreambleFormat getPreambleFormat() const { return preambleFormat; }
    virtual const Ieee80211EhtSignalMode *getSignalMode() const { return extremelyHighThroughputSignalMode; }
    virtual const Ieee80211OfdmSignalMode *getLegacySignalMode() const { return legacySignalMode; }

    virtual const simtime_t getNonHTShortTrainingSequenceDuration() const { return 8E-6; }
    virtual const simtime_t getNonHTLongTrainingFieldDuration() const { return 8E-6; }
    virtual const simtime_t getNonHTSignalField() const { return 4E-6; }
    virtual const simtime_t getUniversalSignalField() const { return 8E-6; }
    virtual const simtime_t getEhtSignalField() const { return 4E-6; }
    virtual const simtime_t getEhtShortTrainingFieldDuration() const { return 4E-6; }

    virtual const simtime_t getDuration() const override;

    virtual Ptr<Ieee80211PhyPreamble> createPreamble() const override { return makeShared<Ieee80211VhtPhyPreamble>(); }
};

/** One EHT MCS definition: per-stream modulation, FEC, and channel bandwidth. */
class INET_API Ieee80211Ehtmcs
{
  protected:
    const unsigned int mcsIndex;
    const Ieee80211OfdmModulation *stream1Modulation = nullptr;
    const Ieee80211OfdmModulation *stream2Modulation = nullptr;
    const Ieee80211OfdmModulation *stream3Modulation = nullptr;
    const Ieee80211OfdmModulation *stream4Modulation = nullptr;
    const Ieee80211OfdmModulation *stream5Modulation = nullptr;
    const Ieee80211OfdmModulation *stream6Modulation = nullptr;
    const Ieee80211OfdmModulation *stream7Modulation = nullptr;
    const Ieee80211OfdmModulation *stream8Modulation = nullptr;
    const Ieee80211VhtCode *code;
    const Hz bandwidth;

  public:
    Ieee80211Ehtmcs(unsigned int mcsIndex, const ApskModulationBase *stream1SubcarrierModulation, const Ieee80211ConvolutionalCode *convolutionalCode, Hz bandwidth, int nss);
    virtual ~Ieee80211Ehtmcs();

    const Ieee80211VhtCode *getCode() const { return code; }
    unsigned int getMcsIndex() const { return mcsIndex; }
    virtual const Ieee80211OfdmModulation *getModulation() const { return stream1Modulation; }
    virtual const Ieee80211OfdmModulation *getStreamExtension1Modulation() const { return stream2Modulation; }
    virtual const Ieee80211OfdmModulation *getStreamExtension2Modulation() const { return stream3Modulation; }
    virtual const Ieee80211OfdmModulation *getStreamExtension3Modulation() const { return stream4Modulation; }
    virtual const Ieee80211OfdmModulation *getStreamExtension4Modulation() const { return stream5Modulation; }
    virtual const Ieee80211OfdmModulation *getStreamExtension5Modulation() const { return stream6Modulation; }
    virtual const Ieee80211OfdmModulation *getStreamExtension6Modulation() const { return stream7Modulation; }
    virtual const Ieee80211OfdmModulation *getStreamExtension7Modulation() const { return stream8Modulation; }
    virtual Hz getBandwidth() const { return bandwidth; }
    virtual unsigned int getNumNss() const;
};

/** EHT PSDU data mode and airtime calculator for the SU mode-set path. */
class INET_API Ieee80211EhtDataMode : public IIeee80211DataMode, public Ieee80211EhtModeBase, public Ieee80211EhtTimingRelatedParametersBase
{
  protected:
    const Ieee80211Ehtmcs *modulationAndCodingScheme;
    const unsigned int numberOfBccEncoders;

  protected:
    bps computeGrossBitrate() const override;
    bps computeNetBitrate() const override;
    unsigned int computeNumberOfCodedBitsPerSubcarrierSum() const;
    unsigned int computeNumberOfBccEncoders() const;

  public:
    Ieee80211EhtDataMode(const Ieee80211Ehtmcs *modulationAndCodingScheme, const Hz bandwidth, GuardIntervalType guardIntervalType);

    b getServiceFieldLength() const { return b(16); }
    b getTailFieldLength() const { return b(6) * numberOfBccEncoders; }

    virtual int getNumberOfSpatialStreams() const override { return Ieee80211EhtModeBase::getNumberOfSpatialStreams(); }
    virtual Hz getBandwidth() const override { return bandwidth; }
    virtual b getPaddingLength(b dataLength) const override { return b(0); }
    virtual b getCompleteLength(b dataLength) const override;
    virtual const simtime_t getDuration(b dataLength) const override;
    virtual bps getNetBitrate() const override { return Ieee80211EhtModeBase::getNetBitrate(); }
    virtual bps getGrossBitrate() const override { return Ieee80211EhtModeBase::getGrossBitrate(); }
    virtual const Ieee80211Ehtmcs *getModulationAndCodingScheme() const { return modulationAndCodingScheme; }
    virtual const Ieee80211VhtCode *getCode() const { return modulationAndCodingScheme->getCode(); }
    virtual const simtime_t getSymbolInterval() const override;
    virtual const Ieee80211OfdmModulation *getModulation() const override { return modulationAndCodingScheme->getModulation(); }
};

/** Complete EHT PHY mode composed of its preamble and PSDU data mode. */
class INET_API Ieee80211EhtMode : public Ieee80211ModeBase
{
  public:
    enum BandMode {
        BAND_5GHZ,
        BAND_6GHZ
    };

  protected:
    const Ieee80211EhtPreambleMode *preambleMode;
    const Ieee80211EhtDataMode *dataMode;
    const BandMode centerFrequencyMode;

  protected:
    virtual int getLegacyCwMin() const override { return 15; }
    virtual int getLegacyCwMax() const override { return 1023; }

  public:
    Ieee80211EhtMode(const char *name, const Ieee80211EhtPreambleMode *preambleMode, const Ieee80211EhtDataMode *dataMode, const BandMode centerFrequencyMode);
    virtual ~Ieee80211EhtMode() { delete preambleMode; delete dataMode; }

    virtual const Ieee80211EhtDataMode *getDataMode() const override { return dataMode; }
    virtual const Ieee80211EhtPreambleMode *getPreambleMode() const override { return preambleMode; }
    virtual const Ieee80211EhtSignalMode *getHeaderMode() const override { return preambleMode->getSignalMode(); }
    virtual const Ieee80211OfdmSignalMode *getLegacySignalMode() const { return preambleMode->getLegacySignalMode(); }

    virtual const simtime_t getSlotTime() const override { return 9E-6; }
    virtual const simtime_t getSifsTime() const override { return 16E-6; }
    virtual const simtime_t getRifsTime() const override { return 2E-6; }
    virtual const simtime_t getCcaTime() const override { return 4E-6; }
    virtual const simtime_t getPhyRxStartDelay() const override { return 32E-6; }
    virtual const simtime_t getRxTxTurnaroundTime() const override { return 2E-6; }
    virtual const simtime_t getPreambleLength() const override { return 16E-6; }
    virtual const simtime_t getPlcpHeaderLength() const override { return 4E-6; }
    virtual int getMpduMaxLength() const override { return 65535; }
    virtual BandMode getCenterFrequencyMode() const { return centerFrequencyMode; }

    virtual const simtime_t getDuration(b dataBitLength) const override { return preambleMode->getDuration() + dataMode->getDuration(dataBitLength); }
};

/** Lookup table for standard EHT MCS combinations. */
class INET_API Ieee80211EhtmcsTable
{  public:
#define EHT_DECLARE_MCS(WIDTH, NSS, MCS) static const DI<Ieee80211Ehtmcs> ehtMcs##MCS##BW##WIDTH##MHzNss##NSS;
#define EHT_DECLARE_MCS_FOR_NSS(WIDTH, NSS) \
    EHT_DECLARE_MCS(WIDTH, NSS, 0) \
    EHT_DECLARE_MCS(WIDTH, NSS, 1) \
    EHT_DECLARE_MCS(WIDTH, NSS, 2) \
    EHT_DECLARE_MCS(WIDTH, NSS, 3) \
    EHT_DECLARE_MCS(WIDTH, NSS, 4) \
    EHT_DECLARE_MCS(WIDTH, NSS, 5) \
    EHT_DECLARE_MCS(WIDTH, NSS, 6) \
    EHT_DECLARE_MCS(WIDTH, NSS, 7) \
    EHT_DECLARE_MCS(WIDTH, NSS, 8) \
    EHT_DECLARE_MCS(WIDTH, NSS, 9) \
    EHT_DECLARE_MCS(WIDTH, NSS, 10) \
    EHT_DECLARE_MCS(WIDTH, NSS, 11) \
    EHT_DECLARE_MCS(WIDTH, NSS, 12) \
    EHT_DECLARE_MCS(WIDTH, NSS, 13)
#define EHT_DECLARE_MCS_FOR_BW(WIDTH) \
    EHT_DECLARE_MCS_FOR_NSS(WIDTH, 1) \
    EHT_DECLARE_MCS_FOR_NSS(WIDTH, 2) \
    EHT_DECLARE_MCS_FOR_NSS(WIDTH, 3) \
    EHT_DECLARE_MCS_FOR_NSS(WIDTH, 4) \
    EHT_DECLARE_MCS_FOR_NSS(WIDTH, 5) \
    EHT_DECLARE_MCS_FOR_NSS(WIDTH, 6) \
    EHT_DECLARE_MCS_FOR_NSS(WIDTH, 7) \
    EHT_DECLARE_MCS_FOR_NSS(WIDTH, 8)

    EHT_DECLARE_MCS_FOR_BW(20)
    EHT_DECLARE_MCS_FOR_BW(40)
    EHT_DECLARE_MCS_FOR_BW(80)
    EHT_DECLARE_MCS_FOR_BW(160)
    EHT_DECLARE_MCS_FOR_BW(320)

#undef EHT_DECLARE_MCS_FOR_BW
#undef EHT_DECLARE_MCS_FOR_NSS
#undef EHT_DECLARE_MCS
};

#undef DI

/** Factory and cache for standard EHT PHY modes. */
class INET_API Ieee80211EhtCompliantModes
{
  protected:
    static OPP_THREAD_LOCAL const Ieee80211EhtCompliantModes singleton;

    mutable std::map<std::tuple<Hz, unsigned int, Ieee80211EhtModeBase::GuardIntervalType,
            unsigned int, Ieee80211EhtMode::BandMode,
            Ieee80211EhtPreambleMode::ExtremelyHighThroughputPreambleFormat>, const Ieee80211EhtMode *> modeCache;

  public:
    Ieee80211EhtCompliantModes();
    virtual ~Ieee80211EhtCompliantModes();

    static const Ieee80211EhtMode *getCompliantMode(const Ieee80211Ehtmcs *mcsMode, Ieee80211EhtMode::BandMode centerFrequencyMode, Ieee80211EhtPreambleMode::ExtremelyHighThroughputPreambleFormat preambleFormat, Ieee80211EhtModeBase::GuardIntervalType guardIntervalType);
};

} // namespace physicallayer
} // namespace inet

#endif
