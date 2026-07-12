#!/usr/bin/env python3
"""Generate the complete IEEE 802.11ax first-tranche figure set."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from omnetpp.scave import results


OPTIONS = dict(include_attrs=True, include_runattrs=True, include_itervars=True, include_config_entries=True)


@dataclass(frozen=True)
class Dataset:
    label: str
    path: Path

    def activate(self) -> None:
        if not self.path.exists():
            raise FileNotFoundError(self.path)
        results.set_inputs(str(self.path))

    def vectors(self, name: str) -> pd.DataFrame:
        self.activate()
        try:
            frame = results.get_vectors(f'name =~ "*{name}*"', **OPTIONS)
        except ValueError as error:
            if "duplicate entries" not in str(error):
                raise
            fallback = dict(OPTIONS)
            fallback["include_attrs"] = False
            frame = results.get_vectors(f'name =~ "*{name}*"', **fallback)
        if frame.empty:
            return frame
        valid = []
        for index, row in frame.iterrows():
            times = np.asarray(row.vectime, dtype=float)
            values = np.asarray(row.vecvalue, dtype=float)
            if len(times) != len(values):
                raise RuntimeError(f"{self.label}/{name}: mismatched vector arrays")
            if len(times) and np.any(np.diff(times) < 0):
                raise RuntimeError(f"{self.label}/{name}: non-monotonic time")
            if len(times):
                valid.append(index)
        frame = frame.loc[valid].copy()
        print(f"{self.label}: {name}: {len(frame)} nonempty vector(s)")
        return frame

    def scalars(self, name: str) -> pd.DataFrame:
        self.activate()
        try:
            frame = results.get_scalars(f'name =~ "*{name}*"', **OPTIONS)
        except ValueError as error:
            if "duplicate entries" not in str(error):
                raise
            fallback = dict(OPTIONS)
            fallback["include_attrs"] = False
            frame = results.get_scalars(f'name =~ "*{name}*"', **fallback)
        if not frame.empty:
            frame["value"] = pd.to_numeric(frame["value"], errors="raise")
        print(f"{self.label}: {name}: {len(frame)} scalar(s)")
        return frame


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("group", choices=("rate", "bsr", "bss", "width", "twt", "dl", "uora", "puncturing", "fragmentation", "mimo"))
    parser.add_argument("--input", action="append", required=True, metavar="LABEL=PATH")
    parser.add_argument("--output-dir", type=Path, required=True)
    return parser.parse_args()


def parse_inputs(items: list[str]) -> list[Dataset]:
    datasets = []
    for item in items:
        if "=" not in item:
            raise ValueError(f"Expected LABEL=PATH, got {item!r}")
        label, path = item.split("=", 1)
        datasets.append(Dataset(label, Path(path)))
    return datasets


def save(fig: plt.Figure, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(path, dpi=200, bbox_inches="tight")
    plt.close(fig)
    print(f"CREATED {path}")


def node(module: str) -> str:
    match = re.search(r"(?:^|\.)((?:host|sta\d*|ap\d*|server\d*)(?:\[\d+\])?)(?:\.|$)", module)
    return match.group(1) if match else module.split(".")[0]


def all_values(frame: pd.DataFrame) -> np.ndarray:
    return np.concatenate([np.asarray(value, dtype=float) for value in frame.vecvalue]) if not frame.empty else np.array([])


def longest_vector(frame: pd.DataFrame) -> pd.Series:
    if frame.empty:
        raise RuntimeError("Cannot select a vector from an empty result frame")
    return frame.loc[frame.vecvalue.map(len).idxmax()]


def aligned_longest(*frames: pd.DataFrame) -> list[pd.Series]:
    """Select synchronized telemetry vectors from the module with most samples."""
    common_modules = set(frames[0].module)
    for frame in frames[1:]:
        common_modules &= set(frame.module)
    if not common_modules:
        raise RuntimeError("Measured telemetry vectors do not share a recording module")
    module = max(common_modules, key=lambda item: len(frames[0][frames[0].module == item].iloc[0].vecvalue))
    rows = [frame[frame.module == module].iloc[0] for frame in frames]
    if len({len(row.vecvalue) for row in rows}) != 1:
        raise RuntimeError(f"Measured telemetry vectors are not aligned for {module}")
    return rows


def duration(dataset: Dataset) -> float:
    for name in ("endToEndDelay:vector", "packetReceived:vector", "radioMode:vector", "transmissionState:vector"):
        frame = dataset.vectors(name)
        if not frame.empty:
            return max(float(np.max(times)) for times in frame.vectime)
    scalar = dataset.scalars("simulated time")
    return float(scalar.value.max()) if not scalar.empty else 1.0


def sink_goodputs(dataset: Dataset) -> dict[str, float]:
    frame = dataset.vectors("packetReceived:vector(packetBytes)")
    if frame.empty:
        frame = dataset.vectors("packetReceived:vector")
    result: dict[str, float] = {}
    total_time = duration(dataset)
    for _, row in frame.iterrows():
        if ".app[" not in row.module:
            continue
        result[node(row.module)] = result.get(node(row.module), 0.0) + float(np.sum(row.vecvalue)) * 8 / total_time
    return result


def jain(values: list[float]) -> float:
    array = np.asarray(values, dtype=float)
    return float(array.sum() ** 2 / (len(array) * np.square(array).sum())) if len(array) and np.square(array).sum() else 0.0


def ecdf(axis, values: np.ndarray, label: str) -> None:
    values = np.sort(values)
    if len(values):
        axis.step(values, np.arange(1, len(values) + 1) / len(values), where="post", label=label)


def scalar_sum(dataset: Dataset, name: str) -> float:
    frame = dataset.scalars(name)
    return float(frame.value.sum()) if not frame.empty else 0.0


def plot_rate(datasets: list[Dataset], out: Path) -> None:
    dataset = datasets[0]
    names = ("heRateSelectedMcs:vector", "heRateSelectedNss:vector", "datarateSelected:vector", "throughput:vector")
    frames = [(name, dataset.vectors(name)) for name in names]
    frames = [(name, frame) for name, frame in frames if not frame.empty]
    if not frames:
        raise RuntimeError("Rate suite has no populated rate/throughput vectors")
    fig, axes = plt.subplots(len(frames), 1, figsize=(11, 2.7 * len(frames)), sharex=True, squeeze=False)
    labels = {names[0]: "MCS", names[1]: "NSS", names[2]: "Selected rate [bit/s]", names[3]: "Throughput [bit/s]"}
    for axis, (name, frame) in zip(axes[:, 0], frames):
        for _, row in frame.head(8).iterrows():
            axis.step(row.vectime, row.vecvalue, where="post", label=node(row.module), alpha=.8)
        axis.set_ylabel(labels[name]); axis.grid(alpha=.3)
    axes[0, 0].set_title("Rate adaptation decisions and delivered throughput")
    axes[-1, 0].set_xlabel("Simulation time [s]")
    axes[0, 0].legend(fontsize="small", ncol=4)
    save(fig, out / "rate-adaptation-timeline.png")


def plot_bsr(datasets: list[Dataset], out: Path) -> None:
    dataset = datasets[0]
    reported = dataset.vectors("heUlBufferStatusReportedBytes:vector")
    scheduled = dataset.vectors("heUlBufferStatusScheduledBytes:vector")
    if reported.empty or scheduled.empty:
        raise RuntimeError("BSR suite requires reported and scheduled byte vectors")
    fig, axes = plt.subplots(2, 1, figsize=(11, 7), sharex=True)
    for axis, frame, title in ((axes[0], reported, "AP view: bytes reported in BSRs"), (axes[1], scheduled, "Bytes selected for UL scheduling")):
        for _, row in frame.iterrows(): axis.step(row.vectime, row.vecvalue, where="post", label=node(row.module))
        axis.set_ylabel("Bytes"); axis.set_title(title); axis.grid(alpha=.3)
    axes[-1].set_xlabel("Simulation time [s]")
    save(fig, out / "bsr-reported-vs-scheduled.png")


def overlap_fraction(dataset: Dataset) -> float:
    frame = dataset.vectors("transmissionState:vector")
    aps = [row for _, row in frame.iterrows() if re.search(r"\.ap\d*\.wlan|\.ap\d*\[", row.module)]
    if len(aps) < 2: return 0.0
    end = max(float(np.max(row.vectime)) for row in aps); grid = np.linspace(0, end, 5000)
    active = []
    for row in aps[:2]:
        t, v = np.asarray(row.vectime), np.asarray(row.vecvalue)
        active.append(v[np.clip(np.searchsorted(t, grid, side="right") - 1, 0, len(v)-1)] != 0)
    return float(np.mean(active[0] & active[1]))


def plot_bss(datasets: list[Dataset], out: Path) -> None:
    labels, totals, fairness, overlap = [], [], [], []
    for data in datasets:
        rates = sink_goodputs(data); labels.append(data.label); totals.append(sum(rates.values()) / 1e6); fairness.append(jain(list(rates.values()))); overlap.append(overlap_fraction(data) * 100)
    fig, axes = plt.subplots(1, 3, figsize=(14, 4.5))
    axes[0].bar(labels, totals); axes[0].set_ylabel("Aggregate goodput [Mbit/s]")
    axes[1].bar(labels, fairness); axes[1].set_ylabel("Jain fairness"); axes[1].set_ylim(0, 1.05)
    axes[2].bar(labels, overlap); axes[2].set_ylabel("Concurrent AP airtime [%]")
    for axis in axes: axis.tick_params(axis="x", rotation=25); axis.grid(axis="y", alpha=.3)
    fig.suptitle("BSS coloring: capacity, fairness, and spatial reuse")
    save(fig, out / "bss-coloring-comparison.png")


def bandwidth(label: str) -> float:
    match = re.search(r"(20|40|80|160)", label)
    if not match: raise ValueError(f"Channel-width label must contain 20/40/80/160: {label}")
    return float(match.group(1))


def plot_width(datasets: list[Dataset], out: Path) -> None:
    labels, goods, spectral = [], [], []
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.6))
    for data in datasets:
        good = sum(sink_goodputs(data).values()) / 1e6; bw = bandwidth(data.label)
        labels.append(data.label); goods.append(good); spectral.append(good / bw)
        delay = all_values(data.vectors("endToEndDelay:vector")) * 1e3; ecdf(axes[2], delay, data.label)
    axes[0].bar(labels, goods); axes[0].set_ylabel("Aggregate goodput [Mbit/s]")
    axes[1].bar(labels, spectral); axes[1].set_ylabel("Goodput spectral efficiency [bit/s/Hz]")
    axes[2].set(xlabel="End-to-end delay [ms]", ylabel="ECDF"); axes[2].legend(fontsize="small")
    for axis in axes: axis.tick_params(axis="x", rotation=25); axis.grid(alpha=.3)
    fig.suptitle("HE channel-width capacity, efficiency, and delay")
    save(fig, out / "channel-width-dashboard.png")


def radio_raster(dataset: Dataset, axis) -> None:
    frame = dataset.vectors("radioMode:vector").head(8)
    end = max(float(np.max(t)) for t in frame.vectime); grid = np.linspace(0, end, 2000); rows=[]; labels=[]
    for _, row in frame.iterrows():
        t, v = np.asarray(row.vectime), np.asarray(row.vecvalue); rows.append(v[np.clip(np.searchsorted(t, grid, side="right")-1, 0, len(v)-1)]); labels.append(node(row.module))
    image=axis.imshow(rows, aspect="auto", interpolation="nearest", extent=(0,end,len(rows)-.5,-.5), cmap="viridis")
    axis.set_yticks(range(len(labels)), labels); axis.set(xlabel="Time [s]", title=f"{dataset.label}: radio states")
    return image


def energy(dataset: Dataset) -> float:
    for name in ("energyConsumption:sum", "energyConsumed:sum", "totalEnergyConsumed", "energyBalance"):
        value = scalar_sum(dataset, name)
        if value: return value
    frame = dataset.vectors("powerConsumption:vector")
    total = 0.0
    for _, row in frame.iterrows():
        if ".radio.energyConsumer" not in row.module:
            continue
        times = np.asarray(row.vectime, dtype=float)
        values = np.asarray(row.vecvalue, dtype=float)
        if len(times) > 1:
            total += float(np.sum(values[:-1] * np.diff(times)))
    return total


def plot_twt(datasets: list[Dataset], out: Path) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    image = radio_raster(datasets[-1], axes[0]); fig.colorbar(image, ax=axes[0], label="Radio mode code")
    values=[]
    for data in datasets:
        bits=sum(sink_goodputs(data).values()) * duration(data); consumed=energy(data); values.append(consumed / bits if bits and consumed else np.nan)
    axes[1].bar([d.label for d in datasets], np.nan_to_num(values)); axes[1].set_ylabel("Energy per delivered bit [J/bit]"); axes[1].tick_params(axis="x", rotation=25); axes[1].grid(axis="y", alpha=.3)
    for index, value in enumerate(values):
        if np.isnan(value): axes[1].text(index, 0, "no delivered bits", ha="center", va="bottom", rotation=90)
    fig.suptitle("TWT sleep scheduling and energy efficiency")
    save(fig, out / "twt-state-and-energy.png")


def plot_dl(datasets: list[Dataset], out: Path) -> None:
    labels=[]; totals=[]; delays=[]; fairs=[]
    for data in datasets:
        rates=sink_goodputs(data); labels.append(data.label); totals.append(sum(rates.values())/1e6); fairs.append(jain(list(rates.values()))); delay=all_values(data.vectors("endToEndDelay:vector")); delays.append(np.percentile(delay,95)*1e3 if len(delay) else 0)
    fig, axes=plt.subplots(1,3,figsize=(14,4.5))
    for axis, vals, ylabel in zip(axes,(totals,delays,fairs),("Goodput [Mbit/s]","95th-percentile delay [ms]","Jain fairness")): axis.bar(labels,vals); axis.set_ylabel(ylabel); axis.tick_params(axis="x",rotation=25); axis.grid(axis="y",alpha=.3)
    axes[2].set_ylim(0,1.05); fig.suptitle("Downlink scheduler dashboard")
    save(fig,out/"dl-scheduler-dashboard.png")


def plot_uora(datasets: list[Dataset], out: Path) -> None:
    labels=[]; success=[]; collision=[]; fairs=[]
    for data in datasets:
        attempt_frame=data.scalars("heUlRandomAccessAttempt:count"); success_frame=data.scalars("heUlRandomAccessSuccess:count")
        attempt_frame=attempt_frame[attempt_frame.module.str.contains(r"\.host\[",regex=True)]; success_frame=success_frame[success_frame.module.str.contains(r"\.host\[",regex=True)]
        attempts=float(attempt_frame.value.sum()); successes=float(success_frame.value.sum()); labels.append(data.label); success.append(successes/attempts if attempts else 0); collision.append(max(attempts-successes,0)); fairs.append(jain(success_frame.value.tolist()))
    fig,axes=plt.subplots(1,3,figsize=(14,4.5))
    for axis,vals,ylabel in zip(axes,(success,collision,fairs),("UORA success probability","Unsuccessful attempts","Jain fairness")): axis.bar(labels,vals); axis.set_ylabel(ylabel); axis.tick_params(axis="x",rotation=25); axis.grid(axis="y",alpha=.3)
    axes[0].set_ylim(0,1.05); axes[2].set_ylim(0,1.05); fig.suptitle("UORA contention, success, and fairness")
    save(fig,out/"uora-dashboard.png")


def plot_puncturing(datasets: list[Dataset], out: Path) -> None:
    fig,axes=plt.subplots(1,3,figsize=(18,4.8)); labels=[]; goods=[]
    for data in datasets: labels.append(data.label); goods.append(sum(sink_goodputs(data).values())/1e6)
    axes[0].bar(labels,goods); axes[0].set_ylabel("Goodput [Mbit/s]"); axes[0].tick_params(axis="x",rotation=25); axes[0].grid(axis="y",alpha=.3)
    measured = datasets[-1]
    offsets=measured.vectors("heRuToneOffset:vector"); sizes=measured.vectors("heRuToneSize:vector"); staids=measured.vectors("heStaId:vector"); masks=measured.vectors("hePuncturedSubchannelMask:vector")
    if offsets.empty or sizes.empty or staids.empty or masks.empty:
        raise RuntimeError("Puncturing suite requires measured HE RU placement and puncturing-mask vectors")
    offset_row, size_row, sta_row=aligned_longest(offsets,sizes,staids)
    scatter=axes[1].scatter(offset_row.vectime, offset_row.vecvalue, s=np.maximum(12,np.asarray(size_row.vecvalue)/2), c=sta_row.vecvalue, cmap="tab20", alpha=.7)
    axes[1].set(xlabel="Simulation time [s]",ylabel="RU tone offset",title=f"{measured.label}: measured RU placement")
    axes[1].grid(alpha=.3); fig.colorbar(scatter,ax=axes[1],label="STA ID")
    mask_row=longest_vector(masks)
    axes[2].step(mask_row.vectime,np.asarray(mask_row.vecvalue,dtype=np.int64),where="post")
    axes[2].set(xlabel="Simulation time [s]",ylabel="Punctured-subchannel bit mask",title="Resolved mask per transmitted HE PPDU")
    axes[2].grid(alpha=.3)
    fig.suptitle("Preamble puncturing: delivered goodput and frequency allocation")
    save(fig,out/"puncturing-frequency-allocation.png")


def plot_fragmentation(datasets: list[Dataset], out: Path) -> None:
    fig,axes=plt.subplots(1,2,figsize=(13,4.8)); labels=[]; per_dataset=[]
    for data in datasets:
        frame=data.vectors("packetSentToPeer:vector(packetBytes)"); sizes=all_values(frame)/8; ecdf(axes[0],sizes,data.label); labels.append(data.label)
        types=data.vectors("acknowledgmentFrameType:vector"); airtimes=data.vectors("acknowledgmentAirtime:vector")
        if types.empty or airtimes.empty: raise RuntimeError("Fragmentation suite requires measured acknowledgment type/airtime vectors")
        type_row,airtime_row=aligned_longest(types,airtimes)
        type_values=np.asarray(type_row.vecvalue,dtype=int); airtime_values=np.asarray(airtime_row.vecvalue,dtype=float)
        per_dataset.append({int(frame_type):float(airtime_values[type_values==frame_type].sum())*1e3 for frame_type in np.unique(type_values)})
    axes[0].set(xlabel="Transmitted MAC frame size [bytes]",ylabel="ECDF",title="Measured transmitted-frame size distribution"); axes[0].legend(fontsize="small"); axes[0].grid(alpha=.3)
    frame_names={0x1d:"ACK",0x18:"Block Ack Request",0x19:"Block Ack"}; frame_types=sorted(set().union(*(values.keys() for values in per_dataset)))
    x=np.arange(len(labels)); width=.8/max(1,len(frame_types))
    for index,frame_type in enumerate(frame_types): axes[1].bar(x+index*width,[values.get(frame_type,0) for values in per_dataset],width,label=frame_names.get(frame_type,f"type {frame_type}"))
    axes[1].set_xticks(x+width*(len(frame_types)-1)/2,labels); axes[1].set_ylabel("Measured acknowledgment airtime [ms]"); axes[1].set_title("Frame-type-specific acknowledgment airtime"); axes[1].tick_params(axis="x",rotation=25); axes[1].grid(axis="y",alpha=.3); axes[1].legend(fontsize="small")
    save(fig,out/"fragmentation-and-ack-overhead.png")


def plot_mimo(datasets: list[Dataset], out: Path) -> None:
    labels=[data.label for data in datasets]
    measured=datasets[-1]; staids=measured.vectors("heStaId:vector"); streams=measured.vectors("heSpatialStreams:vector"); starts=measured.vectors("heStreamStartIndex:vector")
    if staids.empty or streams.empty or starts.empty: raise RuntimeError("MU-MIMO suite requires measured STA/NSS/stream-start vectors")
    sta_row,stream_row,start_row=aligned_longest(staids,streams,starts)
    times=np.asarray(sta_row.vectime); station_ids=np.asarray(sta_row.vecvalue,dtype=int); nss=np.asarray(stream_row.vecvalue,dtype=float); start_indices=np.asarray(start_row.vecvalue,dtype=int)
    ppdu_times=np.unique(times); stations=np.unique(station_ids); matrix=np.full((len(stations),len(ppdu_times)),np.nan); starts_matrix=np.full_like(matrix,np.nan)
    time_index={value:index for index,value in enumerate(ppdu_times)}; station_index={value:index for index,value in enumerate(stations)}
    for time,station,count,start in zip(times,station_ids,nss,start_indices): matrix[station_index[station],time_index[time]]=count; starts_matrix[station_index[station],time_index[time]]=start
    fig,axes=plt.subplots(1,2,figsize=(14,4.8)); im=axes[0].imshow(matrix,aspect="auto",interpolation="nearest",origin="lower",extent=(ppdu_times[0],ppdu_times[-1],-.5,len(stations)-.5),vmin=0.5,cmap="viridis"); axes[0].set_yticks(range(len(stations)),stations); axes[0].set(xlabel="PPDU transmission time [s]",ylabel="STA ID",title=f"{measured.label}: measured NSS per PPDU"); fig.colorbar(im,ax=axes[0],label="Allocated spatial streams (NSS)")
    goods=[sum(sink_goodputs(data).values())/1e6 for data in datasets]; axes[1].bar(labels,goods); axes[1].set_ylabel("Aggregate goodput [Mbit/s]"); axes[1].tick_params(axis="x",rotation=25); axes[1].grid(axis="y",alpha=.3); axes[1].set_title("Measured MU-MIMO delivery")
    fig.suptitle("MU-MIMO spatial compatibility and delivered capacity")
    save(fig,out/"mu-mimo-spatial-stream-matrix.png")


PLOTS={"rate":plot_rate,"bsr":plot_bsr,"bss":plot_bss,"width":plot_width,"twt":plot_twt,"dl":plot_dl,"uora":plot_uora,"puncturing":plot_puncturing,"fragmentation":plot_fragmentation,"mimo":plot_mimo}


def main() -> None:
    args=parse_args(); datasets=parse_inputs(args.input); PLOTS[args.group](datasets,args.output_dir)


if __name__ == "__main__": main()
