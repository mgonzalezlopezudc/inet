//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211MiEffectiveSnirErrorModelBase.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include <omnetpp/cvaluearray.h>

#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211OfdmSubcarrierPlan.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmModulation.h"

namespace inet {
namespace physicallayer {

namespace {

static const cValueMap *manifestMap(const cValueMap *manifest, const char *key, const char *modelName)
{
    if (manifest == nullptr || !manifest->containsKey(key) || !manifest->get(key).containsObject())
        throw cRuntimeError("%s: manifest requires object key '%s'", modelName, key);
    const auto *result = dynamic_cast<const cValueMap *>(manifest->get(key).objectValue());
    if (result == nullptr)
        throw cRuntimeError("%s: manifest key '%s' must be an object map", modelName, key);
    return result;
}

static std::string manifestString(const cValueMap *manifest, const char *key, const char *modelName)
{
    if (manifest == nullptr || !manifest->containsKey(key) || manifest->get(key).getType() != cValue::STRING || std::string(manifest->get(key).stringValue()).empty())
        throw cRuntimeError("%s: manifest requires nonempty string key '%s'", modelName, key);
    return manifest->get(key).stringValue();
}

static const cValueArray *manifestArray(const cValueMap *manifest, const char *key, bool requireNonempty, const char *modelName)
{
    if (manifest == nullptr || !manifest->containsKey(key) || !manifest->get(key).containsObject())
        throw cRuntimeError("%s: manifest requires array key '%s'", modelName, key);
    const auto *result = dynamic_cast<const cValueArray *>(manifest->get(key).objectValue());
    if (result == nullptr || (requireNonempty && result->size() == 0))
        throw cRuntimeError("%s: manifest key '%s' has an invalid array value", modelName, key);
    return result;
}

static bool isSha256(const std::string& value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

static std::string requiredSha256(const cValueMap *manifest, const char *key, const char *modelName)
{
    const auto value = manifestString(manifest, key, modelName);
    if (!isSha256(value))
        throw cRuntimeError("%s: manifest key '%s' must be a 64-character SHA-256", modelName, key);
    return value;
}

static void requireVersion(const cValueMap *manifest, const char *name, const char *modelName)
{
    const auto version = manifestString(manifest, "schemaVersion", modelName);
    if (version != "1")
        throw cRuntimeError("%s: unsupported %s manifest schemaVersion '%s'", modelName, name, version.c_str());
}

static void requireMetadata(const cValueMap *manifest, std::initializer_list<const char *> keys, const char *modelName)
{
    for (const char *key : keys)
        manifestString(manifest, key, modelName);
}

static void requireTrue(const cValueMap *manifest, const char *key, const char *modelName)
{
    if (manifest == nullptr || !manifest->containsKey(key) || manifest->get(key).getType() != cValue::BOOL || !manifest->get(key).boolValue())
        throw cRuntimeError("%s: manifest requires true Boolean key '%s'", modelName, key);
}

static void validateSourceDocuments(const cValueArray *documents, const char *modelName)
{
    bool patidarFound = false;
    bool ieeeFound = false;
    for (int i = 0; i < documents->size(); i++) {
        const cValue& value = documents->get(i);
        if (!value.containsObject())
            throw cRuntimeError("%s: every sourceDocuments entry must be an object", modelName);
        const auto *document = dynamic_cast<const cValueMap *>(value.objectValue());
        if (document == nullptr)
            throw cRuntimeError("%s: every sourceDocuments entry must be an object map", modelName);
        const auto id = manifestString(document, "id", modelName);
        const auto checksum = requiredSha256(document, "sha256", modelName);
        if (id == "patidar2017") {
            if (checksum != "a428fdcaa4423f331e3bd02044345ec4305ee54056c319404a897e713cdab16e")
                throw cRuntimeError("%s: patidar2017 source checksum is not the reviewed publication", modelName);
            patidarFound = true;
        }
        else if (id == "ieee80211-2024") {
            if (checksum != "c08beb0e16bf8c36465a331d349dd8b8ea79b28d4a320288312ddeb5ce8d9bb1")
                throw cRuntimeError("%s: ieee80211-2024 source checksum is not the reviewed standard", modelName);
            ieeeFound = true;
        }
    }
    if (!patidarFound || !ieeeFound)
        throw cRuntimeError("%s: AWGN sourceDocuments must identify the reviewed Patidar publication and IEEE 802.11-2024 standard", modelName);
}

static void validateReviewedManifest(const cValueMap *manifest, const char *modelName)
{
    requireVersion(manifest, "root", modelName);
    requiredSha256(manifest, "perTableSha256", modelName);

    const auto *awgn = manifestMap(manifest, "awgn", modelName);
    requireVersion(awgn, "AWGN", modelName);
    requiredSha256(awgn, "observationsSha256", modelName);
    requireMetadata(awgn, {"scope", "snrDefinition", "psduLengthDefinition"}, modelName);
    const auto *awgnProvenance = manifestMap(awgn, "provenance", modelName);
    requireMetadata(awgnProvenance, {"method", "campaignId", "generatedDataLicense", "reviewedBy"}, modelName);
    validateSourceDocuments(manifestArray(awgnProvenance, "sourceDocuments", true, modelName), modelName);
    if (manifestArray(awgnProvenance, "forbiddenSourcesUsed", false, modelName)->size() != 0)
        throw cRuntimeError("%s: AWGN provenance forbiddenSourcesUsed must be empty", modelName);
    requireTrue(awgnProvenance, "licenseApproved", modelName);
    requireTrue(awgnProvenance, "cleanRoomVerified", modelName);

    const auto *providerTool = manifestMap(awgn, "providerTool", modelName);
    requireMetadata(providerTool, {"name", "version", "gitCommit", "executableSha256", "toolchain", "configurationSha256"}, modelName);
    if (!isSha256(manifestString(providerTool, "executableSha256", modelName)) || !isSha256(manifestString(providerTool, "configurationSha256", modelName)))
        throw cRuntimeError("%s: AWGN providerTool checksums must be SHA-256 values", modelName);
    const auto *awgnReview = manifestMap(awgn, "review", modelName);
    requireMetadata(awgnReview, {"status", "reviewer", "record"}, modelName);
    if (manifestString(awgnReview, "status", modelName) != "approved")
        throw cRuntimeError("%s: AWGN review status must be approved", modelName);
}

static void validateLocalAuthorizationManifest(const cValueMap *manifest, const char *modelName)
{
    requireVersion(manifest, "root", modelName);
    if (manifestString(manifest, "artifactAcceptanceMode", modelName) != "userAuthorizedLocal")
        throw cRuntimeError("%s: local manifest must declare artifactAcceptanceMode=userAuthorizedLocal", modelName);
    if (manifestString(manifest, "deploymentScope", modelName) != "localEvaluationOnly")
        throw cRuntimeError("%s: local manifest deploymentScope must be localEvaluationOnly", modelName);

    const auto *authorization = manifestMap(manifest, "localAuthorization", modelName);
    requireMetadata(authorization, {"authorizedBy", "authorizationDate", "authorizationRecord", "sourceRecord", "ht40TableAssumption"}, modelName);
    const auto status = [&](const char *key, const char *expected, const char *alternative) {
        const auto actual = manifestString(authorization, key, modelName);
        if (actual != expected && actual != alternative)
            throw cRuntimeError("%s: local manifest authorization status '%s' must be '%s' or '%s'", modelName, key, expected, alternative);
    };
    status("rawCountsStatus", "unavailable", "notApplicable");
    status("snrNormalizationStatus", "unavailable", "notApplicable");
    status("providerStatus", "unavailable", "notApplicable");
    status("cleanRoomStatus", "notVerified", "notApplicable");
    status("licenseStatus", "notGranted", "granted");
    if (manifestArray(manifest, "limitations", true, modelName)->size() == 0)
        throw cRuntimeError("%s: local manifest limitations must be nonempty", modelName);

    const auto *awgn = manifestMap(manifest, "awgn", modelName);
    requireVersion(awgn, "AWGN", modelName);
    const auto observationsStatus = manifestString(awgn, "observationsStatus", modelName);
    if (observationsStatus != "unavailable" && observationsStatus != "notApplicable")
        throw cRuntimeError("%s: local AWGN observationsStatus must be unavailable or notApplicable", modelName);
    requireMetadata(awgn, {"scope", "snrDefinition", "psduLengthDefinition"}, modelName);
    const auto *awgnProvenance = manifestMap(awgn, "provenance", modelName);
    requireMetadata(awgnProvenance, {"method", "campaignId", "generatedDataLicense", "reviewedBy"}, modelName);
    const auto reviewedBy = manifestString(awgnProvenance, "reviewedBy", modelName);
    if (reviewedBy != "notVerified" && reviewedBy != "notApplicable")
        throw cRuntimeError("%s: local AWGN review status must be notVerified or notApplicable", modelName);
    if (manifestArray(awgnProvenance, "sourceDocuments", false, modelName)->size() != 0 || manifestArray(awgnProvenance, "forbiddenSourcesUsed", false, modelName)->size() != 0)
        throw cRuntimeError("%s: local AWGN provenance must not assert external source custody", modelName);
    const auto provenanceStatus = [&](const char *key) {
        const auto actual = manifestString(awgnProvenance, key, modelName);
        if (actual != "unavailable" && actual != "notApplicable")
            throw cRuntimeError("%s: local AWGN provenance status '%s' must be unavailable or notApplicable", modelName, key);
    };
    provenanceStatus("rawPacketCounts");
    provenanceStatus("snrNormalization");
    provenanceStatus("provider");
}

} // namespace

void Ieee80211MiEffectiveSnirErrorModelBase::validateAwgnManifest(const cValueMap *manifest, const std::string& acceptanceMode, const char *modelName)
{
    if (manifest != nullptr && manifest->containsKey("artifactAcceptanceMode")) {
        const auto manifestAcceptanceMode = manifestString(manifest, "artifactAcceptanceMode", modelName);
        if (manifestAcceptanceMode != acceptanceMode)
            throw cRuntimeError("%s: manifest artifactAcceptanceMode '%s' does not match selected mode '%s'", modelName, manifestAcceptanceMode.c_str(), acceptanceMode.c_str());
    }
    if (acceptanceMode == "userAuthorizedLocal")
        validateLocalAuthorizationManifest(manifest, modelName);
    else if (acceptanceMode == "reviewed")
        validateReviewedManifest(manifest, modelName);
    else
        throw cRuntimeError("%s: unsupported artifactAcceptanceMode '%s'", modelName, acceptanceMode.c_str());
}

void Ieee80211MiEffectiveSnirErrorModelBase::initialize(int stage)
{
    Ieee80211EffectiveSnirErrorModelBase::initialize(stage);
    if (stage != INITSTAGE_LOCAL)
        return;
    const char *modelName = getErrorModelName();
    if (corruptionMode != CorruptionMode::CM_PACKET)
        throw cRuntimeError("%s supports only packet corruption", modelName);
    const char *perTableFile = par("perTableFile").stringValue();
    if (*perTableFile == '\0')
        throw cRuntimeError("%s: perTableFile is empty; AWGN PER tables are not bundled and must be supplied", modelName);
    beta = par("beta").doubleValue();
    if (!std::isfinite(beta) || beta <= 0)
        throw cRuntimeError("%s: beta must be finite and strictly positive in linear Es/N0 units", modelName);
    const std::string acceptanceMode = par("artifactAcceptanceMode").stdstringValue();
    const auto *manifest = dynamic_cast<const cValueMap *>(par("perTableManifest").objectValue());
    validateAwgnManifest(manifest, acceptanceMode, modelName);
    perTable.requireSha256(perTableFile, manifestString(manifest, "perTableSha256", modelName));
    perTable.loadCsv(perTableFile);
}

const ApskModulationBase *Ieee80211MiEffectiveSnirErrorModelBase::getSubcarrierModulation(const IIeee80211HtDataMode *dataMode) const
{
    const auto *ofdmModulation = dynamic_cast<const Ieee80211OfdmModulation *>(dataMode->getModulation());
    if (ofdmModulation == nullptr || ofdmModulation->getSubcarrierModulation() == nullptr)
        throw cRuntimeError("%s requires an authoritative OFDM subcarrier modulation", getErrorModelName());
    return ofdmModulation->getSubcarrierModulation();
}

double Ieee80211MiEffectiveSnirErrorModelBase::computeEffectiveSnrDb(const std::vector<double>& carrierSnr, const IIeee80211HtDataMode *dataMode) const
{
    return computeMappedEffectiveSnrDb(carrierSnr, getSubcarrierModulation(dataMode), beta);
}

} // namespace physicallayer
} // namespace inet
