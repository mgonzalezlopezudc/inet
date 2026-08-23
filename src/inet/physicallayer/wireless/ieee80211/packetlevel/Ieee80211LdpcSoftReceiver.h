//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211LDPCSOFTRECEIVER_H
#define __INET_IEEE80211LDPCSOFTRECEIVER_H

#include <map>
#include <memory>
#include <complex>

#include "inet/physicallayer/wireless/common/base/packetlevel/ApskModulationBase.h"
#include "inet/physicallayer/wireless/ieee80211/bitlevel/Ieee80211LdpcDataPipeline.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211LdpcSoftTransmissionModel.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"

namespace inet {
namespace physicallayer {

class INET_API Ieee80211LdpcSoftReceiver : public Ieee80211Receiver
{
  public:
    struct ReceptionContext {
        const IIeee80211Mode *mode = nullptr;
        std::unique_ptr<Ieee80211DataEncodingPlan> plan;
        std::vector<const ApskModulationBase *> streamModulations;
        std::vector<int> bitsPerSubcarrier;
        int bandwidthMhz = 0;
        int psduOctets = 0;
        uint8_t vhtSigBCrc = 0;
        bool isVht = false;
    };

    struct ExactDecodeOutcome {
        bool success = false;
        BitVector psduBits;
        int iterations = 0;
    };

  protected:
    LdpcDecodingAlgorithm decodingAlgorithm = LdpcDecodingAlgorithm::SUM_PRODUCT;
    int maxIterations = 20;
    double normalizedMinSumFactor = 0.75;
    double maximumLlr = 20.0;

    static simsignal_t ldpcDataDecodeAttemptedSignal;
    static simsignal_t ldpcDataDecodeSucceededSignal;
    static simsignal_t ldpcDataDecodeIterationsSignal;
    mutable std::map<int, ExactDecodeOutcome> exactReceptionOutcomes;

  protected:
    virtual void initialize(int stage) override;

  public:
    /**
     * Resolves HT/VHT context from the received PHY header, receiver-visible
     * data duration, and local capabilities. For VHT-SU, the exact receive
     * symbol count is reconstructed from duration and the received
     * LDPC-extra-symbol indication (IEEE Std 802.11-2024, §21.3.20,
     * Eqs. 21-104..108); VHT-SIG-B supplies only the rounded APEP indication.
     * mappedSymbolCount is an optional consistency check, never an authority.
     */
    static bool resolveReceptionContext(const Packet *packet, const Ieee80211ModeSet *modeSet,
            simtime_t dataDuration, ReceptionContext& context,
            int mappedSymbolCount = -1);

    /** Computes one exact natural-log bit LLR using stable log-sum-exp. */
    static double computeBitLlr(const std::complex<double>& observation,
            const ApskModulationBase *modulation, double noiseSpectralDensity,
            int bit, double maximumLlr);

  protected:
    static uint8_t computeVhtSigBCrc(const Ieee80211VhtPhyHeader *header);

    bool computeExactDataSuccess(const IReception *reception, const ISnir *snir,
            const Ieee80211LdpcSoftTransmissionModel *signalModel) const;
    bool computeExactPreambleOrHeaderSuccess(const IListening *listening,
            const IReception *reception, IRadioSignal::SignalPart part,
            const IInterference *interference, const ISnir *snir,
            const IIeee80211Mode *receiverMode) const;

    virtual bool computeIsReceptionPossible(const IListening *listening,
            const ITransmission *transmission) const override;
    virtual bool computeIsReceptionPossible(const IListening *listening,
            const IReception *reception, IRadioSignal::SignalPart part) const override;
    virtual const IReceptionDecision *computeReceptionDecision(const IListening *listening,
            const IReception *reception, IRadioSignal::SignalPart part,
            const IInterference *interference, const ISnir *snir) const override;
    virtual const IReceptionResult *computeReceptionResult(const IListening *listening,
            const IReception *reception, const IInterference *interference,
            const ISnir *snir, const std::vector<const IReceptionDecision *> *decisions) const override;
    virtual Packet *computeReceivedPacket(const ISnir *snir, bool isReceptionSuccessful) const override;

  public:
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override;
};

} // namespace physicallayer
} // namespace inet

#endif
