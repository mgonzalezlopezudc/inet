//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_MULTIBANDFUNCTION_H
#define __INET_MULTIBANDFUNCTION_H

#include <algorithm>
#include <vector>

#include "inet/common/math/Functions.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/FrequencyBand.h"

namespace inet {
namespace physicallayer {

using namespace inet::math;

/**
 * A frequency-union mask over component functions. Each component is
 * evaluated only inside its corresponding occupied band, so the value in
 * every spectral gap is zero. The dimensional PSD path uses SummedFunction
 * when component skirts must be preserved; this helper is intentionally the
 * strict occupied-union view used for masks and union-aware statistics.
 */
template<typename R>
class INET_API MultibandFunction : public FunctionBase<R, Domain<simsec, Hz>>
{
  protected:
    const std::vector<FrequencyBand> occupiedBands;
    const std::vector<Ptr<const IFunction<R, Domain<simsec, Hz>>>> components;

  protected:
    static std::vector<Ptr<const IFunction<R, Domain<simsec, Hz>>>> normalizeComponents(const std::vector<FrequencyBand>& bands,
            const std::vector<Ptr<const IFunction<R, Domain<simsec, Hz>>>>& components)
    {
        auto normalizedBands = normalizeFrequencyBands(bands);
        if (normalizedBands.size() != components.size())
            return components;
        std::vector<Ptr<const IFunction<R, Domain<simsec, Hz>>>> result;
        result.reserve(components.size());
        for (const auto& normalizedBand : normalizedBands) {
            auto it = std::find_if(bands.begin(), bands.end(), [&] (const FrequencyBand& band) {
                return band.centerFrequency == normalizedBand.centerFrequency && band.bandwidth == normalizedBand.bandwidth;
            });
            if (it == bands.end())
                throw cRuntimeError("Multiband function band/component pairing is inconsistent");
            result.push_back(components[it - bands.begin()]);
        }
        return result;
    }

    int findBand(Hz frequency) const {
        for (int i = 0; i < (int)occupiedBands.size(); ++i)
            if (occupiedBands[i].getLowerFrequency() <= frequency && frequency < occupiedBands[i].getUpperFrequency())
                return i;
        return -1;
    }

    Interval<simsec, Hz> getBandIntersection(const Interval<simsec, Hz>& interval, const FrequencyBand& band) const {
        const auto& lower = interval.getLower();
        const auto& upper = interval.getUpper();
        Point<simsec, Hz> bandLower(std::get<0>(lower), band.getLowerFrequency());
        Point<simsec, Hz> bandUpper(std::get<0>(upper), band.getUpperFrequency());
        unsigned char lowerClosed = (interval.getLowerClosed() & 0b10) | 0b01;
        unsigned char upperClosed = (interval.getUpperClosed() & 0b10);
        unsigned char fixed = interval.getFixed() & 0b10;
        return interval.getIntersected(Interval<simsec, Hz>(bandLower, bandUpper, lowerClosed, upperClosed, fixed));
    }

    bool hasGap(const Interval<simsec, Hz>& interval) const {
        if (interval.getFixed() & 0b01)
            return findBand(std::get<1>(interval.getLower())) < 0;
        Hz cursor = std::get<1>(interval.getLower());
        Hz upper = std::get<1>(interval.getUpper());
        for (const auto& band : occupiedBands) {
            if (band.getUpperFrequency() <= cursor)
                continue;
            if (band.getLowerFrequency() > cursor)
                return true;
            cursor = std::max(cursor, band.getUpperFrequency());
            if (cursor >= upper)
                return false;
        }
        return cursor < upper;
    }

  public:
    MultibandFunction(const std::vector<FrequencyBand>& occupiedBands,
            const std::vector<Ptr<const IFunction<R, Domain<simsec, Hz>>>>& components) :
        occupiedBands(normalizeFrequencyBands(occupiedBands)),
        components(normalizeComponents(occupiedBands, components))
    {
        if (this->occupiedBands.size() != this->components.size())
            throw cRuntimeError("A multiband function requires one component per occupied band");
        for (const auto& component : this->components)
            if (component == nullptr)
                throw cRuntimeError("A multiband function cannot contain a null component");
    }

    const std::vector<FrequencyBand>& getOccupiedBands() const { return occupiedBands; }
    const std::vector<Ptr<const IFunction<R, Domain<simsec, Hz>>>>& getComponents() const { return components; }

    virtual typename Domain<simsec, Hz>::I getDomain() const override {
        return typename Domain<simsec, Hz>::I(Domain<simsec, Hz>::P::getZero(), Domain<simsec, Hz>::P::getUpperBounds(), 0b11, 0b00, 0b00);
    }

    virtual R getValue(const typename Domain<simsec, Hz>::P& point) const override {
        int index = findBand(std::get<1>(point));
        return index < 0 ? R(0) : components[index]->getValue(point);
    }

    virtual void partition(const typename Domain<simsec, Hz>::I& interval,
            const std::function<void(const typename Domain<simsec, Hz>::I&, const IFunction<R, Domain<simsec, Hz>> *)> callback) const override {
        const auto& lower = interval.getLower();
        const auto& upper = interval.getUpper();
        if (interval.getFixed() & 0b01) {
            int index = findBand(std::get<1>(lower));
            if (index < 0) {
                ConstantFunction<R, Domain<simsec, Hz>> zero(R(0));
                callback(interval, &zero);
            }
            else
                components[index]->partition(interval, callback);
            return;
        }

        std::vector<Hz> boundaries{std::get<1>(lower), std::get<1>(upper)};
        for (const auto& band : occupiedBands) {
            if (std::get<1>(lower) < band.getLowerFrequency() && band.getLowerFrequency() < std::get<1>(upper))
                boundaries.push_back(band.getLowerFrequency());
            if (std::get<1>(lower) < band.getUpperFrequency() && band.getUpperFrequency() < std::get<1>(upper))
                boundaries.push_back(band.getUpperFrequency());
        }
        std::sort(boundaries.begin(), boundaries.end());
        boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
        for (size_t i = 0; i + 1 < boundaries.size(); ++i) {
            Hz bandLower = boundaries[i];
            Hz bandUpper = boundaries[i + 1];
            if (bandLower == bandUpper)
                continue;
            Hz middle = (bandLower + bandUpper) / 2;
            int index = findBand(middle);
            Point<simsec, Hz> sliceLower(std::get<0>(lower), bandLower);
            Point<simsec, Hz> sliceUpper(std::get<0>(upper), bandUpper);
            unsigned char lowerClosed = (interval.getLowerClosed() & 0b10) | (i == 0 ? 0b01 : 0b01);
            unsigned char upperClosed = (interval.getUpperClosed() & 0b10) | (i + 2 == boundaries.size() ? 0 : 0);
            Interval<simsec, Hz> slice(sliceLower, sliceUpper, lowerClosed, upperClosed, interval.getFixed() & 0b10);
            if (index < 0) {
                ConstantFunction<R, Domain<simsec, Hz>> zero(R(0));
                callback(slice, &zero);
            }
            else
                components[index]->partition(slice, callback);
        }
    }

    virtual bool isFinite(const typename Domain<simsec, Hz>::I& interval) const override {
        for (size_t i = 0; i < components.size(); ++i) {
            auto intersection = getBandIntersection(interval, occupiedBands[i]);
            if (!intersection.isEmpty() && !components[i]->isFinite(intersection))
                return false;
        }
        return true;
    }

    virtual bool isNonZero(const typename Domain<simsec, Hz>::I& interval) const override {
        if (hasGap(interval))
            return false;
        bool hasIntersection = false;
        for (size_t i = 0; i < components.size(); ++i) {
            auto intersection = getBandIntersection(interval, occupiedBands[i]);
            if (!intersection.isEmpty()) {
                hasIntersection = true;
                if (!components[i]->isNonZero(intersection))
                    return false;
            }
        }
        return hasIntersection;
    }

    virtual R getMin(const typename Domain<simsec, Hz>::I& interval) const override {
        if (hasGap(interval))
            return R(0);
        R result = getUpperBound<R>();
        bool hasIntersection = false;
        for (size_t i = 0; i < components.size(); ++i) {
            auto intersection = getBandIntersection(interval, occupiedBands[i]);
            if (!intersection.isEmpty()) {
                hasIntersection = true;
                result = minnan(result, components[i]->getMin(intersection));
            }
        }
        return hasIntersection ? result : R(0);
    }

    virtual R getMax(const typename Domain<simsec, Hz>::I& interval) const override {
        R result = getLowerBound<R>();
        bool hasIntersection = false;
        for (size_t i = 0; i < components.size(); ++i) {
            auto intersection = getBandIntersection(interval, occupiedBands[i]);
            if (!intersection.isEmpty()) {
                hasIntersection = true;
                result = maxnan(result, components[i]->getMax(intersection));
            }
        }
        // A gap is an explicitly masked zero, so it participates in extrema
        // even when a component happens to contain negative values.
        if (hasGap(interval))
            result = maxnan(result, R(0));
        return hasIntersection || hasGap(interval) ? result : R(0);
    }

    virtual R getIntegral(const typename Domain<simsec, Hz>::I& interval) const override {
        R result = R(0);
        for (size_t i = 0; i < components.size(); ++i) {
            auto intersection = getBandIntersection(interval, occupiedBands[i]);
            if (!intersection.isEmpty())
                result += components[i]->getIntegral(intersection);
        }
        return result;
    }

    virtual R getMean(const typename Domain<simsec, Hz>::I& interval) const override {
        double occupiedVolume = 0;
        for (size_t i = 0; i < occupiedBands.size(); ++i) {
            auto intersection = getBandIntersection(interval, occupiedBands[i]);
            if (!intersection.isEmpty())
                occupiedVolume += intersection.getVolume();
        }
        return occupiedVolume == 0 ? R(0) : getIntegral(interval) / occupiedVolume;
    }

    virtual void printStructure(std::ostream& os, int level = 0) const override {
        os << "(MultibandFunction, bands = " << occupiedBands.size() << ")";
    }
};

} // namespace physicallayer
} // namespace inet

#endif
