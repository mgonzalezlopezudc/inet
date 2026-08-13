//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfFeatureSet.h"

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangePlan.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTxopCoordinatorService.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.h"

namespace inet {
namespace ieee80211 {

namespace {

class HcfActionExchangeTransaction final : public IHcfExchangeTransaction
{
  private:
    IHcfFeatureSet::ExchangeCommitter committer;
    HcfExchangeClass exchangeClass;
    HcfContext context;
    bool committed = false;

  public:
    HcfActionExchangeTransaction(const IHcfFeatureSet::ExchangeCommitter& committer,
            HcfExchangeClass exchangeClass, const HcfContext& context) :
        committer(committer), exchangeClass(exchangeClass),
        context(context) {}

    virtual HcfExchangeRejection validate(const HcfExchangePlan& plan) override
    {
        if (!committer || plan.getExchangeClass() != exchangeClass ||
                !plan.getReservations().empty())
            return {HcfExchangeRejectionCode::VALIDATION_FAILED, exchangeClass,
                    plan.getTransactionIdentity(),
                    "invalid HCF action exchange transaction"};
        return {};
    }

    virtual void commit(const HcfExchangePlan&) override
    {
        if (committed)
            throw cRuntimeError("HCF action exchange committed twice");
        committed = true;
        committer(exchangeClass, context);
    }

    virtual void rollback(const HcfExchangePlan&) noexcept override {}
    virtual void complete(const HcfExchangePlan&,
            const HcfExchangeResult&) override {}
};

class HcfActionExchangeProvider final : public IHcfExchangeProvider
{
  private:
    HcfExchangeClass exchangeClass;
    IHcfFeatureSet::ExchangeCommitter committer;

  public:
    explicit HcfActionExchangeProvider(HcfExchangeClass exchangeClass) :
        exchangeClass(exchangeClass) {}

    void configure(const IHcfFeatureSet::ExchangeCommitter& committer)
        { this->committer = committer; }

    virtual HcfExchangeClass getExchangeClass() const override { return exchangeClass; }

    virtual std::unique_ptr<PreparedHcfExchange> prepareExchange(
            const HcfContext& context, HcfTransactionIdentity transactionIdentity,
            HcfExchangeRejection& rejection) override
    {
        const bool vhtClass = exchangeClass == HcfExchangeClass::VHT_GROUP_MANAGEMENT ||
                exchangeClass == HcfExchangeClass::VHT_DL_MULTIUSER ||
                exchangeClass == HcfExchangeClass::VHT_SU_SOUNDING;
        if (vhtClass) {
            auto snapshot = context.findProviderSnapshot<VhtGrantSnapshot>();
            if (snapshot == nullptr || snapshot->exchangeClass != exchangeClass) {
                rejection = HcfExchangeRejection(
                        HcfExchangeRejectionCode::NO_ELIGIBLE_PACKET,
                        exchangeClass, transactionIdentity,
                        "VHT provider does not match the immutable grant outcome");
                return nullptr;
            }
        }
        const bool heClass = exchangeClass == HcfExchangeClass::FORCED_SINGLE_USER ||
                exchangeClass == HcfExchangeClass::HE_UL_TRIGGER ||
                exchangeClass == HcfExchangeClass::HE_SOUNDING ||
                exchangeClass == HcfExchangeClass::RECOVERY_SINGLE_USER ||
                exchangeClass == HcfExchangeClass::HE_DL_MULTIUSER;
        if (heClass) {
            auto snapshot = context.findProviderSnapshot<
                    HeTxopCoordinatorService::GrantSnapshot>();
            if (snapshot == nullptr || snapshot->exchangeClass != exchangeClass) {
                rejection = HcfExchangeRejection(
                        HcfExchangeRejectionCode::NO_ELIGIBLE_PACKET,
                        exchangeClass, transactionIdentity,
                        "HE provider does not match the immutable grant outcome");
                return nullptr;
            }
        }
        auto eligible = context.getExchangeEligibility(exchangeClass);
        auto accessCategory = context.getSelectionAccessCategory();
        if (!eligible.has_value() || !*eligible || !accessCategory.has_value()) {
            rejection = HcfExchangeRejection(
                    HcfExchangeRejectionCode::NO_ELIGIBLE_PACKET,
                    exchangeClass, transactionIdentity,
                    "exchange class is not eligible in the immutable grant snapshot");
            return nullptr;
        }
        if (!committer) {
            rejection = HcfExchangeRejection(
                    HcfExchangeRejectionCode::INVALID_TRANSACTION_STATE,
                    exchangeClass, transactionIdentity,
                    "HCF action exchange provider is not configured");
            return nullptr;
        }
        HcfExchangePlan plan(exchangeClass, transactionIdentity, {});
        rejection = {};
        return std::make_unique<PreparedHcfExchange>(plan,
                std::make_unique<HcfActionExchangeTransaction>(committer,
                        exchangeClass, context));
    }
};

} // namespace

Define_Module(CommonHcfFeatureSet);
Define_Module(VhtHcfFeatureSet);
Define_Module(HeHcfFeatureSet);

HcfExchangeProviderDescriptor CommonHcfFeatureSet::makeActionDescriptor(
        HcfExchangeClass exchangeClass)
{
    auto& provider = actionProviders[exchangeClass];
    if (provider == nullptr) {
        provider = std::make_unique<HcfActionExchangeProvider>(exchangeClass);
        static_cast<HcfActionExchangeProvider *>(provider.get())->configure(
                exchangeCommitter);
    }
    return HcfExchangeProviderDescriptor(exchangeClass, provider.get());
}

void CommonHcfFeatureSet::configureExchangeCommitter(
        const ExchangeCommitter& committer)
{
    exchangeCommitter = committer;
    for (auto& entry : actionProviders)
        static_cast<HcfActionExchangeProvider *>(entry.second.get())->configure(committer);
}

std::vector<HcfExchangeProviderDescriptor>
CommonHcfFeatureSet::getExchangeProviderDescriptors()
{
    std::vector<HcfExchangeProviderDescriptor> descriptors;
    if (getConfiguration().enableHtSounding)
        descriptors.push_back(makeActionDescriptor(HcfExchangeClass::HT_SOUNDING));
    descriptors.push_back(makeActionDescriptor(HcfExchangeClass::SINGLE_USER));
    descriptors.push_back(makeActionDescriptor(HcfExchangeClass::CHANNEL_RELEASE));
    return descriptors;
}

std::vector<HcfExchangeProviderDescriptor>
VhtHcfFeatureSet::getExchangeProviderDescriptors()
{
    auto descriptors = CommonHcfFeatureSet::getExchangeProviderDescriptors();
    if (getConfiguration().enableVhtDlMuMimo) {
        descriptors.push_back(makeActionDescriptor(HcfExchangeClass::VHT_GROUP_MANAGEMENT));
        // This class includes the current VHT MU-sounding prerequisite.
        descriptors.push_back(makeActionDescriptor(HcfExchangeClass::VHT_DL_MULTIUSER));
    }
    if (getConfiguration().enableVhtSuBeamforming)
        descriptors.push_back(makeActionDescriptor(HcfExchangeClass::VHT_SU_SOUNDING));
    return descriptors;
}

std::vector<HcfExchangeProviderDescriptor>
HeHcfFeatureSet::getExchangeProviderDescriptors()
{
    auto descriptors = CommonHcfFeatureSet::getExchangeProviderDescriptors();
    if (getConfiguration().enableHeUlMuOfdma)
        descriptors.push_back(makeActionDescriptor(HcfExchangeClass::HE_UL_TRIGGER));
    descriptors.push_back(makeActionDescriptor(HcfExchangeClass::FORCED_SINGLE_USER));
    if (getConfiguration().enableHeDlMuMimo)
        descriptors.push_back(makeActionDescriptor(HcfExchangeClass::HE_SOUNDING));
    descriptors.push_back(makeActionDescriptor(HcfExchangeClass::RECOVERY_SINGLE_USER));
    descriptors.push_back(makeActionDescriptor(HcfExchangeClass::HE_DL_MULTIUSER));
    return descriptors;
}

HeHcfFeatureSet::~HeHcfFeatureSet()
{
    triggeredUlExchangeService.shutdown();
}

} // namespace ieee80211
} // namespace inet
