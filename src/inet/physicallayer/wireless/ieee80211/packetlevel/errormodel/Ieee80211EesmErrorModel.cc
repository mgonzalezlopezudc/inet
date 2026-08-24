//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211EesmErrorModel.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <omnetpp/cvaluearray.h>
#include <omnetpp/cvaluemap.h>

#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211OfdmSubcarrierPlan.h"

namespace inet {
namespace physicallayer {

Define_Module(Ieee80211EesmErrorModel);

namespace {

static const cValueMap *manifestMap(const cValueMap *manifest, const char *key)
{
    if (manifest == nullptr || !manifest->containsKey(key) || !manifest->get(key).containsObject())
        throw cRuntimeError("Ieee80211EesmErrorModel: manifest requires object key '%s'", key);
    auto result = dynamic_cast<const cValueMap *>(manifest->get(key).objectValue());
    if (result == nullptr)
        throw cRuntimeError("Ieee80211EesmErrorModel: manifest key '%s' must be an object map", key);
    return result;
}

static std::string manifestString(const cValueMap *manifest, const char *key)
{
    if (manifest == nullptr || !manifest->containsKey(key) || manifest->get(key).getType() != cValue::STRING || manifest->get(key).stringValue()[0] == '\0')
        throw cRuntimeError("Ieee80211EesmErrorModel: manifest requires nonempty string key '%s'", key);
    return manifest->get(key).stringValue();
}

static int manifestInt(const cValueMap *manifest, const char *key)
{
    if (manifest == nullptr || !manifest->containsKey(key) || manifest->get(key).getType() != cValue::INT)
        throw cRuntimeError("Ieee80211EesmErrorModel: manifest requires integer key '%s'", key);
    return int(manifest->get(key).intValue());
}

static const cValueArray *manifestArray(const cValueMap *manifest, const char *key, bool requireNonempty)
{
    if (manifest == nullptr || !manifest->containsKey(key) || !manifest->get(key).containsObject())
        throw cRuntimeError("Ieee80211EesmErrorModel: manifest requires array key '%s'", key);
    auto result = dynamic_cast<const cValueArray *>(manifest->get(key).objectValue());
    if (result == nullptr || (requireNonempty && result->size() == 0))
        throw cRuntimeError("Ieee80211EesmErrorModel: manifest key '%s' has an invalid array value", key);
    return result;
}

static void requireTrue(const cValueMap *manifest, const char *key)
{
    if (manifest == nullptr || !manifest->containsKey(key) || manifest->get(key).getType() != cValue::BOOL || !manifest->get(key).boolValue())
        throw cRuntimeError("Ieee80211EesmErrorModel: manifest requires true Boolean key '%s'", key);
}

static bool isSha256(const std::string& value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

static std::string requiredSha256(const cValueMap *manifest, const char *key)
{
    auto value = manifestString(manifest, key);
    if (!isSha256(value))
        throw cRuntimeError("Ieee80211EesmErrorModel: manifest key '%s' must be a 64-character SHA-256", key);
    return value;
}

static void validateSourceDocuments(const cValueArray *documents)
{
    bool patidarFound = false;
    bool ieeeFound = false;
    for (int i = 0; i < documents->size(); i++) {
        const cValue& value = documents->get(i);
        if (!value.containsObject())
            throw cRuntimeError("Ieee80211EesmErrorModel: every sourceDocuments entry must be an object");
        const auto *document = dynamic_cast<const cValueMap *>(value.objectValue());
        if (document == nullptr)
            throw cRuntimeError("Ieee80211EesmErrorModel: every sourceDocuments entry must be an object map");
        const auto id = manifestString(document, "id");
        const auto checksum = requiredSha256(document, "sha256");
        if (id == "patidar2017") {
            if (checksum != "a428fdcaa4423f331e3bd02044345ec4305ee54056c319404a897e713cdab16e")
                throw cRuntimeError("Ieee80211EesmErrorModel: patidar2017 source checksum is not the reviewed publication");
            patidarFound = true;
        }
        else if (id == "ieee80211-2024") {
            if (checksum != "c08beb0e16bf8c36465a331d349dd8b8ea79b28d4a320288312ddeb5ce8d9bb1")
                throw cRuntimeError("Ieee80211EesmErrorModel: ieee80211-2024 source checksum is not the reviewed standard");
            ieeeFound = true;
        }
    }
    if (!patidarFound || !ieeeFound)
        throw cRuntimeError("Ieee80211EesmErrorModel: AWGN sourceDocuments must identify the reviewed Patidar publication and IEEE 802.11-2024 standard");
}

static void validateBetaSourceRows(const cValueArray *rows, const std::string& calibrationSet, const std::string& sourceTable)
{
    if (rows->size() != 8)
        throw cRuntimeError("Ieee80211EesmErrorModel: beta sourceRows must contain exactly the selected MCS 0-7 rows");
    bool seen[8] = {false, false, false, false, false, false, false, false};
    for (int i = 0; i < rows->size(); i++) {
        const cValue& value = rows->get(i);
        if (!value.containsObject())
            throw cRuntimeError("Ieee80211EesmErrorModel: every beta sourceRows entry must be an object");
        const auto *row = dynamic_cast<const cValueMap *>(value.objectValue());
        if (row == nullptr)
            throw cRuntimeError("Ieee80211EesmErrorModel: every beta sourceRows entry must be an object map");
        if (manifestString(row, "calibrationSet") != calibrationSet || manifestString(row, "sourceTable") != sourceTable)
            throw cRuntimeError("Ieee80211EesmErrorModel: beta sourceRows identity does not match the selected calibration set");
        const int mcs = manifestInt(row, "mcs");
        if (mcs < 0 || mcs > 7 || seen[mcs])
            throw cRuntimeError("Ieee80211EesmErrorModel: beta sourceRows must contain each MCS 0-7 exactly once");
        seen[mcs] = true;
    }
}

static void requireVersion(const cValueMap *manifest, const char *name);
static void requireMetadata(const cValueMap *manifest, const char *name, std::initializer_list<const char *> keys);

static void validateLocalAuthorizationManifest(const cValueMap *manifest, const std::string& calibrationSet)
{
    requireVersion(manifest, "root");
    if (manifestString(manifest, "artifactAcceptanceMode") != "userAuthorizedLocal")
        throw cRuntimeError("Ieee80211EesmErrorModel: local manifest must declare artifactAcceptanceMode=userAuthorizedLocal");
    if (manifestString(manifest, "deploymentScope") != "localEvaluationOnly")
        throw cRuntimeError("Ieee80211EesmErrorModel: local manifest deploymentScope must be localEvaluationOnly");

    const auto *authorization = manifestMap(manifest, "localAuthorization");
    manifestString(authorization, "authorizedBy");
    manifestString(authorization, "authorizationDate");
    manifestString(authorization, "authorizationRecord");
    manifestString(authorization, "sourceRecord");
    manifestString(authorization, "ht40TableAssumption");
    const auto status = [&](const char *key, const char *expected, const char *alternative) {
        const auto actual = manifestString(authorization, key);
        if (actual != expected && actual != alternative)
            throw cRuntimeError("Ieee80211EesmErrorModel: local manifest authorization status '%s' must be '%s' or '%s'", key, expected, alternative);
    };
    status("rawCountsStatus", "unavailable", "notApplicable");
    status("snrNormalizationStatus", "unavailable", "notApplicable");
    status("providerStatus", "unavailable", "notApplicable");
    status("cleanRoomStatus", "notVerified", "notApplicable");
    status("licenseStatus", "notGranted", "granted");
    const auto *limitations = manifestArray(manifest, "limitations", true);
    if (limitations->size() == 0)
        throw cRuntimeError("Ieee80211EesmErrorModel: local manifest limitations must be nonempty");

    const auto *awgn = manifestMap(manifest, "awgn");
    requireVersion(awgn, "AWGN");
    const auto observationsStatus = manifestString(awgn, "observationsStatus");
    if (observationsStatus != "unavailable" && observationsStatus != "notApplicable")
        throw cRuntimeError("Ieee80211EesmErrorModel: local AWGN observationsStatus must be unavailable or notApplicable");
    requireMetadata(awgn, "AWGN", {"scope", "snrDefinition", "psduLengthDefinition"});
    const auto *awgnProvenance = manifestMap(awgn, "provenance");
    requireMetadata(awgnProvenance, "AWGN provenance", {"method", "campaignId", "generatedDataLicense", "reviewedBy"});
    manifestString(awgnProvenance, "method");
    const auto generatedDataLicense = manifestString(awgnProvenance, "generatedDataLicense");
    if (generatedDataLicense.empty())
        throw cRuntimeError("Ieee80211EesmErrorModel: local AWGN license description must be nonempty");
    const auto reviewedBy = manifestString(awgnProvenance, "reviewedBy");
    if (reviewedBy != "notVerified" && reviewedBy != "notApplicable")
        throw cRuntimeError("Ieee80211EesmErrorModel: local AWGN review status must be notVerified or notApplicable");
    if (manifestArray(awgnProvenance, "sourceDocuments", false)->size() != 0 || manifestArray(awgnProvenance, "forbiddenSourcesUsed", false)->size() != 0)
        throw cRuntimeError("Ieee80211EesmErrorModel: local AWGN provenance must not assert external source custody");
    const auto provenanceStatus = [&](const char *key) {
        const auto actual = manifestString(awgnProvenance, key);
        if (actual != "unavailable" && actual != "notApplicable")
            throw cRuntimeError("Ieee80211EesmErrorModel: local AWGN provenance status '%s' must be unavailable or notApplicable", key);
    };
    provenanceStatus("rawPacketCounts");
    provenanceStatus("snrNormalization");
    provenanceStatus("provider");

    const auto *beta = manifestMap(manifest, "beta");
    requireVersion(beta, "beta");
    requiredSha256(manifest, "perTableSha256");
    const auto betaChecksum = requiredSha256(manifest, "betaTableSha256");
    if (requiredSha256(beta, "tableSha256") != betaChecksum)
        throw cRuntimeError("Ieee80211EesmErrorModel: local beta metadata tableSha256 does not match betaTableSha256");
    if (beta->containsKey("publicationSha256"))
        requiredSha256(beta, "publicationSha256");
    else if (manifestString(beta, "publicationStatus") != "notApplicable")
        throw cRuntimeError("Ieee80211EesmErrorModel: local beta metadata without a publication checksum must declare publicationStatus=notApplicable");
    if (manifestString(beta, "calibrationSet") != calibrationSet || manifestString(beta, "coding") != "BCC" ||
        manifestString(beta, "carrierSet") != "dataOnly" || manifestInt(beta, "ht20DataCarriers") != 52 ||
        manifestInt(beta, "ht40DataCarriers") != 108)
        throw cRuntimeError("Ieee80211EesmErrorModel: local beta metadata does not match the reviewed BCC carrier calibration");
    requireMetadata(beta, "local beta provenance", {"sourceTable", "citation", "generatedDataLicense", "reviewedBy"});
    validateBetaSourceRows(manifestArray(beta, "sourceRows", true), calibrationSet, manifestString(beta, "sourceTable"));
    manifestString(beta, "generatedDataLicense");
    manifestString(beta, "reviewedBy");
    manifestString(beta, "reviewStatus");
    const auto betaLicenseStatus = manifestString(beta, "licenseStatus");
    if (betaLicenseStatus != "notGranted" && betaLicenseStatus != "granted")
        throw cRuntimeError("Ieee80211EesmErrorModel: local beta license status must be notGranted or granted");
    const auto betaCleanRoomStatus = manifestString(beta, "cleanRoomStatus");
    if (betaCleanRoomStatus != "notVerified" && betaCleanRoomStatus != "notApplicable")
        throw cRuntimeError("Ieee80211EesmErrorModel: local beta clean-room status must be notVerified or notApplicable");
}

static void requireVersion(const cValueMap *manifest, const char *name)
{
    auto version = manifestString(manifest, "schemaVersion");
    if (version != "1")
        throw cRuntimeError("Ieee80211EesmErrorModel: unsupported %s manifest schemaVersion '%s'", name, version.c_str());
}

static void requireMetadata(const cValueMap *manifest, const char *name, std::initializer_list<const char *> keys)
{
    for (const char *key : keys)
        manifestString(manifest, key);
    (void)name;
}

static void validateReviewedManifest(const cValueMap *manifest, const std::string& calibrationSet)
{
    requireVersion(manifest, "root");
    auto perChecksum = requiredSha256(manifest, "perTableSha256");
    auto betaChecksum = requiredSha256(manifest, "betaTableSha256");
    (void)perChecksum;
    (void)betaChecksum;

    const auto *awgn = manifestMap(manifest, "awgn");
    requireVersion(awgn, "AWGN");
    auto observationsChecksum = requiredSha256(awgn, "observationsSha256");
    (void)observationsChecksum;
    requireMetadata(awgn, "AWGN", {"scope", "snrDefinition", "psduLengthDefinition"});
    const auto *awgnProvenance = manifestMap(awgn, "provenance");
    requireMetadata(awgnProvenance, "AWGN provenance", {"method", "campaignId", "generatedDataLicense", "reviewedBy"});
    validateSourceDocuments(manifestArray(awgnProvenance, "sourceDocuments", true));
    const auto *forbiddenSources = manifestArray(awgnProvenance, "forbiddenSourcesUsed", false);
    if (forbiddenSources->size() != 0)
        throw cRuntimeError("Ieee80211EesmErrorModel: AWGN provenance forbiddenSourcesUsed must be empty");
    requireTrue(awgnProvenance, "licenseApproved");
    requireTrue(awgnProvenance, "cleanRoomVerified");
    const auto *providerTool = manifestMap(awgn, "providerTool");
    requireMetadata(providerTool, "AWGN providerTool", {"name", "version", "gitCommit", "executableSha256", "toolchain", "configurationSha256"});
    if (!isSha256(manifestString(providerTool, "executableSha256")) || !isSha256(manifestString(providerTool, "configurationSha256")))
        throw cRuntimeError("Ieee80211EesmErrorModel: AWGN providerTool checksums must be SHA-256 values");
    const auto *awgnReview = manifestMap(awgn, "review");
    requireMetadata(awgnReview, "AWGN review", {"status", "reviewer", "record"});
    if (manifestString(awgnReview, "status") != "approved")
        throw cRuntimeError("Ieee80211EesmErrorModel: AWGN review status must be approved");

    const auto *beta = manifestMap(manifest, "beta");
    requireVersion(beta, "beta");
    if (requiredSha256(beta, "tableSha256") != betaChecksum)
        throw cRuntimeError("Ieee80211EesmErrorModel: beta metadata tableSha256 does not match betaTableSha256");
    if (manifestString(beta, "publicationSha256") != "a428fdcaa4423f331e3bd02044345ec4305ee54056c319404a897e713cdab16e")
        throw cRuntimeError("Ieee80211EesmErrorModel: beta metadata publication checksum is not the reviewed Patidar source");
    if (manifestString(beta, "calibrationSet") != calibrationSet)
        throw cRuntimeError("Ieee80211EesmErrorModel: beta metadata calibrationSet does not match the selected calibrationSet");
    if (manifestString(beta, "coding") != "BCC" || manifestString(beta, "carrierSet") != "dataOnly")
        throw cRuntimeError("Ieee80211EesmErrorModel: beta metadata must declare BCC dataOnly calibration");
    if (manifestInt(beta, "ht20DataCarriers") != 52 || manifestInt(beta, "ht40DataCarriers") != 108)
        throw cRuntimeError("Ieee80211EesmErrorModel: beta metadata must pair HT20/HT40 with 52/108 data carriers");
    requireMetadata(beta, "beta provenance", {"sourceTable", "citation", "generatedDataLicense", "reviewedBy"});
    validateBetaSourceRows(manifestArray(beta, "sourceRows", true), calibrationSet, manifestString(beta, "sourceTable"));
    requireTrue(beta, "licenseApproved");
    requireTrue(beta, "cleanRoomVerified");
    const auto *betaReview = manifestMap(beta, "review");
    requireMetadata(betaReview, "beta review", {"status", "reviewer", "record"});
    if (manifestString(betaReview, "status") != "approved")
        throw cRuntimeError("Ieee80211EesmErrorModel: beta review status must be approved");
}

static void validateManifest(const cValueMap *manifest, const std::string& calibrationSet, const std::string& acceptanceMode)
{
    if (manifest != nullptr && manifest->containsKey("artifactAcceptanceMode")) {
        const auto manifestAcceptanceMode = manifestString(manifest, "artifactAcceptanceMode");
        if (manifestAcceptanceMode != acceptanceMode)
            throw cRuntimeError("Ieee80211EesmErrorModel: manifest artifactAcceptanceMode '%s' does not match selected mode '%s'", manifestAcceptanceMode.c_str(), acceptanceMode.c_str());
    }
    if (acceptanceMode == "userAuthorizedLocal")
        validateLocalAuthorizationManifest(manifest, calibrationSet);
    else if (acceptanceMode == "reviewed")
        validateReviewedManifest(manifest, calibrationSet);
    else
        throw cRuntimeError("Ieee80211EesmErrorModel: unsupported artifactAcceptanceMode '%s'", acceptanceMode.c_str());
}

} // namespace

void Ieee80211EesmErrorModel::initialize(int stage)
{
    Ieee80211EffectiveSnirErrorModelBase::initialize(stage);
    if (stage != INITSTAGE_LOCAL)
        return;
    if (corruptionMode != CorruptionMode::CM_PACKET)
        throw cRuntimeError("Ieee80211EesmErrorModel supports only packet corruption");
    const char *perTableFile = par("perTableFile").stringValue();
    const char *betaTableFile = par("betaTableFile").stringValue();
    calibrationSet = par("calibrationSet").stringValue();
    const std::string artifactAcceptanceMode = par("artifactAcceptanceMode").stdstringValue();
    if (*perTableFile == '\0')
        throw cRuntimeError("Ieee80211EesmErrorModel: perTableFile is empty; AWGN PER tables are not bundled and must be supplied");
    if (*betaTableFile == '\0')
        throw cRuntimeError("Ieee80211EesmErrorModel: betaTableFile is empty; supply the versioned BCC beta artifact");
    if (calibrationSet.empty())
        throw cRuntimeError("Ieee80211EesmErrorModel: calibrationSet is empty");
    cValueMap *manifest = dynamic_cast<cValueMap *>(par("perTableManifest").objectValue());
    validateManifest(manifest, calibrationSet, artifactAcceptanceMode);
    const std::string perChecksum = manifestString(manifest, "perTableSha256");
    const std::string betaChecksum = manifestString(manifest, "betaTableSha256");
    perTable.requireSha256(perTableFile, perChecksum);
    eesm.requireSha256(betaTableFile, betaChecksum);
    perTable.loadCsv(perTableFile);
    eesm.loadBetaCsv(betaTableFile, calibrationSet);
}

double Ieee80211EesmErrorModel::computeEffectiveSnrDb(const std::vector<double>& carrierSnr, const IIeee80211HtDataMode *dataMode) const
{
    const int bandwidthMHz = dataMode->getBandwidth() == MHz(20) ? 20 : 40;
    const double beta = eesm.getBeta(calibrationSet, bandwidthMHz, dataMode->getMcsIndex(), carrierSnr.size());
    return Ieee80211Eesm::computeEffectiveSnrDb(carrierSnr, beta);
}

} // namespace physicallayer
} // namespace inet
