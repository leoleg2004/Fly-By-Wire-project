#!/usr/bin/env python3
"""
Real-Time Monitor — Logic Analyzer / Gantt Chart  v9.0
=======================================================
Reads timeline.csv  (columns: task, type, start, end, duration_ms)

Layout:  TOP  = timeline (RUN / PREEMPT / SLEEP + period-end lines)
         BOT  = full-width statistics table

C source matching:  the trace directory name (e.g. trace_results_app10f_*)
                    selects ONLY app10f.c for parameter extraction.

Usage
-----
  python monitorRealTime.py  <timeline.csv>  [output.png]  [source_dir]
"""

import sys, re, os, glob
from collections import defaultdict
from pathlib import Path

import numpy  as np
import pandas as pd
import matplotlib
import matplotlib.pyplot   as plt
import matplotlib.patches  as mpatches
import matplotlib.ticker   as mticker
import matplotlib.gridspec as gridspec
from matplotlib.lines import Line2D

# ─────────────────────────────────────────────────────────────────────────────
#  Palette
# ─────────────────────────────────────────────────────────────────────────────
C = dict(
    RUN      = "#43A047",   PREEMPT  = "#E53935",   SLEEP    = "#90A4AE",
    RESOURCE_WAIT = "#FB8C00",  RESOURCE_LOCK = "#5E35B1",
    BG_EVEN  = "#EEF2F7",  BG_ODD   = "#FFFFFF",
    HEADER   = "#263238",   PANEL_BG = "#F8FAFC",   BORDER   = "#CFD8DC",
    TEXT     = "#37474F",   DIM      = "#78909C",    ACCENT   = "#1565C0",
    WARN     = "#C62828",   OK       = "#2E7D32",    MISS     = "#FF1744",
    SLACK    = "#00C853",   PEND     = "#212121",
)
STATE_TYPES = {"RUN", "PREEMPT", "SLEEP", "RESOURCE_WAIT", "RESOURCE_LOCK"}
BAR_H  = 0.52
LANE_H = 1.0


# ═══════════════════════════════════════════════════════════════════════════════
#  1.  CSV LOADING
# ═══════════════════════════════════════════════════════════════════════════════
def load_csv(path: str) -> pd.DataFrame:
    rows = []
    with open(path, "r") as fh:
        fh.readline()
        for line in fh:
            line = line.strip()
            if not line:
                continue
            parts = line.split(",")
            if len(parts) < 5:
                continue
            try:
                dur   = float(parts[-1])
                end   = float(parts[-2])
                start = float(parts[-3])
            except ValueError:
                continue
            prefix = parts[: len(parts) - 3]
            task   = prefix[0].strip()
            etype  = ",".join(prefix[1:]).strip()
            rows.append((task, etype, start, end, dur))
    df = pd.DataFrame(rows, columns=["task", "type", "start", "end", "duration_ms"])
    df.sort_values("start", kind="mergesort", inplace=True)
    df.reset_index(drop=True, inplace=True)
    return df


# ═══════════════════════════════════════════════════════════════════════════════
#  2.  C SOURCE PARSER  — match ONLY the file for this trace variant
# ═══════════════════════════════════════════════════════════════════════════════
def _detect_app_variant(csv_path: str) -> str | None:
    """
    Extract app variant from trace directory name.
    e.g. 'trace_results_app10f_20260330_183511' → 'app10f'
         'trace_results_app10_20260327_155144'  → 'app10'
    """
    dirname = Path(csv_path).resolve().parent.name
    m = re.search(r'(app\d+[a-zA-Z]*)', dirname)
    return m.group(1) if m else None


def _parse_one_c_file(fpath: str) -> dict:
    """
    Parse a single C/CPP file. Returns {task_name: {period_ms, deadline_ms}}.

    Handles TWO code styles:
      Style A (named vars):  sprintf(activity_1.name, "Activity_1");
                              activity_1.period = 1800;
      Style B (array):       sprintf(activities[0].name, "Activity_1");
                              activities[0].period = 200;
    """
    result = {}

    try:
        with open(fpath) as f:
            text = f.read()
    except (OSError, UnicodeDecodeError):
        return result

    # Match both  sprintf(VAR.name, "Activity_N")  and
    #             sprintf(ARR[IDX].name, "Activity_N")
    # Capture the full accessor (e.g. "activity_1" or "activities[0]")
    p_name = re.compile(
        r'sprintf\s*\(\s*([\w\[\]]+)\.name\s*,\s*"(Activity_\d+)"\s*\)')
    # Match both  VAR.period = N  and  ARR[IDX].period = N
    p_per  = re.compile(r'([\w\[\]]+)\.period\s*=\s*(\d+)')
    p_dl   = re.compile(r'([\w\[\]]+)\.deadline\s*=\s*(\d+)')

    # Build accessor → Activity_N mapping
    accessor_to_name: dict = {}
    for m in p_name.finditer(text):
        accessor = m.group(1).strip()      # e.g. "activities[0]" or "activity_1"
        act_name = m.group(2)              # e.g. "Activity_1"
        accessor_to_name[accessor] = act_name

    for m in p_per.finditer(text):
        accessor = m.group(1).strip()
        name = accessor_to_name.get(accessor)
        if name:
            result.setdefault(name, {})["period_ms"] = int(m.group(2))

    for m in p_dl.finditer(text):
        accessor = m.group(1).strip()
        name = accessor_to_name.get(accessor)
        if name:
            result.setdefault(name, {})["deadline_ms"] = int(m.group(2))

    return result


def parse_c_sources(src_dir: str, csv_path: str) -> dict:
    """
    Scan C source files, but ONLY the one matching the trace variant.
    Falls back to scanning all files if no match is found.
    """
    variant = _detect_app_variant(csv_path)

    if variant:
        # Try exact match first: app10f.c, app10f.cpp
        for ext in (".c", ".cpp"):
            candidate = os.path.join(src_dir, variant + ext)
            if os.path.isfile(candidate):
                result = _parse_one_c_file(candidate)
                if result:
                    print(f"[..] C source: {os.path.basename(candidate)}  (matched variant '{variant}')")
                    return result

        # Try subdirectory: src_dir/app10f/*.c
        subdir = os.path.join(src_dir, variant)
        if os.path.isdir(subdir):
            merged = {}
            for ext in ("*.c", "*.cpp"):
                for fpath in glob.glob(os.path.join(subdir, ext)):
                    merged.update(_parse_one_c_file(fpath))
            if merged:
                print(f"[..] C source: {variant}/  (matched variant '{variant}')")
                return merged

    # Fallback: scan all, but warn
    print(f"[..] C source: no exact match for variant '{variant}', scanning all files")
    merged = {}
    for ext in ("*.c", "*.cpp"):
        for fpath in glob.glob(os.path.join(src_dir, "**", ext), recursive=True):
            merged.update(_parse_one_c_file(fpath))
    return merged


# ═══════════════════════════════════════════════════════════════════════════════
#  3.  TASK ORDERING
# ═══════════════════════════════════════════════════════════════════════════════
def _nsuf(n: str):
    m = re.search(r"(\d+)$", n)
    return (re.sub(r"\d+$", "", n), int(m.group(1)) if m else 0)

def order_tasks(tasks: list) -> list:
    act   = sorted([t for t in tasks if t.startswith("Activity_")],
                   key=_nsuf, reverse=True)
    sys_t = sorted([t for t in tasks if not t.startswith("Activity_")])
    return act + sys_t


# ═══════════════════════════════════════════════════════════════════════════════
#  4.  PERIOD ANALYSIS
# ═══════════════════════════════════════════════════════════════════════════════
def analyse_periods(task: str, markers: pd.DataFrame,
                    states: pd.DataFrame, c_params: dict) -> dict:
    """
    Analyse periods using PERIOD_START/END markers from the CSV and
    compute per-instance actual RUN/SLEEP times from the state blocks.
    """
    mk = markers[markers["task"] == task]
    st = states[states["task"] == task].sort_values("start")

    pstart = mk[mk["type"].str.contains("PERIOD_START", na=False)]["start"] \
               .sort_values().values
    pend   = mk[mk["type"].str.contains("PERIOD_END", na=False)]["start"] \
               .sort_values().values

    # app10e-style inline params
    marker_period_ms = marker_deadline_ms = None
    fps = mk[mk["type"].str.contains("PERIOD_START", na=False)]
    if not fps.empty:
        sample = fps.iloc[0]["type"]
        mp = re.search(r"Period\s*=\s*(\d+)", sample)
        md = re.search(r"Deadline\s*=\s*(\d+)", sample)
        if mp: marker_period_ms   = int(mp.group(1))
        if md: marker_deadline_ms = int(md.group(1))

    # ── Expected period (from C source or marker text) ────────────────────
    if task in c_params and "period_ms" in c_params[task]:
        expected_period_ms = float(c_params[task]["period_ms"])
    elif marker_period_ms:
        expected_period_ms = float(marker_period_ms)
    else:
        expected_period_ms = None

    # ── Measured period (from PERIOD_START gaps) ──────────────────────────
    if len(pstart) >= 2:
        gaps          = np.diff(pstart)
        measured_ms   = float(np.median(gaps) * 1000.0)
        jitter_ms     = float(np.std(gaps) * 1000.0)
    else:
        measured_ms   = None
        jitter_ms     = None

    period_ms = expected_period_ms if expected_period_ms else measured_ms

    # ── Deadline ──────────────────────────────────────────────────────────
    if task in c_params and "deadline_ms" in c_params[task]:
        deadline_ms = float(c_params[task]["deadline_ms"])
    elif marker_deadline_ms:
        deadline_ms = float(marker_deadline_ms)
    elif period_ms is not None:
        deadline_ms = period_ms
    else:
        deadline_ms = None

    n_dm = int(mk["type"].str.contains("DEADLINE_MISS", na=False).sum())

    # ── Helper: sum state durations overlapping [t_lo, t_hi] ─────────────
    def _state_time_in(stype: str, t_lo: float, t_hi: float) -> float:
        sub = st[st["type"] == stype]
        if sub.empty:
            return 0.0
        s_start = sub["start"].values
        s_end   = sub["end"].values
        lo = np.maximum(s_start, t_lo)
        hi = np.minimum(s_end, t_hi)
        overlap = np.maximum(hi - lo, 0.0)
        return float(overlap.sum())

    # ── Per-period instances (using PERIOD_END markers from CSV) ──────────
    instances = []
    for idx in range(len(pstart)):
        ps = pstart[idx]
        pe = None
        # Match this PERIOD_START to the next PERIOD_END
        if len(pend) > 0:
            cands = pend[pend > ps]
            if len(cands) > 0:
                pe = float(cands[0])
        # Fallback: next PERIOD_START
        if pe is None and idx + 1 < len(pstart):
            pe = float(pstart[idx + 1])

        if pe is not None:
            # Actual RUN and SLEEP from CSV state blocks within this period
            run_ms   = _state_time_in("RUN",   ps, pe) * 1000.0
            sleep_ms = _state_time_in("SLEEP", ps, pe) * 1000.0
            preempt_ms = _state_time_in("PREEMPT", ps, pe) * 1000.0
            elapsed_ms = (pe - ps) * 1000.0
            # Slack computed from actual run time vs deadline
            slack_ms = (deadline_ms - run_ms) if deadline_ms else None
            miss     = (slack_ms < 0) if slack_ms is not None else False
        else:
            run_ms = sleep_ms = preempt_ms = elapsed_ms = slack_ms = None
            miss = False
        instances.append(dict(start=ps, end=pe,
                              run_ms=run_ms, sleep_ms=sleep_ms,
                              preempt_ms=preempt_ms, elapsed_ms=elapsed_ms,
                              slack_ms=slack_ms, miss=miss))

    n_misses = max(sum(1 for i in instances if i["miss"]), n_dm)

    return dict(period_ms=period_ms, measured_ms=measured_ms,
                deadline_ms=deadline_ms,
                jitter_ms=jitter_ms, n_periods=len(pstart),
                instances=instances, n_misses=n_misses)


# ═══════════════════════════════════════════════════════════════════════════════
#  5.  PER-TASK STATISTICS
# ═══════════════════════════════════════════════════════════════════════════════
def build_stats(task: str, states: pd.DataFrame, markers: pd.DataFrame,
                csv_span_s: float, pi: dict) -> dict:
    sub = states[states["task"] == task].sort_values("start")

    dur = {}
    for t in STATE_TYPES:
        dur[t] = float((sub[sub["type"] == t]["end"] -
                         sub[sub["type"] == t]["start"]).sum())
    total_state = sum(dur.values())
    task_span   = float(sub["end"].max() - sub["start"].min()) if not sub.empty else 0.0

    if sub.shape[0] > 1:
        gs       = sub["start"].values[1:] - sub["end"].values[:-1]
        has_gaps = bool((gs > 1e-6).any())
        max_gap  = float(gs.max() * 1000.0)
        n_gaps   = int((gs > 1e-6).sum())
    else:
        has_gaps = False; max_gap = 0.0; n_gaps = 0

    rms = sub[sub["type"] == "RUN"]["duration_ms"].values
    if len(rms) > 0:
        wcet, acet, rmin, n_run = float(rms.max()), float(rms.mean()), float(rms.min()), len(rms)
    else:
        wcet = acet = rmin = 0.0; n_run = 0

    pms       = sub[sub["type"] == "PREEMPT"]["duration_ms"].values
    n_preempt = len(pms)
    pre_tot   = float(pms.sum()) if n_preempt else 0.0

    n_sleep = len(sub[sub["type"] == "SLEEP"])

    # Utilization: use EXPECTED period (from C source)
    if pi["period_ms"] and pi["n_periods"] >= 2:
        denom = (pi["n_periods"] - 1) * (pi["period_ms"] / 1000.0)
        util  = dur["RUN"] / denom if denom > 0 else 0.0
    else:
        util = dur["RUN"] / csv_span_s if csv_span_s > 0 else 0.0

    slacks = [i["slack_ms"] for i in pi["instances"] if i["slack_ms"] is not None]
    worst_slack = min(slacks) if slacks else None

    # Per-period run/sleep aggregates
    inst_runs   = [i["run_ms"]   for i in pi["instances"] if i["run_ms"]   is not None]
    inst_sleeps = [i["sleep_ms"] for i in pi["instances"] if i["sleep_ms"] is not None]
    avg_run_ms  = float(np.mean(inst_runs))  if inst_runs  else None
    avg_sleep_ms = float(np.mean(inst_sleeps)) if inst_sleeps else None

    return dict(
        total_state=total_state, task_span=task_span, dur=dur,
        has_gaps=has_gaps, n_gaps=n_gaps, max_gap_ms=max_gap,
        n_run=n_run, wcet=wcet, acet=acet, rmin=rmin,
        n_preempt=n_preempt, preempt_tot=pre_tot, n_sleep=n_sleep,
        period_ms=pi["period_ms"], measured_ms=pi["measured_ms"],
        deadline_ms=pi["deadline_ms"],
        jitter_ms=pi["jitter_ms"], n_periods=pi["n_periods"],
        n_misses=pi["n_misses"], utilization=util,
        worst_slack=worst_slack,
    )


# ═══════════════════════════════════════════════════════════════════════════════
#  6.  STATS TABLE  (dedicated bottom panel)
# ═══════════════════════════════════════════════════════════════════════════════
def draw_stats_table(ax, all_tasks: list, stats_map: dict):
    n = len(all_tasks)
    ROW_H = 1.0
    total_h = n + 1.5
    ax.set_xlim(0, 1)
    ax.set_ylim(-0.5, total_h)
    ax.set_yticks([])
    ax.xaxis.set_visible(False)
    for sp in ax.spines.values():
        sp.set_visible(False)
    ax.set_facecolor(C["PANEL_BG"])

    cols = [
        ("Task",             0.005),
        ("Period\n(ms)",     0.115),
        ("Measured\n(ms)",   0.185),
        ("Deadline\n(ms)",   0.265),
        ("WCET\n(ms)",       0.345),
        ("ACET\n(ms)",       0.420),
        ("Jitter\n(ms)",     0.495),
        ("Util %",           0.570),
        ("nRUN",             0.633),
        ("nPRE",             0.683),
        ("nSLP",             0.733),
        ("Miss",             0.783),
        ("W.Slack\n(ms)",    0.840),
        ("Gaps",             0.920),
    ]

    # Header
    header_y = total_h - 0.75
    ax.add_patch(mpatches.Rectangle(
        (0, header_y - 0.15), 1, 0.65,
        facecolor=C["HEADER"], zorder=1, clip_on=False))
    for lbl, x0 in cols:
        ax.text(x0, header_y + 0.10, lbl,
                fontsize=7.5, fontweight="bold", color="white",
                va="center", ha="left", linespacing=1.15,
                zorder=2, clip_on=False)
    ax.text(0.5, total_h - 0.05,
            "Task Statistics",
            fontsize=10, fontweight="bold", color=C["HEADER"],
            va="center", ha="center", clip_on=False)

    fm = lambda v, d=2: f"{v:.{d}f}" if v is not None else "—"
    fp = lambda v: f"{v*100:.1f}" if v is not None else "—"
    fi = lambda v: str(int(v))

    for i, task in enumerate(all_tasks):
        ry = (total_h - 1.5) - i * ROW_H
        bg = C["BG_EVEN"] if i % 2 == 0 else C["BG_ODD"]
        ax.add_patch(mpatches.Rectangle(
            (0, ry - 0.45), 1, 0.90,
            facecolor=bg, edgecolor=C["BORDER"], linewidth=0.3, zorder=0))

        s  = stats_map[task]
        ws = s["worst_slack"]

        warn_wcet = s["wcet"] > (s["deadline_ms"] if s["deadline_ms"] else 1e12)
        warn_util = s["utilization"] > 0.90
        warn_pre  = s["n_preempt"] > 0
        warn_miss = s["n_misses"] > 0
        warn_ws   = ws is not None and ws < 0
        # measured vs expected drift
        drift = (s["measured_ms"] is not None and s["period_ms"] is not None
                 and abs(s["measured_ms"] - s["period_ms"]) > s["period_ms"] * 0.05)

        vals = [
            task,
            fm(s["period_ms"]),
            fm(s["measured_ms"]),
            fm(s["deadline_ms"]),
            fm(s["wcet"]),
            fm(s["acet"]),
            fm(s["jitter_ms"]),
            fp(s["utilization"]),
            fi(s["n_run"]),
            fi(s["n_preempt"]),
            fi(s["n_sleep"]),
            fi(s["n_misses"]),
            fm(ws, 1) if ws is not None else "—",
            f"{s['n_gaps']}x{s['max_gap_ms']:.0f}" if s["has_gaps"] else "—",
        ]
        clrs = [
            C["HEADER"],
            C["ACCENT"] if s["period_ms"] else C["DIM"],
            C["WARN"] if drift else (C["TEXT"] if s["measured_ms"] else C["DIM"]),
            C["ACCENT"] if s["deadline_ms"] else C["DIM"],
            C["WARN"] if warn_wcet else C["TEXT"],
            C["TEXT"],
            C["DIM"],
            C["WARN"] if warn_util else C["OK"],
            C["TEXT"],
            C["WARN"] if warn_pre else C["TEXT"],
            C["TEXT"],
            C["MISS"] if warn_miss else C["OK"],
            C["MISS"] if warn_ws else (C["SLACK"] if ws is not None else C["DIM"]),
            C["WARN"] if s["has_gaps"] else C["DIM"],
        ]
        bolds = [True, True, drift,
                 True, warn_wcet, False, False,
                 True, False, warn_pre, False,
                 warn_miss, warn_ws or (ws is not None), s["has_gaps"]]

        for (lbl, x0), val, col, bold in zip(cols, vals, clrs, bolds):
            ax.text(x0, ry + 0.05, val,
                    fontsize=7.5 if lbl == "Task" else 8,
                    fontweight="bold" if bold else "normal",
                    color=col, va="center", ha="left", clip_on=True)

        # Stacked minibar
        bx, by, bw, bh = 0.005, ry - 0.38, 0.99, 0.20
        denom = s["total_state"] if s["total_state"] > 1e-9 else 1.0
        cx = bx
        for tt in ("RUN", "PREEMPT", "SLEEP"):
            frac = s["dur"].get(tt, 0.0) / denom
            if frac < 1e-6:
                continue
            ax.add_patch(mpatches.Rectangle(
                (cx, by), frac * bw, bh,
                facecolor=C[tt], edgecolor="none", zorder=2))
            if frac > 0.06:
                ax.text(cx + frac * bw / 2, by + bh / 2,
                        f"{frac*100:.0f}%", fontsize=6.5, color="white",
                        fontweight="bold", va="center", ha="center", zorder=3)
            cx += frac * bw
        ax.add_patch(mpatches.Rectangle(
            (bx, by), bw, bh,
            facecolor="none", edgecolor=C["BORDER"], linewidth=0.5, zorder=3))

    # Legend
    ly = -0.38
    for lbl, col, xi in [("RUN", C["RUN"], 0.01),
                          ("PREEMPT", C["PREEMPT"], 0.14),
                          ("SLEEP", C["SLEEP"], 0.30)]:
        ax.add_patch(mpatches.Rectangle((xi, ly), 0.02, 0.14,
                                         facecolor=col, edgecolor=C["BORDER"],
                                         linewidth=0.4, zorder=2))
        ax.text(xi + 0.025, ly + 0.07, lbl,
                fontsize=7, va="center", ha="left", color=C["HEADER"])
    ax.text(0.46, ly + 0.07,
            "red = warning       Measured red = drift > 5% from expected period",
            fontsize=6, color=C["WARN"], va="center", ha="left")


# ═══════════════════════════════════════════════════════════════════════════════
#  7.  MAIN CHART
# ═══════════════════════════════════════════════════════════════════════════════
def build_chart(df: pd.DataFrame, output: str, c_params: dict) -> None:

    states  = df[df["type"].isin(STATE_TYPES)].copy()
    markers = df[~df["type"].isin(STATE_TYPES)].copy()

    t0 = df["start"].min()
    for col in ("start", "end"):
        states[col]  -= t0
        markers[col] -= t0
    csv_span_s = float(df["end"].max() - df["start"].min())

    all_tasks = order_tasks(list(states["task"].unique()))
    n = len(all_tasks)
    if n == 0:
        print("[!] No state rows."); return

    task_to_y = {t: (n - 1 - i) * LANE_H for i, t in enumerate(all_tasks)}
    x_min  = float(states["start"].min())
    x_max  = float(states["end"].max())
    x_span = max(x_max - x_min, 1e-9)
    y_bot  = -LANE_H * 0.6
    y_top  = (n - 1) * LANE_H + LANE_H * 0.6

    period_map = {t: analyse_periods(t, markers, states, c_params) for t in all_tasks}
    stats_map  = {t: build_stats(t, states, markers, csv_span_s, period_map[t])
                  for t in all_tasks}

    # ── Figure ──────────────────────────────────────────────────────────────
    timeline_h = max(5, n * 0.85 + 2.5)
    table_h    = max(3, n * 0.70 + 2.5)
    fig_h      = timeline_h + table_h
    fig_w      = 30

    fig = plt.figure(figsize=(fig_w, fig_h), facecolor="white")
    gs  = gridspec.GridSpec(2, 1, figure=fig,
                            height_ratios=[timeline_h, table_h],
                            hspace=0.15)
    ax      = fig.add_subplot(gs[0])
    ax_stat = fig.add_subplot(gs[1])
    ax.set_facecolor("white")

    # ── Zebra stripes ─────────────────────────────────────────────────────
    for i, task in enumerate(all_tasks):
        yc = task_to_y[task]
        ax.axhspan(yc - LANE_H/2, yc + LANE_H/2,
                   facecolor=C["BG_EVEN"] if i % 2 == 0 else C["BG_ODD"],
                   alpha=0.60, zorder=0)

    # ── State blocks (NO boundary ticks) ──────────────────────────────────
    type_color = {t: C[t] for t in STATE_TYPES}
    for _, row in states.iterrows():
        task = row["task"]
        if task not in task_to_y:
            continue
        yc  = task_to_y[task]
        t_s = row["start"]; t_e = row["end"]
        col = type_color.get(row["type"], "#9E9E9E")
        ax.broken_barh([(t_s, t_e - t_s)], (yc - BAR_H/2, BAR_H),
                       facecolors=col, edgecolors="black",
                       linewidth=0.35, zorder=2)

    # ── Period / Deadline lines — IDEAL GRID from C-configured values ────
    #
    # From the C code:
    #   clock_gettime(CLOCK_MONOTONIC, &exec_release_time)  ← thread start
    #   while(1) {
    #     PERIOD_START marker
    #     time_add_millisecs(&exec_release_time, period)    ← next release
    #     ... compute + sleep ...
    #     PERIOD_END marker                                 ← actual wake-up
    #   }
    #
    # The ideal grid starts at the task's first state-block start time
    # (= when the thread actually began running) and repeats every
    # period_ms, regardless of any preemption drift in the CSV.
    #
    # Lines:
    #   Fine Periodo (black solid)  = t0_task + k * period_ms/1000
    #   Deadline (red dashed)       = t0_task + (k-1)*period_ms/1000 + deadline_ms/1000
    #                                 (only drawn if deadline != period)
    #
    use_ms = x_span < 0.5
    MIN_LBL_GAP = x_span * 0.008
    placed_p: dict = defaultdict(list)
    placed_d: dict = defaultdict(list)

    for task in all_tasks:
        if task not in task_to_y:
            continue
        yc = task_to_y[task]
        pi = period_map[task]
        period_ms   = pi["period_ms"]
        deadline_ms = pi["deadline_ms"]

        if period_ms is None:
            continue    # no period info — skip (system threads)

        period_s = period_ms / 1000.0

        # Task start = first state-block start for this task
        task_states = states[states["task"] == task]
        if task_states.empty:
            continue
        t0_task = float(task_states["start"].min())

        # Generate ideal grid from t0_task to x_max
        k = 1
        while True:
            fp_x = t0_task + k * period_s
            if fp_x > x_max + x_span * 0.01:
                break

            # ── Fine Periodo line (black solid) ───────────────────────
            ax.plot([fp_x, fp_x], [yc - LANE_H/2, yc + LANE_H/2],
                    color=C["PEND"], linewidth=1.8,
                    solid_capstyle="butt", zorder=5)
            # Time label below axis
            too_close = any(abs(fp_x - px) < MIN_LBL_GAP
                            for px in placed_p[task])
            if not too_close:
                lbl = f"{fp_x*1000:.0f}" if use_ms else f"{fp_x:.3f}"
                ax.text(fp_x, y_bot - 0.10, lbl,
                        fontsize=5.8, color=C["PEND"], fontweight="bold",
                        rotation=90, va="top", ha="center",
                        zorder=6, clip_on=False)
                placed_p[task].append(fp_x)

            # ── Deadline line (red dashed, only if deadline != period) ─
            if deadline_ms is not None and deadline_ms != period_ms:
                # Deadline for period k: starts at beginning of period k
                period_start_x = t0_task + (k - 1) * period_s
                dl_x = period_start_x + deadline_ms / 1000.0
                if dl_x <= x_max + x_span * 0.01:
                    ax.plot([dl_x, dl_x], [yc - LANE_H/2, yc + LANE_H/2],
                            color=C["WARN"], linewidth=1.2, linestyle="--",
                            solid_capstyle="butt", zorder=4, alpha=0.8)
                    too_close_d = any(abs(dl_x - px) < MIN_LBL_GAP
                                      for px in placed_d[task])
                    if not too_close_d:
                        lbl_d = f"{dl_x*1000:.0f}" if use_ms else f"{dl_x:.3f}"
                        ax.text(dl_x, y_bot - 0.10, lbl_d,
                                fontsize=5.5, color=C["WARN"], fontweight="bold",
                                rotation=90, va="top", ha="center",
                                zorder=6, clip_on=False)
                        placed_d[task].append(dl_x)

            k += 1

    # ── Axes ──────────────────────────────────────────────────────────────
    Y_LABEL_MARGIN = 0.60
    ax.set_yticks([task_to_y[t] for t in all_tasks])
    ax.set_yticklabels(all_tasks, fontsize=9)
    ax.set_ylim(y_bot - Y_LABEL_MARGIN, y_top)
    ax.set_xlim(x_min - x_span * 0.002, x_max + x_span * 0.002)

    if use_ms:
        ax.xaxis.set_major_formatter(
            mticker.FuncFormatter(lambda v, _: f"{v*1000:.1f}"))
        ax.set_xlabel("Tempo (ms)  —  t = 0 = inizio trace", fontsize=9)
    else:
        ax.xaxis.set_major_formatter(mticker.FormatStrFormatter("%.3f"))
        ax.set_xlabel("Tempo (s)  —  t = 0 = inizio trace", fontsize=9)

    ax.xaxis.set_major_locator(mticker.MaxNLocator(nbins=22, prune="both"))
    plt.setp(ax.get_xticklabels(), fontsize=8, rotation=30, ha="right")
    ax.set_title("Monitor Real-Time — Diagramma Temporale Scheduler",
                 fontsize=13, pad=12)
    ax.grid(axis="x", linestyle=":", linewidth=0.35, color="#CFD8DC", zorder=1)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    # ── Legend ──────────────────────────────────────────────────────────────
    handles = [
        mpatches.Patch(facecolor=C["RUN"],     edgecolor="black", lw=0.4, label="RUN"),
        mpatches.Patch(facecolor=C["PREEMPT"], edgecolor="black", lw=0.4, label="PREEMPT"),
        mpatches.Patch(facecolor=C["SLEEP"],   edgecolor="black", lw=0.4, label="SLEEP"),
        Line2D([0],[0], color=C["PEND"], lw=2.0, ls="-",  label="Fine Periodo"),
        Line2D([0],[0], color=C["WARN"], lw=1.2, ls="--", label="Deadline"),
        Line2D([0],[0], color=C["MISS"], lw=0, marker="v", markersize=7, label="Deadline Miss"),
    ]
    leg = ax.legend(handles=handles,
                    loc="upper center", bbox_to_anchor=(0.5, -0.08),
                    ncol=6, frameon=True, framealpha=0.95,
                    fontsize=9, borderpad=0.9, handlelength=2.2)
    for txt in leg.get_texts():
        txt.set_fontweight("bold")

    # ── Stats table ──────────────────────────────────────────────────────
    draw_stats_table(ax_stat, all_tasks, stats_map)

    # ── Save ─────────────────────────────────────────────────────────────
    plt.savefig(output, dpi=150, bbox_inches="tight", facecolor="white")
    print(f"[OK]  Salvato  →  {output}")
    plt.show()


# ═══════════════════════════════════════════════════════════════════════════════
#  8.  VALIDATION
# ═══════════════════════════════════════════════════════════════════════════════
def validate(df: pd.DataFrame) -> None:
    STATE = {"RUN", "PREEMPT", "SLEEP"}
    states = df[df["type"].isin(STATE)].sort_values("start")
    markers = df[~df["type"].isin(STATE)]

    print("\n── CSV Validation Report ─────────────────────────────────────────")
    print(f"   Rows:       {len(df)}")
    print(f"   Span:       {(df['end'].max() - df['start'].min()) * 1000:.1f} ms")
    print(f"   Tasks:      {sorted(states['task'].unique())}")
    print(f"   State cnt:  { {t: int((states['type']==t).sum()) for t in STATE} }")
    mt = sorted(set(re.sub(r'Activity_\d+.*', 'Activity_N',
                    re.sub(r'_\d+$', '', r))
                    for r in markers["type"].unique())) if len(markers) > 0 else []
    print(f"   Markers:    {mt}")
    issues = 0
    for task in sorted(states["task"].unique()):
        sub = states[states["task"] == task].sort_values("start")
        if sub.shape[0] > 1:
            gs = sub["start"].values[1:] - sub["end"].values[:-1]
            if (gs < -1e-6).any():
                print(f"   [WARN] {task}: overlapping state blocks"); issues += 1
            if ((sub["end"] - sub["start"]) < 0).any():
                print(f"   [WARN] {task}: negative durations"); issues += 1
    if issues == 0:
        print("   [OK] Data coherent.")
    print("──────────────────────────────────────────────────────────────────\n")


# ═══════════════════════════════════════════════════════════════════════════════
#  MAIN
# ═══════════════════════════════════════════════════════════════════════════════
if __name__ == "__main__":
    csv_path = sys.argv[1] if len(sys.argv) > 1 else "timeline.csv"
    out_path = sys.argv[2] if len(sys.argv) > 2 else "timeline_monitor.png"
    src_dir  = sys.argv[3] if len(sys.argv) > 3 else str(
        Path(csv_path).resolve().parent.parent)

    print(f"[..] CSV:     {csv_path}")
    print(f"[..] Source:  {src_dir}")
    df = load_csv(csv_path)

    c_params = parse_c_sources(src_dir, csv_path)
    if c_params:
        print(f"[..] C params: {c_params}")
    else:
        print("[..] C params: none found")

    validate(df)
    build_chart(df, out_path, c_params)
