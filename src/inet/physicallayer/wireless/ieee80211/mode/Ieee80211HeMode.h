//
// Copyright (C) 2026 Antigravity
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEMODE_H
#define __INET_IEEE80211HEMODE_H

#define DI    DelayedInitializer

#include "inet/common/DelayedInitializer.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeBase.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"

namespace inet {
namespace physicallayer {

/**
 * HE OFDM timing constants for the 0.8, 1.6, and 3.2 microsecond guard intervals.
 * IEEE 802.11-2024 Table 27-61 ("HE PHY characteristics").
 * In 802.11ax HE, the FFT size is quadrupled to 256/512/1024/2048 points, which
 * increases the DFT period (T_DFT) to 12.8 µs (4x the legacy 3.2 µs DFT period).
 * This base class provides the standard timing equations:
 * - T_DFT (DFT Period) = 12.8 µs
 * - T_GI (Guard Interval): 3.2 µs (Long), 1.6 µs (Medium), 0.8 µs (Short)
 * - T_SYM (Symbol Interval) = T_DFT + T_GI
 */
class INET_API Ieee80211HeTimingRelatedParametersBase
{
  public:
    const simtime_t getDFTPeriod() const { return 12.8E-6; } // DFT period for HE is 12.8 µs
    const simtime_t getGIDuration() const { return 3.2E-6; }  // Long GI (1/4 GI = 3.2 µs)
    const simtime_t getMediumGIDuration() const { return 1.6E-6; } // Medium GI (1/8 GI = 1.6 µs)
    const simtime_t getShortGIDuration() const { return 0.8E-6; } // Short GI (1/16 GI = 0.8 µs)
    const simtime_t getSymbolInterval() const { return getDFTPeriod() + getGIDuration(); } // 16.0 µs
    const simtime_t getMediumGISymbolInterval() const { return getDFTPeriod() + getMediumGIDuration(); } // 14.4 µs
    const simtime_t getShortGISymbolInterval() const { return getDFTPeriod() + getShortGIDuration(); } // 13.6 µs
};

/** Common HE bandwidth, guard-interval, MCS, stream-count, and bitrate state. */
class INET_API Ieee80211HeModeBase
{
  public:
    enum GuardIntervalType {
        HE_GUARD_INTERVAL_SHORT, // 0.8 µs (used in low-delay outdoor/indoor channels)
        HE_GUARD_INTERVAL_MEDIUM, // 1.6 µs (good trade-off for typical indoor/outdoor)
        HE_GUARD_INTERVAL_LONG   // 3.2 µs (default for outdoor high-multipath mitigation)
    };

  protected:
    const Hz bandwidth;
    const GuardIntervalType guardIntervalType;
    const unsigned int mcsIndex; // MCS index (0 to 11)
    const unsigned int numberOfSpatialStreams; // N_SS (up to 8 spatial streams; Clause 27.3.12.5)

    mutable bps netBitrate; // cached
    mutable bps grossBitrate; // cached

  protected:
    virtual bps computeGrossBitrate() const = 0;
    virtual bps computeNetBitrate() const = 0;

  public:
    Ieee80211HeModeBase(unsigned int modulationAndCodingScheme, unsigned int numberOfSpatialStreams, const Hz bandwidth, GuardIntervalType guardIntervalType);

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

/** HE signaling-field transmission mode (HE-SIG-A). */
class INET_API Ieee80211HeSignalMode : public IIeee80211HeaderMode, public Ieee80211HeModeBase, public Ieee80211HeTimingRelatedParametersBase
{
  protected:
    const Ieee80211OfdmModulation *modulation;
    const Ieee80211VhtCode *code;

  protected:
    virtual bps computeGrossBitrate() const override;
    virtual bps computeNetBitrate() const override;

  public:
    Ieee80211HeSignalMode(unsigned int modulationAndCodingScheme, const Ieee80211OfdmModulation *modulation, const Ieee80211VhtCode *code, const Hz bandwidth, GuardIntervalType guardIntervalType);
    Ieee80211HeSignalMode(unsigned int modulationAndCodingScheme, const Ieee80211OfdmModulation *modulation, const Ieee80211ConvolutionalCode *convolutionalCode, const Hz bandwidth, GuardIntervalType guardIntervalType);
    virtual ~Ieee80211HeSignalMode();

    virtual unsigned int getModulationAndCodingScheme() const { return mcsIndex; }
    virtual const simtime_t getDuration() const override { return 2 * getSymbolInterval(); }
    virtual b getLength() const override { return b(48); }
    virtual bps getNetBitrate() const override { return Ieee80211HeModeBase::getNetBitrate(); }
    virtual bps getGrossBitrate() const override { return Ieee80211HeModeBase::getGrossBitrate(); }
    virtual const simtime_t getSymbolInterval() const override {
        switch (guardIntervalType) {
            case HE_GUARD_INTERVAL_SHORT: return getShortGISymbolInterval();
            case HE_GUARD_INTERVAL_MEDIUM: return getMediumGISymbolInterval();
            case HE_GUARD_INTERVAL_LONG: return Ieee80211HeTimingRelatedParametersBase::getSymbolInterval();
            default: throw cRuntimeError("Unknown HE guard interval");
        }
    }
    virtual const Ieee80211OfdmModulation *getModulation() const override { return modulation; }
    virtual const Ieee80211VhtCode *getCode() const { return code; }

    virtual Ptr<Ieee80211PhyHeader> createHeader() const override { return makeShared<Ieee80211HtPhyHeader>(); }
};

/**
 * HE SU or MU preamble mode, including the legacy-compatible preamble portion.
 * IEEE 802.11-2024, Clause 27.3.4 ("HE PPDU formats").
 * The HE preamble consists of:
 * - Legacy parts (L-STF, L-LTF, L-SIG) to maintain backward compatibility.
 * - Repeated L-SIG (RL-SIG) to signal HE PPDU presence.
 * - HE-SIG-A (common parameters).
 * - HE-SIG-B (optional, DL MU PPDU only).
 * - HE-STF and multiple HE-LTFs for spatial channel training.
 */
class INET_API Ieee80211HePreambleMode : public IIeee80211PreambleMode, public Ieee80211HeTimingRelatedParametersBase
{
  public:
    enum HighEfficiencyPreambleFormat {
        HE_PREAMBLE_SU,
        HE_PREAMBLE_MU,
        HE_PREAMBLE_ER_SU
    };

  protected:
    const Ieee80211HeSignalMode *highEfficiencySignalMode;
    const Ieee80211OfdmSignalMode *legacySignalMode;
    const HighEfficiencyPreambleFormat preambleFormat;
    const unsigned int numberOfHELongTrainings; // Depends on the number of spatial streams (N_STS)

  protected:
    virtual unsigned int computeNumberOfHELongTrainings(unsigned int numberOfSpatialStreams) const;

  public:
    Ieee80211HePreambleMode(const Ieee80211HeSignalMode *highEfficiencySignalMode, const Ieee80211OfdmSignalMode *legacySignalMode, HighEfficiencyPreambleFormat preambleFormat, unsigned int numberOfSpatialStreams);
    virtual ~Ieee80211HePreambleMode() { delete highEfficiencySignalMode; }

    HighEfficiencyPreambleFormat getPreambleFormat() const { return preambleFormat; }
    virtual const Ieee80211HeSignalMode *getSignalMode() const { return highEfficiencySignalMode; }
    virtual const Ieee80211OfdmSignalMode *getLegacySignalMode() const { return legacySignalMode; }

    virtual const simtime_t getDoubleGIDuration() const { return 2 * getGIDuration(); }
    virtual const simtime_t getLSIGDuration() const { return 4E-6; }
    virtual const simtime_t getNonHTShortTrainingSequenceDuration() const { return 8E-6; }
    virtual const simtime_t getNonHTLongTrainingFieldDuration() const { return 8E-6; }
    virtual const simtime_t getNonHTSignalField() const { return 4E-6; }
    virtual const simtime_t getHeSignalFieldA() const { return 8E-6; }
    virtual const simtime_t getHeShortTrainingFieldDuration() const { return 4E-6; }
    virtual const simtime_t getHeSignalFieldB() const { return 4E-6; }

    virtual const simtime_t getDuration() const override;

    virtual Ptr<Ieee80211PhyPreamble> createPreamble() const override { return makeShared<Ieee80211VhtPhyPreamble>(); }
};

/** One HE MCS definition: per-stream modulation, FEC, and channel bandwidth. */
class INET_API Ieee80211Hemcs
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
    Ieee80211Hemcs(unsigned int mcsIndex, const ApskModulationBase *stream1SubcarrierModulation, const Ieee80211ConvolutionalCode* convolutionalCode, Hz bandwidth);
    Ieee80211Hemcs(unsigned int mcsIndex, const ApskModulationBase *stream1SubcarrierModulation, const ApskModulationBase *stream2SubcarrierModulation, const Ieee80211ConvolutionalCode* convolutionalCode, Hz bandwidth);
    Ieee80211Hemcs(unsigned int mcsIndex, const ApskModulationBase *stream1SubcarrierModulation, const Ieee80211ConvolutionalCode* convolutionalCode, Hz bandwidth, int nss);
    virtual ~Ieee80211Hemcs();

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
    virtual unsigned int getNumNss() const {
        return (stream1Modulation ? 1 : 0) + (stream2Modulation ? 1 : 0) +
               (stream3Modulation ? 1 : 0) + (stream4Modulation ? 1 : 0) +
               (stream5Modulation ? 1 : 0) + (stream6Modulation ? 1 : 0) +
               (stream7Modulation ? 1 : 0) + (stream8Modulation ? 1 : 0);
    }
};

/** HE PSDU data mode and airtime calculator for the SU mode-set path. */
class INET_API Ieee80211HeDataMode : public IIeee80211DataMode, public Ieee80211HeModeBase, public Ieee80211HeTimingRelatedParametersBase
{
  protected:
    const Ieee80211Hemcs *modulationAndCodingScheme;
    const unsigned int numberOfBccEncoders;
    const bool ldpc;
    const Ieee80211VhtCode *ldpcCode = nullptr;

  protected:
    bps computeGrossBitrate() const override;
    bps computeNetBitrate() const override;
    unsigned int computeNumberOfSpatialStreams(const Ieee80211Hemcs *) const;
    unsigned int computeNumberOfCodedBitsPerSubcarrierSum() const;
    unsigned int computeNumberOfBccEncoders() const;

  public:
    Ieee80211HeDataMode(const Ieee80211Hemcs *modulationAndCodingScheme, const Hz bandwidth, GuardIntervalType guardIntervalType, bool ldpc = false);
    virtual ~Ieee80211HeDataMode() { delete ldpcCode; }

    b getServiceFieldLength() const { return b(16); }
    b getTailFieldLength() const { return ldpc ? b(0) : b(6) * numberOfBccEncoders; }
    bool isLdpc() const { return ldpc; }

    virtual int getNumberOfSpatialStreams() const override { return Ieee80211HeModeBase::getNumberOfSpatialStreams(); }
    virtual Hz getBandwidth() const override { return bandwidth; }
    virtual b getPaddingLength(b dataLength) const override { return b(0); }
    virtual b getCompleteLength(b dataLength) const override;
    virtual const simtime_t getDuration(b dataLength) const override;
    virtual bps getNetBitrate() const override { return Ieee80211HeModeBase::getNetBitrate(); }
    virtual bps getGrossBitrate() const override { return Ieee80211HeModeBase::getGrossBitrate(); }
    virtual const Ieee80211Hemcs *getModulationAndCodingScheme() const { return modulationAndCodingScheme; }
    virtual const Ieee80211VhtCode *getCode() const { return ldpc ? ldpcCode : modulationAndCodingScheme->getCode(); }
    virtual const simtime_t getSymbolInterval() const override {
        switch (guardIntervalType) {
            case HE_GUARD_INTERVAL_SHORT: return getShortGISymbolInterval();
            case HE_GUARD_INTERVAL_MEDIUM: return getMediumGISymbolInterval();
            case HE_GUARD_INTERVAL_LONG: return Ieee80211HeTimingRelatedParametersBase::getSymbolInterval();
            default: throw cRuntimeError("Unknown HE guard interval");
        }
    }
    virtual const Ieee80211OfdmModulation *getModulation() const override { return modulationAndCodingScheme->getModulation(); }
};

/** Complete HE PHY mode composed of its preamble and PSDU data mode. */
class INET_API Ieee80211HeMode : public Ieee80211ModeBase
{
  public:
    enum BandMode {
        BAND_2_4GHZ,
        BAND_5GHZ
    };

  protected:
    const Ieee80211HePreambleMode *preambleMode;
    const Ieee80211HeDataMode *dataMode;
    const BandMode centerFrequencyMode;

  protected:
    virtual int getLegacyCwMin() const override { return 15; }
    virtual int getLegacyCwMax() const override { return 1023; }

  public:
    Ieee80211HeMode(const char *name, const Ieee80211HePreambleMode *preambleMode, const Ieee80211HeDataMode *dataMode, const BandMode centerFrequencyMode);
    virtual ~Ieee80211HeMode() { delete preambleMode; delete dataMode; }

    virtual const Ieee80211HeDataMode *getDataMode() const override { return dataMode; }
    virtual const Ieee80211HePreambleMode *getPreambleMode() const override { return preambleMode; }
    virtual const Ieee80211HeSignalMode *getHeaderMode() const override { return preambleMode->getSignalMode(); }
    virtual const Ieee80211OfdmSignalMode *getLegacySignalMode() const { return preambleMode->getLegacySignalMode(); }

    virtual const simtime_t getSlotTime() const override;
    virtual const simtime_t getSifsTime() const override;
    virtual const simtime_t getRifsTime() const override { return 2E-6; }
    virtual const simtime_t getCcaTime() const override { return 4E-6; }
    virtual const simtime_t getPhyRxStartDelay() const override { return 33E-6; }
    virtual const simtime_t getRxTxTurnaroundTime() const override { return 2E-6; }
    virtual const simtime_t getPreambleLength() const override { return 16E-6; }
    virtual const simtime_t getPlcpHeaderLength() const override { return 4E-6; }
    virtual int getMpduMaxLength() const override { return 65535; }
    virtual BandMode getCenterFrequencyMode() const { return centerFrequencyMode; }

    virtual const simtime_t getDuration(b dataBitLength) const override { return preambleMode->getDuration() + dataMode->getDuration(dataBitLength); }
};

/** Lookup table for standard HE MCS combinations. */
class INET_API Ieee80211HemcsTable
{  public:
#define HE_DECLARE_MCS(WIDTH, NSS, MCS) static const DI<Ieee80211Hemcs> heMcs##MCS##BW##WIDTH##MHzNss##NSS;
#define HE_DECLARE_MCS_FOR_NSS(WIDTH, NSS) \
    HE_DECLARE_MCS(WIDTH, NSS, 0) \
    HE_DECLARE_MCS(WIDTH, NSS, 1) \
    HE_DECLARE_MCS(WIDTH, NSS, 2) \
    HE_DECLARE_MCS(WIDTH, NSS, 3) \
    HE_DECLARE_MCS(WIDTH, NSS, 4) \
    HE_DECLARE_MCS(WIDTH, NSS, 5) \
    HE_DECLARE_MCS(WIDTH, NSS, 6) \
    HE_DECLARE_MCS(WIDTH, NSS, 7) \
    HE_DECLARE_MCS(WIDTH, NSS, 8) \
    HE_DECLARE_MCS(WIDTH, NSS, 9) \
    HE_DECLARE_MCS(WIDTH, NSS, 10) \
    HE_DECLARE_MCS(WIDTH, NSS, 11)
#define HE_DECLARE_MCS_FOR_BW(WIDTH) \
    HE_DECLARE_MCS_FOR_NSS(WIDTH, 1) \
    HE_DECLARE_MCS_FOR_NSS(WIDTH, 2) \
    HE_DECLARE_MCS_FOR_NSS(WIDTH, 3) \
    HE_DECLARE_MCS_FOR_NSS(WIDTH, 4) \
    HE_DECLARE_MCS_FOR_NSS(WIDTH, 5) \
    HE_DECLARE_MCS_FOR_NSS(WIDTH, 6) \
    HE_DECLARE_MCS_FOR_NSS(WIDTH, 7) \
    HE_DECLARE_MCS_FOR_NSS(WIDTH, 8)

    HE_DECLARE_MCS_FOR_BW(20)
    HE_DECLARE_MCS_FOR_BW(40)
    HE_DECLARE_MCS_FOR_BW(80)
    HE_DECLARE_MCS_FOR_BW(160)

#undef HE_DECLARE_MCS_FOR_BW
#undef HE_DECLARE_MCS_FOR_NSS
#undef HE_DECLARE_MCS
};

#undef DI   // was: #define DI DelayedInitializer — limit scope to this header

/** Factory and cache for standard HE PHY modes. */
class INET_API Ieee80211HeCompliantModes
{
  protected:
    static OPP_THREAD_LOCAL const Ieee80211HeCompliantModes singleton;

    mutable std::map<std::tuple<Hz, unsigned int, Ieee80211HeModeBase::GuardIntervalType,
            unsigned int, Ieee80211HeMode::BandMode,
            Ieee80211HePreambleMode::HighEfficiencyPreambleFormat,
            bool>, const Ieee80211HeMode *> modeCache;

  public:
    Ieee80211HeCompliantModes();
    virtual ~Ieee80211HeCompliantModes();

    static const Ieee80211HeMode *getCompliantMode(const Ieee80211Hemcs *mcsMode, Ieee80211HeMode::BandMode centerFrequencyMode, Ieee80211HePreambleMode::HighEfficiencyPreambleFormat preambleFormat, Ieee80211HeModeBase::GuardIntervalType guardIntervalType, bool ldpc = false);
};

} // namespace physicallayer
} // namespace inet

#endif
