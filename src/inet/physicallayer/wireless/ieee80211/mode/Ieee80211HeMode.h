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

class INET_API Ieee80211HeModeBase
{
  public:
    enum GuardIntervalType {
        HE_GUARD_INTERVAL_SHORT, // 0.8 µs
        HE_GUARD_INTERVAL_MEDIUM, // 1.6 µs
        HE_GUARD_INTERVAL_LONG   // 3.2 µs
    };

  protected:
    const Hz bandwidth;
    const GuardIntervalType guardIntervalType;
    const unsigned int mcsIndex; // MCS index (0 to 11)
    const unsigned int numberOfSpatialStreams; // N_SS

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

class INET_API Ieee80211HePreambleMode : public IIeee80211PreambleMode, public Ieee80211HeTimingRelatedParametersBase
{
  public:
    enum HighEfficiencyPreambleFormat {
        HE_PREAMBLE_SU,
        HE_PREAMBLE_MU
    };

  protected:
    const Ieee80211HeSignalMode *highEfficiencySignalMode;
    const Ieee80211OfdmSignalMode *legacySignalMode;
    const HighEfficiencyPreambleFormat preambleFormat;
    const unsigned int numberOfHELongTrainings;

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

class INET_API Ieee80211HeDataMode : public IIeee80211DataMode, public Ieee80211HeModeBase, public Ieee80211HeTimingRelatedParametersBase
{
  protected:
    const Ieee80211Hemcs *modulationAndCodingScheme;
    const unsigned int numberOfBccEncoders;

  protected:
    bps computeGrossBitrate() const override;
    bps computeNetBitrate() const override;
    unsigned int computeNumberOfSpatialStreams(const Ieee80211Hemcs *) const;
    unsigned int computeNumberOfCodedBitsPerSubcarrierSum() const;
    unsigned int computeNumberOfBccEncoders() const;

  public:
    Ieee80211HeDataMode(const Ieee80211Hemcs *modulationAndCodingScheme, const Hz bandwidth, GuardIntervalType guardIntervalType);

    b getServiceFieldLength() const { return b(16); }
    b getTailFieldLength() const { return b(6) * numberOfBccEncoders; }

    virtual int getNumberOfSpatialStreams() const override { return Ieee80211HeModeBase::getNumberOfSpatialStreams(); }
    virtual Hz getBandwidth() const override { return bandwidth; }
    virtual b getPaddingLength(b dataLength) const override { return b(0); }
    virtual b getCompleteLength(b dataLength) const override;
    virtual const simtime_t getDuration(b dataLength) const override;
    virtual bps getNetBitrate() const override { return Ieee80211HeModeBase::getNetBitrate(); }
    virtual bps getGrossBitrate() const override { return Ieee80211HeModeBase::getGrossBitrate(); }
    virtual const Ieee80211Hemcs *getModulationAndCodingScheme() const { return modulationAndCodingScheme; }
    virtual const Ieee80211VhtCode *getCode() const { return modulationAndCodingScheme->getCode(); }
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

class INET_API Ieee80211HemcsTable
{  public:
    // BW=20MHz, NSS=1
    static const DI<Ieee80211Hemcs> heMcs0BW20MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs1BW20MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs2BW20MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs3BW20MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs4BW20MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs5BW20MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs6BW20MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs7BW20MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs8BW20MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs9BW20MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs10BW20MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs11BW20MHzNss1;
    // BW=20MHz, NSS=2
    static const DI<Ieee80211Hemcs> heMcs0BW20MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs1BW20MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs2BW20MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs3BW20MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs4BW20MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs5BW20MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs6BW20MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs7BW20MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs8BW20MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs9BW20MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs10BW20MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs11BW20MHzNss2;
    // BW=20MHz, NSS=3
    static const DI<Ieee80211Hemcs> heMcs0BW20MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs1BW20MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs2BW20MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs3BW20MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs4BW20MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs5BW20MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs6BW20MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs7BW20MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs8BW20MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs9BW20MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs10BW20MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs11BW20MHzNss3;
    // BW=20MHz, NSS=4
    static const DI<Ieee80211Hemcs> heMcs0BW20MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs1BW20MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs2BW20MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs3BW20MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs4BW20MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs5BW20MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs6BW20MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs7BW20MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs8BW20MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs9BW20MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs10BW20MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs11BW20MHzNss4;
    // BW=20MHz, NSS=5
    static const DI<Ieee80211Hemcs> heMcs0BW20MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs1BW20MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs2BW20MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs3BW20MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs4BW20MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs5BW20MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs6BW20MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs7BW20MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs8BW20MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs9BW20MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs10BW20MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs11BW20MHzNss5;
    // BW=20MHz, NSS=6
    static const DI<Ieee80211Hemcs> heMcs0BW20MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs1BW20MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs2BW20MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs3BW20MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs4BW20MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs5BW20MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs6BW20MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs7BW20MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs8BW20MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs9BW20MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs10BW20MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs11BW20MHzNss6;
    // BW=20MHz, NSS=7
    static const DI<Ieee80211Hemcs> heMcs0BW20MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs1BW20MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs2BW20MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs3BW20MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs4BW20MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs5BW20MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs6BW20MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs7BW20MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs8BW20MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs9BW20MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs10BW20MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs11BW20MHzNss7;
    // BW=20MHz, NSS=8
    static const DI<Ieee80211Hemcs> heMcs0BW20MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs1BW20MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs2BW20MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs3BW20MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs4BW20MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs5BW20MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs6BW20MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs7BW20MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs8BW20MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs9BW20MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs10BW20MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs11BW20MHzNss8;
    // BW=40MHz, NSS=1
    static const DI<Ieee80211Hemcs> heMcs0BW40MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs1BW40MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs2BW40MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs3BW40MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs4BW40MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs5BW40MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs6BW40MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs7BW40MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs8BW40MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs9BW40MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs10BW40MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs11BW40MHzNss1;
    // BW=40MHz, NSS=2
    static const DI<Ieee80211Hemcs> heMcs0BW40MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs1BW40MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs2BW40MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs3BW40MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs4BW40MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs5BW40MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs6BW40MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs7BW40MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs8BW40MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs9BW40MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs10BW40MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs11BW40MHzNss2;
    // BW=40MHz, NSS=3
    static const DI<Ieee80211Hemcs> heMcs0BW40MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs1BW40MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs2BW40MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs3BW40MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs4BW40MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs5BW40MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs6BW40MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs7BW40MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs8BW40MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs9BW40MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs10BW40MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs11BW40MHzNss3;
    // BW=40MHz, NSS=4
    static const DI<Ieee80211Hemcs> heMcs0BW40MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs1BW40MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs2BW40MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs3BW40MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs4BW40MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs5BW40MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs6BW40MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs7BW40MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs8BW40MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs9BW40MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs10BW40MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs11BW40MHzNss4;
    // BW=40MHz, NSS=5
    static const DI<Ieee80211Hemcs> heMcs0BW40MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs1BW40MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs2BW40MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs3BW40MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs4BW40MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs5BW40MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs6BW40MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs7BW40MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs8BW40MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs9BW40MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs10BW40MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs11BW40MHzNss5;
    // BW=40MHz, NSS=6
    static const DI<Ieee80211Hemcs> heMcs0BW40MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs1BW40MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs2BW40MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs3BW40MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs4BW40MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs5BW40MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs6BW40MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs7BW40MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs8BW40MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs9BW40MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs10BW40MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs11BW40MHzNss6;
    // BW=40MHz, NSS=7
    static const DI<Ieee80211Hemcs> heMcs0BW40MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs1BW40MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs2BW40MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs3BW40MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs4BW40MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs5BW40MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs6BW40MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs7BW40MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs8BW40MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs9BW40MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs10BW40MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs11BW40MHzNss7;
    // BW=40MHz, NSS=8
    static const DI<Ieee80211Hemcs> heMcs0BW40MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs1BW40MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs2BW40MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs3BW40MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs4BW40MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs5BW40MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs6BW40MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs7BW40MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs8BW40MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs9BW40MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs10BW40MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs11BW40MHzNss8;
    // BW=80MHz, NSS=1
    static const DI<Ieee80211Hemcs> heMcs0BW80MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs1BW80MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs2BW80MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs3BW80MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs4BW80MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs5BW80MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs6BW80MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs7BW80MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs8BW80MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs9BW80MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs10BW80MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs11BW80MHzNss1;
    // BW=80MHz, NSS=2
    static const DI<Ieee80211Hemcs> heMcs0BW80MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs1BW80MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs2BW80MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs3BW80MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs4BW80MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs5BW80MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs6BW80MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs7BW80MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs8BW80MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs9BW80MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs10BW80MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs11BW80MHzNss2;
    // BW=80MHz, NSS=3
    static const DI<Ieee80211Hemcs> heMcs0BW80MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs1BW80MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs2BW80MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs3BW80MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs4BW80MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs5BW80MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs6BW80MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs7BW80MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs8BW80MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs9BW80MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs10BW80MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs11BW80MHzNss3;
    // BW=80MHz, NSS=4
    static const DI<Ieee80211Hemcs> heMcs0BW80MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs1BW80MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs2BW80MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs3BW80MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs4BW80MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs5BW80MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs6BW80MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs7BW80MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs8BW80MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs9BW80MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs10BW80MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs11BW80MHzNss4;
    // BW=80MHz, NSS=5
    static const DI<Ieee80211Hemcs> heMcs0BW80MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs1BW80MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs2BW80MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs3BW80MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs4BW80MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs5BW80MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs6BW80MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs7BW80MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs8BW80MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs9BW80MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs10BW80MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs11BW80MHzNss5;
    // BW=80MHz, NSS=6
    static const DI<Ieee80211Hemcs> heMcs0BW80MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs1BW80MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs2BW80MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs3BW80MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs4BW80MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs5BW80MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs6BW80MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs7BW80MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs8BW80MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs9BW80MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs10BW80MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs11BW80MHzNss6;
    // BW=80MHz, NSS=7
    static const DI<Ieee80211Hemcs> heMcs0BW80MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs1BW80MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs2BW80MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs3BW80MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs4BW80MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs5BW80MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs6BW80MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs7BW80MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs8BW80MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs9BW80MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs10BW80MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs11BW80MHzNss7;
    // BW=80MHz, NSS=8
    static const DI<Ieee80211Hemcs> heMcs0BW80MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs1BW80MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs2BW80MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs3BW80MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs4BW80MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs5BW80MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs6BW80MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs7BW80MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs8BW80MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs9BW80MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs10BW80MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs11BW80MHzNss8;
    // BW=160MHz, NSS=1
    static const DI<Ieee80211Hemcs> heMcs0BW160MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs1BW160MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs2BW160MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs3BW160MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs4BW160MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs5BW160MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs6BW160MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs7BW160MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs8BW160MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs9BW160MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs10BW160MHzNss1;
    static const DI<Ieee80211Hemcs> heMcs11BW160MHzNss1;
    // BW=160MHz, NSS=2
    static const DI<Ieee80211Hemcs> heMcs0BW160MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs1BW160MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs2BW160MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs3BW160MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs4BW160MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs5BW160MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs6BW160MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs7BW160MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs8BW160MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs9BW160MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs10BW160MHzNss2;
    static const DI<Ieee80211Hemcs> heMcs11BW160MHzNss2;
    // BW=160MHz, NSS=3
    static const DI<Ieee80211Hemcs> heMcs0BW160MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs1BW160MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs2BW160MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs3BW160MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs4BW160MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs5BW160MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs6BW160MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs7BW160MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs8BW160MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs9BW160MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs10BW160MHzNss3;
    static const DI<Ieee80211Hemcs> heMcs11BW160MHzNss3;
    // BW=160MHz, NSS=4
    static const DI<Ieee80211Hemcs> heMcs0BW160MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs1BW160MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs2BW160MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs3BW160MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs4BW160MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs5BW160MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs6BW160MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs7BW160MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs8BW160MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs9BW160MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs10BW160MHzNss4;
    static const DI<Ieee80211Hemcs> heMcs11BW160MHzNss4;
    // BW=160MHz, NSS=5
    static const DI<Ieee80211Hemcs> heMcs0BW160MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs1BW160MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs2BW160MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs3BW160MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs4BW160MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs5BW160MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs6BW160MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs7BW160MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs8BW160MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs9BW160MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs10BW160MHzNss5;
    static const DI<Ieee80211Hemcs> heMcs11BW160MHzNss5;
    // BW=160MHz, NSS=6
    static const DI<Ieee80211Hemcs> heMcs0BW160MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs1BW160MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs2BW160MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs3BW160MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs4BW160MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs5BW160MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs6BW160MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs7BW160MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs8BW160MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs9BW160MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs10BW160MHzNss6;
    static const DI<Ieee80211Hemcs> heMcs11BW160MHzNss6;
    // BW=160MHz, NSS=7
    static const DI<Ieee80211Hemcs> heMcs0BW160MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs1BW160MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs2BW160MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs3BW160MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs4BW160MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs5BW160MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs6BW160MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs7BW160MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs8BW160MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs9BW160MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs10BW160MHzNss7;
    static const DI<Ieee80211Hemcs> heMcs11BW160MHzNss7;
    // BW=160MHz, NSS=8
    static const DI<Ieee80211Hemcs> heMcs0BW160MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs1BW160MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs2BW160MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs3BW160MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs4BW160MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs5BW160MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs6BW160MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs7BW160MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs8BW160MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs9BW160MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs10BW160MHzNss8;
    static const DI<Ieee80211Hemcs> heMcs11BW160MHzNss8;
};

class INET_API Ieee80211HeCompliantModes
{
  protected:
    static OPP_THREAD_LOCAL const Ieee80211HeCompliantModes singleton;

    mutable std::map<std::tuple<Hz, unsigned int, Ieee80211HeModeBase::GuardIntervalType,
            unsigned int, Ieee80211HeMode::BandMode,
            Ieee80211HePreambleMode::HighEfficiencyPreambleFormat>, const Ieee80211HeMode *> modeCache;

  public:
    Ieee80211HeCompliantModes();
    virtual ~Ieee80211HeCompliantModes();

    static const Ieee80211HeMode *getCompliantMode(const Ieee80211Hemcs *mcsMode, Ieee80211HeMode::BandMode centerFrequencyMode, Ieee80211HePreambleMode::HighEfficiencyPreambleFormat preambleFormat, Ieee80211HeModeBase::GuardIntervalType guardIntervalType);
};

} // namespace physicallayer
} // namespace inet

#endif
