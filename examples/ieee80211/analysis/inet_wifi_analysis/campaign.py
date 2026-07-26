"""Generation-neutral Cmdenv campaign construction."""

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

from .paths import result_configuration_directory, result_root


@dataclass(frozen=True)
class CampaignJob:
    group: str
    config: str
    run: int
    result_dir: Path
    command: tuple[str, ...]

    @property
    def label(self) -> str:
        return f"{self.group}/{self.config} run {self.run}"


def build_cmdenv_command(
    repository_root: Path,
    ini: Path,
    result_dir: Path,
    config: str,
    run: int,
    repetitions: int,
    vector_statistics: Iterable[str] = (),
    scalar_statistics: Iterable[str] = (),
    pcap_interface_patterns: Iterable[str] = (),
    config_overrides: Iterable[str] = (),
) -> tuple[str, ...]:
    command = [
        str(repository_root / "bin/inet"),
        "-u",
        "Cmdenv",
        "-f",
        str(ini),
        "-c",
        config,
        "-r",
        str(run),
        f"--repeat={repetitions}",
        f"--result-dir={result_dir}",
        f"--seed-set={run}",
        "--**.vector-recording=false",
        "--**.scalar-recording=false",
    ]
    command.extend(
        f"--**.{statistic}*.scalar-recording=true"
        for statistic in scalar_statistics
    )
    command.extend(
        f"--**.{statistic}*.vector-recording=true"
        for statistic in vector_statistics
    )
    interface_patterns = tuple(pcap_interface_patterns)
    if interface_patterns:
        command.extend(
            f"--{pattern}.recordPcap=true"
            for pattern in interface_patterns
        )
        command.extend((
            '--**.wlan[*].pcapRecorder[*].moduleNamePatterns="mac"',
            "--**.wlan[*].pcapRecorder[*].verbose=false",
            '--**.wlan[*].pcapRecorder[*].fileFormat="pcapng"',
            '--**.checksumMode="computed"',
            '--**.fcsMode="computed"',
        ))
    command.extend(config_overrides)
    return tuple(command)


def collect_campaign_jobs(
    manifest: dict[str, Any],
    selected_group: str,
    repository_root: Path,
    session_id: str,
    vector_statistics: Iterable[str] = (),
    scalar_statistics: Iterable[str] = (),
    repetitions_override: int | None = None,
    selected_configs: set[str] | None = None,
    pcap_run: int | None = None,
    pcap_interface_patterns: Iterable[str] = (),
) -> list[CampaignJob]:
    group_names = (
        sorted(manifest["groups"])
        if selected_group == "all"
        else [selected_group]
    )
    jobs = []
    for group_name in group_names:
        group = manifest["groups"][group_name]
        if "result_dir" in group or any(
            "result_dir" in entry for entry in group["conditions"]
        ):
            raise ValueError(
                f"{group_name}: result_dir is obsolete; results are derived "
                "from the effective INI"
            )
        result_roots = {
            result_root(
                repository_root,
                entry.get("ini", group["ini"]),
            )
            for entry in group["conditions"]
        }
        if len(result_roots) != 1:
            raise ValueError(
                f"{group_name}: all configurations must belong to one "
                "simulation example"
            )
        repetitions = repetitions_override or int(group["expected_repetitions"])
        for entry in group["conditions"]:
            if selected_configs and entry["config"] not in selected_configs:
                continue
            ini = repository_root / entry.get("ini", group["ini"])
            result_dir = result_configuration_directory(
                repository_root,
                ini,
                session_id,
                entry["config"],
            )
            for run in range(repetitions):
                jobs.append(CampaignJob(
                    group=group_name,
                    config=entry["config"],
                    run=run,
                    result_dir=result_dir,
                    command=build_cmdenv_command(
                        repository_root,
                        ini,
                        result_dir,
                        entry["config"],
                        run,
                        repetitions,
                        vector_statistics,
                        scalar_statistics,
                        (
                            pcap_interface_patterns
                            if run == pcap_run
                            else ()
                        ),
                        entry.get("command_overrides", ()),
                    ),
                ))
    return jobs
