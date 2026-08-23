//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TGNCHANNELMODEL_H
#define __INET_TGNCHANNELMODEL_H

#include <map>
#include <memory>
#include <set>
#include <string>

#include "inet/common/Module.h"
#include "inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixSnapshot.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IWidebandChannelModel.h"
#include "inet/physicallayer/wireless/ieee80211/channelmodel/TgnMimoChannel.h"

namespace inet {
namespace physicallayer {

class INET_API TgnChannelModel : public Module, public IWidebandChannelModel
{
  protected:
    struct LinkKey {
        std::string transmitterId;
        std::string receiverId;

        bool operator<(const LinkKey& other) const {
            return transmitterId < other.transmitterId ||
                (transmitterId == other.transmitterId && receiverId < other.receiverId);
        }
    };

    TgnChannelProfile profile = TgnChannelProfile::create(TgnModel::B);
    TgnCondition condition = TgnCondition::NLOS;
    bool reciprocal = false;
    bool timeVariation = true;
    bool vehicleEffect = true;
    bool fluorescentEffect = true;
    double fluorescentMainsFrequencyHz = NaN;
    double environmentalSpeedMps = NaN;
    double vehicleSpeedMps = NaN;
    double antennaSpacingInWavelengths = NaN;
    int oscillatorCount = -1;
    uint64_t linkMasterSeed = 0;
    uint64_t shadowingMasterSeed = 0;
    uint64_t diffuseMasterSeed = 0;
    uint64_t fluorescentMasterSeed = 0;

    std::set<std::string> registeredRadioIds;
    mutable std::map<LinkKey, std::shared_ptr<const TgnChannelRealization>> linkStates;

  protected:
    virtual void initialize(int stage) override;
    static std::string stableRadioId(const IRadio *radio);
    LinkKey makeLinkKey(const IRadio *transmitter, const IRadio *receiver, bool& transpose) const;
    static uint64_t splitMix64Finalizer(uint64_t value);
    static uint64_t derivePurposeSeed(uint64_t familySeed, const LinkKey& key, TgnModel model,
        TgnCondition condition, int component, int matrixRow, int matrixColumn, const char *temporalEffect);
    static uint64_t extractMasterSeed(cRNG *rng);
    void validateMode(const ITransmission *transmission) const;
    std::shared_ptr<const TgnChannelRealization> createLinkState(const LinkKey& key,
        const IRadio *canonicalTransmitter, const IRadio *canonicalReceiver, Hz referenceFrequency,
        mps propagationSpeed) const;

  public:
    virtual void addRadio(const IRadio *radio) override;
    virtual void removeRadio(const IRadio *radio) override;
    virtual std::shared_ptr<const IChannelMatrixSnapshot> computeChannel(const IRadio *receiver,
        const ITransmission *transmission, const IArrival *arrival) const override;
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
