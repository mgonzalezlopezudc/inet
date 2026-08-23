//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211LdpcPerSuccessModel.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee80211LdpcPerSuccessModel);

void Ieee80211LdpcPerSuccessModel::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        const char *configuredPath = par("perTableFile");
        if (configuredPath == nullptr || *configuredPath == '\0')
            throw cRuntimeError("Ieee80211LdpcPerSuccessModel requires an explicit perTableFile");
        std::string path = resolveResourcePath(configuredPath);
        if (path.empty())
            throw cRuntimeError("Cannot resolve IEEE 802.11 LDPC PER table '%s'", configuredPath);
        table.load(path.c_str());
    }
}

double Ieee80211LdpcPerSuccessModel::computeDataSuccessRate(const IIeee80211DataMode& mode,
        const Ieee80211DataEncodingPlan& plan, double snrDb) const
{
    auto key = makeIeee80211LdpcPerCurveKey(mode, plan);
    return 1.0 - table.getPacketErrorRate(key, snrDb);
}

} // namespace physicallayer
} // namespace inet
