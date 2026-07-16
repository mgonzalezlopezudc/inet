//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211RBIRERRORMODEL_H
#define __INET_IEEE80211RBIRERRORMODEL_H

#include <array>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211NistErrorModel.h"

namespace inet {
namespace physicallayer {

/**
 * External calibration tables and interpolation for the RBIR PHY abstraction
 * described by IEEE 11-14/0571r12, Appendices 1 and 3.
 */
class INET_API Ieee80211RbirCalibration
{
  public:
    enum class Modulation { BPSK, QPSK, QAM16, QAM64, QAM256 };
    enum class Coding { BCC, LDPC };

  protected:
    struct Curve {
        std::vector<double> arguments;
        std::vector<double> values;
        mutable std::vector<double> monotonicValues;
    };

    std::map<Modulation, Curve> rbirCurves;
    std::map<std::tuple<Coding, int, int>, Curve> perCurves;

  protected:
    static double interpolate(const Curve& curve, double argument);
    static double interpolate(const std::vector<double>& arguments,
            const std::vector<double>& values, double argument);
    static const std::vector<double>& getMonotonicValues(const Curve& curve);
    static void validateCurve(const Curve& curve, const char *name, double expectedStep = NaN);
    static Modulation parseModulation(const std::string& text);
    static Coding parseCoding(const std::string& text);
    void sortAndValidateCurves();

  public:
    void clear();
    void load(const char *fileName, bool requireTgaxCompleteness = true);
    void validateTgaxCompleteness() const;

    void addRbirPoint(Modulation modulation, double snrDb, double rbir);
    void addPerPoint(Coding coding, int referencePacketLength, int mcs, double snrDb, double per);

    double mapSnirToRbir(Modulation modulation, double snrDb) const;
    double mapRbirToSnir(Modulation modulation, double rbir) const;
    double computeEffectiveSnirDb(Modulation modulation, const std::vector<double>& snirDb) const;
    double lookupPer(Coding coding, int mcs, int packetLength, double effectiveSnirDb) const;
};

/**
 * Opt-in single-spatial-stream HE MCS 0-9 RBIR packet error model. Header and
 * non-HE data retain the inherited NIST behavior.
 */
class INET_API Ieee80211RbirErrorModel : public Ieee80211NistErrorModel
{
  protected:
    Ieee80211RbirCalibration calibration;

  protected:
    virtual void initialize(int stage) override;

    virtual double getDataSuccessRate(const IIeee80211Mode *mode, unsigned int bitLength,
            const ISnir *snir, double scalarSnir) const override;
    virtual double getHeDataSuccessRate(const Ieee80211HeUserPhyParameters& parameters,
            unsigned int bitLength, const ISnir *snir, double scalarSnir) const override;

    virtual std::vector<double> sampleHeSnirDb(const ISnir *snir, int toneSize,
            int toneOffset, double scalarSnir) const;
    virtual void validateStationaryToneSnir(double minimum, double maximum) const;
    virtual double computeHeSuccessRate(int mcs, Ieee80211RbirCalibration::Coding coding,
            int numberOfSpatialStreams, bool dcm, int packetLength,
            int toneSize, int toneOffset, const ISnir *snir, double scalarSnir) const;

  public:
    virtual std::ostream& printToStream(std::ostream& stream, int level,
            int evFlags = 0) const override { return stream << "Ieee80211RbirErrorModel"; }
};

} // namespace physicallayer
} // namespace inet

#endif
