#!/usr/bin/env python3
"""Run the configurations and repetitions declared in experiments.json."""

from __future__ import annotations

import argparse
import math
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, replace
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from analysis_core import DEFAULT_MANIFEST, REPOSITORY_ROOT, load_manifest

GENERAL_ANALYSIS_ROOT = Path(__file__).resolve().parents[2] / "ieee80211" / "analysis"
if str(GENERAL_ANALYSIS_ROOT) not in sys.path:
    sys.path.insert(0, str(GENERAL_ANALYSIS_ROOT))

from inet_wifi_analysis import (
    CampaignJob,
    build_cmdenv_command,
    collect_campaign_jobs,
)

CORE_PERFORMANCE_RECORDING_OVERRIDES = (
    "--**.app[*].packetSent*.vector-recording=true",
    "--**.app[*].packetReceived*.vector-recording=true",
    "--**.app[*].endToEndDelay*.vector-recording=true",
)

GROUP_PERFORMANCE_VECTOR_STATISTICS = {
    "fragmentation": (
        "packetSentToPeer",
        "acknowledgmentFrameType",
        "acknowledgmentAirtime",
    ),
    "twt": ("radioMode", "powerConsumption"),
    "rate": (
        "heRateSelectedMcs",
        "heRateSelectedNss",
        "heRateSuccessProbability",
        "heRateTxSuccess",
        "heRateRetryCount",
    ),
    "er": ("packetDropIncorrectlyReceived",),
    "puncturing": (
        "heRuToneOffset",
        "heRuToneSize",
        "heStaId",
        "hePuncturedSubchannelMask",
    ),
    "mimo": ("heStaId", "heSpatialStreams", "heStreamStartIndex"),
    "bss": (
        "transmissionState",
        "heSpatialReuseBssType",
        "heSpatialReuseReceivedBssColor",
        "heSpatialReuseLocalBssColor",
        "heSpatialReuseEligible",
        "heSpatialReuseIgnoredPpdu",
        "heSpatialReuseObssPdThreshold",
        "heSpatialReuseTransmitPowerLimit",
        "heSpatialReuseReason",
    ),
    "dl_sched": ("heStaId", "heScheduledPsduBytes", "heUserPpduDuration"),
    "dl_asym": ("heStaId", "heScheduledPsduBytes", "heUserPpduDuration"),
    "bsr": (
        "heUlBufferStatusReportedBytes",
        "heUlBufferStatusScheduledBytes",
        "heUlTriggerDecisionTriggerId",
        "heUlTriggerDecisionTriggerType",
        "heUlTriggerDecisionUserOrdinal",
        "heUlTriggerDecisionAssociationId",
        "heUlTriggerDecisionReportedBytes",
        "heUlTriggerDecisionPlannedBytes",
    ),
}

BSR_TRIGGER_DECISION_VECTOR_OVERRIDES = tuple(
    f"--**.{statistic}.result-recording-modes=+vector"
    for statistic in (
        "heUlTriggerDecisionTriggerId",
        "heUlTriggerDecisionTriggerType",
        "heUlTriggerDecisionUserOrdinal",
        "heUlTriggerDecisionAssociationId",
        "heUlTriggerDecisionReportedBytes",
        "heUlTriggerDecisionPlannedBytes",
    )
)

GROUP_DIAGNOSTIC_VECTOR_STATISTICS = {
    "fragmentation": (
        "packetSentToPeerWithRetry",
        "packetDropRetryLimitReached",
        "frameSequenceDuration",
        "frameSequenceNumPackets",
    ),
    "uora": (
        "heUlBufferStatusReportedBytes",
        "heUlBufferStatusScheduledBytes",
        "heRuIndex",
        "heRuToneOffset",
        "heRuToneSize",
        "heStaId",
        "heUlTriggerDecisionId",
        "heTbResponseTriggerId",
        "heTbResponseReason",
        "heTbResponsePendingBytes",
        "heTbResponseSelectedBytes",
        "backoffPeriodGenerated",
        "contentionWindowChanged",
    ),
    "twt": (
        "packetSentToPeer",
        "packetSentToPeerWithRetry",
        "packetDropQueueOverflow",
        "twtStationAwake",
        "twtActiveServicePeriodCount",
        "twtAgreementCount",
    ),
    "rate": (
        "datarateChanged",
        "packetSentToPeerWithRetry",
        "packetDropRetryLimitReached",
    ),
    "er": (
        "packetSentToPeerWithRetry",
        "packetDropRetryLimitReached",
        "receptionState",
    ),
    "puncturing": (
        "heRuIndex",
        "heScheduledPsduBytes",
        "heUserPpduDuration",
        "packetDropIncorrectlyReceived",
        "transmissionState",
    ),
    "mimo": (
        "heRuIndex",
        "heRuToneOffset",
        "heRuToneSize",
        "heScheduledPsduBytes",
        "heUserPpduDuration",
        "packetDropIncorrectlyReceived",
    ),
    "bss": (
        "nav",
        "receptionState",
        "packetSentToPeerWithRetry",
        "packetDropRetryLimitReached",
    ),
    "width": (
        "datarateSelected",
        "packetSentToPeer",
        "acknowledgmentFrameType",
        "acknowledgmentAirtime",
        "frameSequenceDuration",
    ),
    "dl_sched": (
        "heRuIndex",
        "heRuToneOffset",
        "heRuToneSize",
        "packetSentToPeer",
        "packetSentToPeerWithRetry",
    ),
    "dl_asym": (
        "heRuIndex",
        "heRuToneOffset",
        "heRuToneSize",
        "packetSentToPeer",
        "packetSentToPeerWithRetry",
    ),
    "bsr": (
        "heStaId",
        "heScheduledPsduBytes",
        "heUserPpduDuration",
        "heUlTriggerDecisionId",
        "heTbResponseTriggerId",
        "heTbResponseReason",
        "heTbResponsePendingBytes",
        "heTbResponseSelectedBytes",
    ),
    "multi_tid": (
        "blockAckAgreementActive",
        "acknowledgmentFrameType",
        "acknowledgmentAirtime",
        "packetSentToPeer",
        "packetSentToPeerWithRetry",
        "packetDropRetryLimitReached",
    ),
    "operating_mode": (
        "heStaId",
        "heSpatialStreams",
        "heStreamStartIndex",
        "heScheduledPsduBytes",
        "heUserPpduDuration",
        "heUlBufferStatusScheduledBytes",
        "peerOperatingModeAssociationId",
        "peerOperatingModeRxNss",
        "peerOperatingModeChannelWidth",
        "peerOperatingModeUlMuDisable",
        "heUlTriggerDecisionId",
        "heTbResponseTriggerId",
        "heTbResponseReason",
        "packetSentToPeerWithRetry",
    ),
    "frequency_selective": (
        "heRuIndex",
        "heRuToneOffset",
        "heRuToneSize",
        "heStaId",
        "packetDropIncorrectlyReceived",
        "receptionState",
    ),
    "ndp_feedback": (
        "heRuIndex",
        "heRuToneOffset",
        "heRuToneSize",
        "heStaId",
        "heUlTriggerDecisionId",
        "heTbResponseTriggerId",
        "heTbResponseReason",
        "heTbResponseReportedBytes",
        "transmissionState",
        "receptionState",
        "packetDropIncorrectlyReceived",
    ),
    "dense_iot": (
        "datarateChanged",
        "heRateSelectedMcs",
        "heStaId",
        "heRuToneSize",
        "packetSentToPeerWithRetry",
        "packetDropQueueOverflow",
        "packetDropRetryLimitReached",
        "receptionState",
        "transmissionState",
        "powerConsumption",
        "twtStationAwake",
        "twtActiveServicePeriodCount",
        "twtAgreementCount",
    ),
    "bcc_ldpc": (
        "heRuIndex",
        "heRuToneOffset",
        "heRuToneSize",
        "heStaId",
        "heScheduledPsduBytes",
        "heUserPpduDuration",
        "packetDropIncorrectlyReceived",
        "datarateSelected",
    ),
    "ul_mu_mimo": (
        "heRuIndex",
        "heRuToneOffset",
        "heRuToneSize",
        "heStaId",
        "heSpatialStreams",
        "heStreamStartIndex",
        "heScheduledPsduBytes",
        "heUserPpduDuration",
        "heUlBufferStatusReportedBytes",
        "heUlBufferStatusScheduledBytes",
        "heUlTriggerDecisionId",
        "heTbResponseTriggerId",
        "heTbResponseReason",
        "heTbResponsePendingBytes",
        "heTbResponseSelectedBytes",
        "transmissionState",
        "packetSentToPeerWithRetry",
    ),
    "eht_features": (
        "datarateSelected",
        "packetSentToPeer",
        "packetSentToPeerWithRetry",
        "packetDropIncorrectlyReceived",
        "packetDropRetryLimitReached",
        "transmissionState",
        "receptionState",
    ),
}

CORE_SCALAR_STATISTICS = (
    "packetDropQueueOverflow",
    "packetDropRetryLimitReached",
)

CORE_SCALAR_RECORDING_OVERRIDES = (
    "--**.app[*].packetSent*.scalar-recording=true",
    "--**.app[*].packetReceived*.scalar-recording=true",
)

GROUP_SCALAR_STATISTICS = {
    "uora": ("heUlRandomAccessAttempt", "heUlRandomAccessSuccess"),
    "ul_ofdma": (
        "heUlBasicTriggerSent",
        "heUlBsrpTriggerSent",
        "heUlStaleBufferStatus",
        "heUlScheduledUsers",
    ),
}

UL_OFDMA_TRIGGER_DECISION_VECTOR_STATISTICS = (
    "heUlTriggerDecisionTriggerId",
    "heUlTriggerDecisionTriggerType",
    "heUlTriggerDecisionUserOrdinal",
    "heUlTriggerDecisionAssociationId",
    "heUlTriggerDecisionBacklogBytes",
    "heUlTriggerDecisionReportedBytes",
    "heUlTriggerDecisionPlannedBytes",
    "heUlTriggerDecisionTid",
    "heUlTriggerDecisionAccessCategory",
    "heUlTriggerDecisionSelected",
    "heUlTriggerDecisionRuIndex",
    "heUlTriggerDecisionRuToneSize",
    "heUlTriggerDecisionRuToneOffset",
)

UL_OFDMA_DIAGNOSTIC_VECTOR_STATISTICS = (
    "heUlBufferStatusReportedBytes",
    "heUlBufferStatusScheduledBytes",
    *UL_OFDMA_TRIGGER_DECISION_VECTOR_STATISTICS,
    "heRuToneOffset",
    "heRuToneSize",
    "heStaId",
    "heScheduledPsduBytes",
    "heUserPpduDuration",
    "heUlTriggerDecisionId",
    "heTbResponseTriggerId",
    "heTbResponseReason",
    "heTbResponseHadPendingPayload",
    "heTbResponsePendingBytes",
    "heTbResponseSelectedBytes",
    "heTbResponseReportedBytes",
)

UL_OFDMA_TRIGGER_DECISION_VECTOR_OVERRIDES = tuple(
    f"--**.{statistic}.result-recording-modes=+vector"
    for statistic in UL_OFDMA_TRIGGER_DECISION_VECTOR_STATISTICS
)

UL_OFDMA_QUEUE_VECTOR_OVERRIDES = (
    "--**.host[*].wlan[*].mac.hcf.edca.edcaf[*].pendingQueue.queueLength*.vector-recording=true",
    "--**.host[*].wlan[*].mac.hcf.edca.edcaf[*].pendingQueue.queueingTime*.vector-recording=true",
    "--**.host[*].wlan[*].mac.hcf.edca.edcaf[*].inProgressFrames.queueLength*.vector-recording=true",
    "--**.host[*].wlan[*].mac.hcf.edca.edcaf[*].inProgressFrames.queueingTime*.vector-recording=true",
)

GROUP_DIAGNOSTIC_VECTOR_OVERRIDES = {
    group: UL_OFDMA_QUEUE_VECTOR_OVERRIDES
    for group in (
        "uora",
        "twt",
        "dl_sched",
        "dl_asym",
        "bsr",
        "dense_iot",
        "ul_mu_mimo",
        "ul_ofdma",
    )
}
SESSION_ID_PATTERN = re.compile(r"^\d{8}T\d{6}Z$")


@dataclass(frozen=True)
class JobResult:
    job: CampaignJob
    returncode: int
    output: str


@dataclass(frozen=True)
class RequiredResult:
    kind: str
    module: str
    name: str
    expectation: str = "nonzero"


@dataclass(frozen=True)
class RecordedResult:
    kind: str
    module: str
    name: str
    count: int | None = None
    value: float | None = None
    maximum: float | None = None


PERFORMANCE_REQUIREMENTS = (
    RequiredResult("scalar", "**.app[*]", "packetSent:count"),
    RequiredResult(
        "vector",
        "**.app[*]",
        "packetReceived:vector(packetBytes)",
    ),
    RequiredResult("vector", "**.app[*]", "endToEndDelay:vector"),
)

UL_OFDMA_DIAGNOSTIC_REQUIREMENTS = (
    RequiredResult(
        "vector",
        "**.host[*].wlan[*].mac.hcf.edca.edcaf[*].pendingQueue",
        "queueLength:vector",
        "positive",
    ),
    RequiredResult(
        "vector",
        "**.host[*].wlan[*].mac.hcf",
        "heTbResponseReason:vector",
    ),
    RequiredResult(
        "vector",
        "**.host[*].wlan[*].mac.hcf",
        "heTbResponseSelectedBytes:vector",
    ),
    RequiredResult(
        "vector",
        "**.ap.wlan[*].mac.hcf.ulCoordinator",
        "heUlBufferStatusScheduledBytes:vector",
    ),
    RequiredResult(
        "vector",
        "**.host[*].wlan[*].radio",
        "heScheduledPsduBytes:vector",
    ),
)

UL_OFDMA_EDCA_INACTIVE_REQUIREMENTS = (
    RequiredResult(
        "vector",
        "**.host[*].wlan[*].mac.hcf",
        "heTbResponseReason:vector",
        "zero",
    ),
    RequiredResult(
        "vector",
        "**.ap.wlan[*].mac.hcf.ulCoordinator",
        "heUlBufferStatusScheduledBytes:vector",
        "zero",
    ),
)


def positive_int(value: str) -> int:
    parsed = int(value)
    if parsed < 1:
        raise argparse.ArgumentTypeError("must be at least 1")
    return parsed


def session_id(value: str) -> str:
    if not SESSION_ID_PATTERN.fullmatch(value):
        raise argparse.ArgumentTypeError("must have UTC format YYYYMMDDTHHMMSSZ")
    return value


def new_session_id() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def available_cpu_count() -> int:
    """Return the CPUs available to this process, respecting affinity limits."""
    try:
        return len(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        return os.cpu_count() or 1


def unique_statistics(*groups: tuple[str, ...]) -> tuple[str, ...]:
    return tuple(dict.fromkeys(
        statistic
        for group in groups
        for statistic in group
    ))


def performance_vector_statistics(group: str) -> tuple[str, ...]:
    return GROUP_PERFORMANCE_VECTOR_STATISTICS.get(group, ())


def diagnostic_vector_statistics(group: str) -> tuple[str, ...]:
    if group == "ul_ofdma":
        return UL_OFDMA_DIAGNOSTIC_VECTOR_STATISTICS
    return GROUP_DIAGNOSTIC_VECTOR_STATISTICS.get(group, ())


def diagnostic_vector_overrides(group: str) -> tuple[str, ...]:
    return GROUP_DIAGNOSTIC_VECTOR_OVERRIDES.get(group, ())


def scalar_statistics(group: str) -> tuple[str, ...]:
    return unique_statistics(
        CORE_SCALAR_STATISTICS,
        GROUP_SCALAR_STATISTICS.get(group, ()),
    )


def diagnostic_run(group: dict[str, Any]) -> int | None:
    recording = group.get("recording", {})
    selected = recording.get("diagnostic_run")
    return None if selected is None else int(selected)


def build_command(
    ini: Path,
    result_dir: Path,
    config: str,
    run: int,
    repetitions: int,
    pcap_interface_patterns: tuple[str, ...] = (),
    group: str = "",
    diagnostic: bool = False,
    exhaustive_vectors: bool = False,
) -> tuple[str, ...]:
    vectors = performance_vector_statistics(group)
    overrides = (
        CORE_SCALAR_RECORDING_OVERRIDES
        + CORE_PERFORMANCE_RECORDING_OVERRIDES
    )
    if group == "ul_ofdma" and diagnostic and exhaustive_vectors:
        vectors = unique_statistics(
            vectors,
            UL_OFDMA_DIAGNOSTIC_VECTOR_STATISTICS,
        )
        overrides += (
            UL_OFDMA_QUEUE_VECTOR_OVERRIDES
            + UL_OFDMA_TRIGGER_DECISION_VECTOR_OVERRIDES
        )
    if exhaustive_vectors and run == 0:
        vectors = unique_statistics(
            vectors,
            diagnostic_vector_statistics(group),
        )
        overrides += diagnostic_vector_overrides(group)
    if group == "bsr":
        overrides += BSR_TRIGGER_DECISION_VECTOR_OVERRIDES
    return build_cmdenv_command(
        REPOSITORY_ROOT,
        ini,
        result_dir,
        config,
        run,
        repetitions,
        vectors,
        scalar_statistics(group),
        pcap_interface_patterns,
        overrides,
    )


def collect_jobs(
    manifest: dict[str, Any],
    selected_group: str,
    repetitions_override: int | None = None,
    selected_configs: set[str] | None = None,
    campaign_session_id: str | None = None,
    pcap_run: int | None = None,
    pcap_interface_patterns: tuple[str, ...] = (),
    exhaustive_vectors: bool = False,
) -> list[CampaignJob]:
    session = campaign_session_id or new_session_id()
    group_names = (
        sorted(manifest["groups"])
        if selected_group == "all"
        else [selected_group]
    )
    jobs: list[CampaignJob] = []
    for group_name in group_names:
        group = manifest["groups"][group_name]
        group_jobs = collect_campaign_jobs(
            {"groups": {group_name: group}},
            group_name,
            REPOSITORY_ROOT,
            session,
            performance_vector_statistics(group_name),
            scalar_statistics(group_name),
            repetitions_override,
            selected_configs,
            pcap_run,
            pcap_interface_patterns,
        )
        selected_diagnostic_run = diagnostic_run(group)
        for job in group_jobs:
            additional = (
                CORE_SCALAR_RECORDING_OVERRIDES
                + CORE_PERFORMANCE_RECORDING_OVERRIDES
            )
            if exhaustive_vectors and (
                group_name == "ul_ofdma"
                and job.run == selected_diagnostic_run
            ):
                additional += tuple(
                    f"--**.{statistic}*.vector-recording=true"
                    for statistic in UL_OFDMA_DIAGNOSTIC_VECTOR_STATISTICS
                    if statistic not in performance_vector_statistics(group_name)
                )
                additional += UL_OFDMA_QUEUE_VECTOR_OVERRIDES
                additional += UL_OFDMA_TRIGGER_DECISION_VECTOR_OVERRIDES
            if exhaustive_vectors and job.run == 0:
                additional += tuple(
                    f"--**.{statistic}*.vector-recording=true"
                    for statistic in diagnostic_vector_statistics(group_name)
                    if statistic not in performance_vector_statistics(group_name)
                    and not (
                        group_name == "ul_ofdma"
                        and statistic in UL_OFDMA_DIAGNOSTIC_VECTOR_STATISTICS
                    )
                )
                if group_name != "ul_ofdma":
                    additional += diagnostic_vector_overrides(group_name)
            if group_name == "bsr":
                additional += BSR_TRIGGER_DECISION_VECTOR_OVERRIDES
            job = replace(job, command=job.command + additional)
            jobs.append(job)
    return jobs


def execute_job(job: CampaignJob) -> JobResult:
    print("RUN", job.label, "::", " ".join(job.command), flush=True)
    job.result_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as output:
        completed = subprocess.run(
            job.command,
            cwd=REPOSITORY_ROOT,
            stdout=output,
            stderr=subprocess.STDOUT,
            check=False,
        )
        if completed.returncode:
            output.seek(0)
            return JobResult(job, completed.returncode, output.read())
    return JobResult(job, 0, "")


def run_jobs(jobs: list[CampaignJob], jobs_limit: int) -> bool:
    if not jobs:
        print("No simulation runs matched the selection.", file=sys.stderr)
        return False

    workers = min(jobs_limit, len(jobs))
    print(f"Executing {len(jobs)} simulation runs with {workers} parallel worker(s).", flush=True)
    failures = []
    with ThreadPoolExecutor(max_workers=workers, thread_name_prefix="simulation") as executor:
        futures = {executor.submit(execute_job, job): job for job in jobs}
        for completed_count, future in enumerate(as_completed(futures), 1):
            result = future.result()
            status = "DONE" if result.returncode == 0 else f"FAILED ({result.returncode})"
            print(f"[{completed_count}/{len(jobs)}] {status} {result.job.label}", flush=True)
            if result.returncode:
                failures.append(result)

    for failure in failures:
        print(f"\n===== {failure.job.label}: exit {failure.returncode} =====", file=sys.stderr)
        print(failure.output.rstrip(), file=sys.stderr)
    return not failures


def result_artifacts(job: CampaignJob) -> tuple[Path, Path]:
    artifacts = []
    for extension in ("sca", "vec"):
        matches = sorted(
            job.result_dir.glob(f"{job.config}*-#{job.run}.{extension}")
        )
        if len(matches) != 1:
            raise RuntimeError(
                f"{job.label}: expected one .{extension} result, "
                f"found {len(matches)}"
            )
        artifacts.append(matches[0])
    return artifacts[0], artifacts[1]


def parse_query_output(output: str) -> list[RecordedResult]:
    parsed = []
    for line in output.splitlines():
        fields = line.split()
        if len(fields) < 4 or fields[0] not in {"scalar", "vector"}:
            continue
        kind, module, name = fields[:3]
        if kind == "scalar":
            try:
                value = float(fields[3])
            except ValueError:
                continue
            parsed.append(RecordedResult(kind, module, name, value=value))
        else:
            count_field = next(
                (field for field in fields[3:] if field.startswith("count=")),
                None,
            )
            if count_field is None:
                continue
            maximum_field = next(
                (field for field in fields[3:] if field.startswith("max=")),
                None,
            )
            parsed.append(RecordedResult(
                kind,
                module,
                name,
                count=int(count_field.split("=", 1)[1]),
                maximum=(
                    float(maximum_field.split("=", 1)[1])
                    if maximum_field is not None
                    else None
                ),
            ))
    return parsed


def query_job_results(job: CampaignJob) -> list[RecordedResult]:
    scalar, vector = result_artifacts(job)
    completed = subprocess.run(
        ["opp_scavetool", "query", "-l", str(scalar), str(vector)],
        cwd=REPOSITORY_ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode:
        raise RuntimeError(
            f"{job.label}: opp_scavetool query failed: "
            f"{completed.stderr.strip()}"
        )
    return parse_query_output(completed.stdout)


def module_matches(module: str, pattern: str) -> bool:
    expression = re.escape(pattern)
    expression = expression.replace(r"\*\*", ".*")
    expression = expression.replace(r"\*", r"[^.]*")
    return re.fullmatch(expression, module) is not None


def requirements_for_job(
    job: CampaignJob,
    manifest: dict[str, Any],
    diagnostic_vectors: bool = False,
) -> tuple[RequiredResult, ...]:
    requirements = PERFORMANCE_REQUIREMENTS
    group = manifest["groups"][job.group]
    if (
        not diagnostic_vectors
        or job.group != "ul_ofdma"
        or job.run != diagnostic_run(group)
    ):
        return requirements
    if job.config == "EdcaBaseline":
        return requirements + (
            UL_OFDMA_DIAGNOSTIC_REQUIREMENTS[0],
        ) + UL_OFDMA_EDCA_INACTIVE_REQUIREMENTS
    return requirements + UL_OFDMA_DIAGNOSTIC_REQUIREMENTS


def validate_requirement(
    records: list[RecordedResult],
    requirement: RequiredResult,
) -> str | None:
    matches = [
        record
        for record in records
        if record.kind == requirement.kind
        and record.name == requirement.name
        and module_matches(record.module, requirement.module)
    ]
    if not matches:
        return (
            f"missing {requirement.kind} {requirement.module} "
            f"{requirement.name}"
        )
    total = sum(
        record.count if record.kind == "vector" else record.value or 0
        for record in matches
    )
    if not math.isfinite(total):
        return (
            f"non-finite {requirement.kind} "
            f"{requirement.module} {requirement.name}"
        )
    if requirement.expectation == "nonzero" and total <= 0:
        return (
            f"expected nonzero {requirement.kind} "
            f"{requirement.module} {requirement.name}"
        )
    if requirement.expectation == "positive":
        maximum = max(
            (
                record.maximum
                for record in matches
                if record.maximum is not None
                and math.isfinite(record.maximum)
            ),
            default=0,
        )
        if maximum <= 0:
            return (
                f"expected positive {requirement.kind} "
                f"{requirement.module} {requirement.name}"
            )
    if requirement.expectation == "zero" and total != 0:
        return (
            f"expected inactive {requirement.kind} "
            f"{requirement.module} {requirement.name}, observed {total:g}"
        )
    return None


def validate_campaign_results(
    jobs: list[CampaignJob],
    manifest: dict[str, Any],
    diagnostic_vectors: bool = False,
) -> list[str]:
    errors = []
    for job in jobs:
        try:
            records = query_job_results(job)
        except RuntimeError as error:
            errors.append(str(error))
            continue
        for requirement in requirements_for_job(
            job,
            manifest,
            diagnostic_vectors,
        ):
            error = validate_requirement(records, requirement)
            if error is not None:
                errors.append(f"{job.label}: {error}")
    return errors


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("group", help="manifest group name, or 'all'")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--runs", type=positive_int, help="override repetition count for a diagnostic campaign")
    parser.add_argument("--config", action="append", help="run only the named configuration")
    parser.add_argument(
        "--pcap-run",
        type=int,
        help="record PCAPng files for this run number (normally 0)",
    )
    parser.add_argument(
        "--pcap-interface-pattern",
        action="append",
        default=[],
        help="module pattern whose WLAN interfaces record PCAPng; repeat as needed",
    )
    parser.add_argument(
        "--exhaustive-vectors",
        action="store_true",
        help="record the example's curated diagnostic vectors for run 0",
    )
    parser.add_argument(
        "--session-id",
        type=session_id,
        help="UTC result-set ID (default: current time as YYYYMMDDTHHMMSSZ)",
    )
    parser.add_argument(
        "-j", "--jobs", type=positive_int, default=available_cpu_count(),
        help="maximum parallel simulations (default: all available CPUs, equivalent to nproc)",
    )
    args = parser.parse_args()
    manifest = load_manifest(args.manifest)
    if args.group != "all" and args.group not in manifest["groups"]:
        parser.error(f"unknown group {args.group!r}; choose from: all, {', '.join(sorted(manifest['groups']))}")

    campaign_session_id = args.session_id or new_session_id()
    repetitions = args.runs or max(
        int(group["expected_repetitions"])
        for group in manifest["groups"].values()
    )
    if args.pcap_run is not None:
        if not args.pcap_interface_pattern:
            parser.error("--pcap-run requires --pcap-interface-pattern")
        if args.pcap_run < 0 or args.pcap_run >= repetitions:
            parser.error(f"--pcap-run must be in [0, {repetitions})")
    print(f"Campaign session: {campaign_session_id}", flush=True)
    jobs = collect_jobs(
        manifest,
        args.group,
        repetitions_override=args.runs,
        selected_configs=set(args.config) if args.config else None,
        campaign_session_id=campaign_session_id,
        pcap_run=args.pcap_run,
        pcap_interface_patterns=tuple(args.pcap_interface_pattern),
        exhaustive_vectors=args.exhaustive_vectors,
    )
    if not run_jobs(jobs, args.jobs):
        raise SystemExit(1)
    validation_errors = validate_campaign_results(
        jobs,
        manifest,
        diagnostic_vectors=args.exhaustive_vectors,
    )
    if validation_errors:
        print("\nCampaign result validation failed:", file=sys.stderr)
        for error in validation_errors:
            print(f"- {error}", file=sys.stderr)
        raise SystemExit(1)
    print(
        f"Validated required result artifacts and evidence for "
        f"{len(jobs)} run(s).",
        flush=True,
    )


if __name__ == "__main__":
    main()
