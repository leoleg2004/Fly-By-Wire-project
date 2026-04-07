#!/usr/bin/env python3
"""
Real-Time Monitor — Logic Analyzer / Gantt Chart  v10.0
=======================================================
Reads timeline.csv  (columns: task, type, start, end, duration_ms)
which contains state rows (RUN, PREEMPT, SLEEP), marker rows (MARKER_*),
and pre-computed statistics rows (STAT_*) and miss log rows (MISS_LOG).

The C++ thread_analysis generates the CSV with all analysis already done.
This script is purely a visualisation frontend.

Layout:  TOP  = timeline (RUN / PREEMPT / SLEEP + period-end lines)
         BOT  = full-width statistics table

Usage
-----
  python monitorRealTime.py  [timeline.csv]  [output.png]
"""

import sys, re, os
from collections import defaultdict

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
#  1.  CSV LOADING  — separates state/marker rows from STAT_ and MISS_LOG
# ═══════════════════════════════════════════════════════════════════════════════
def load_csv(path: str):
    """
    Returns (df, stats_map, all_misses).
      df         — DataFrame with state+marker rows (same as old timeline.csv)
      stats_map  — {task: {stat_key: value, ...}}
      all_misses — list of miss-log dicts
    """
    rows = []
    stats_raw = {}   # task -> {key -> val}
    misses = []

    with open(path, "r") as fh:
        fh.readline()  # skip header
        for line in fh:
            line = line.strip()
            if not line:
                continue
            parts = line.split(",")
            if len(parts) < 5:
                continue

            task  = parts[0].strip()
            etype = parts[1].strip()

            if etype.startswith("STAT_"):
                stat_key = etype[5:].lower()   # e.g. "STAT_WCET" -> "wcet"
                val = float(parts[4])
                stats_raw.setdefault(task, {})[stat_key] = val

            elif etype == "MISS_LOG":
                # format: task,MISS_LOG,ps_abs,ps_rel,{dl_ms}|{resp_ms}|{overshoot_ms}|{period_idx}
                ps_abs = float(parts[2])
                ps_rel = float(parts[3])
                fields = parts[4].split("|")
                if len(fields) >= 4:
                    misses.append(dict(
                        task=task,
                        ps_abs=ps_abs, ps_rel=ps_rel,
                        deadline_ms=float(fields[0]),
                        response_ms=float(fields[1]),
                        overshoot_ms=float(fields[2]),
                        period_idx=int(fields[3]),
                    ))
            else:
                # Normal row: state or marker
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

    # Build stats_map with the same keys the old build_stats() produced
    stats_map = {}
    for task, raw in stats_raw.items():
        s = raw
        period_ms   = s.get("period", 0);    period_ms   = period_ms if period_ms > 0 else None
        measured_ms = s.get("measured", -1);  measured_ms = measured_ms if measured_ms > 0 else None
        deadline_ms = s.get("deadline", 0);   deadline_ms = deadline_ms if deadline_ms > 0 else None
        jitter_ms   = s.get("jitter", -1);    jitter_ms   = jitter_ms if jitter_ms > 0 else None
        ws          = s.get("wslack", -999999); worst_slack = ws if ws > -99999 else None

        run_tot = s.get("runtot", 0)
        pre_tot = s.get("pretot", 0)
        slp_tot = s.get("slptot", 0)

        stats_map[task] = dict(
            total_state = (run_tot + pre_tot + slp_tot) / 1000.0,  # seconds
            dur = {
                "RUN":     run_tot / 1000.0,
                "PREEMPT": pre_tot / 1000.0,
                "SLEEP":   slp_tot / 1000.0,
                "RESOURCE_WAIT": 0.0,
                "RESOURCE_LOCK": 0.0,
            },
            has_gaps    = s.get("hgaps", 0) > 0.5,
            n_gaps      = int(s.get("ngaps", 0)),
            max_gap_ms  = s.get("maxgap", 0),
            n_run       = int(s.get("nrun", 0)),
            wcet        = s.get("wcet", 0),
            acet        = s.get("acet", 0),
            n_preempt   = int(s.get("npre", 0)),
            preempt_tot = pre_tot,
            n_sleep     = int(s.get("nslp", 0)),
            period_ms   = period_ms,
            measured_ms = measured_ms,
            deadline_ms = deadline_ms,
            jitter_ms   = jitter_ms,
            n_periods   = 0,
            n_misses    = int(s.get("nmiss", 0)),
            utilization = s.get("util", 0),
            worst_slack = worst_slack,
        )

    return df, stats_map, misses


# ═══════════════════════════════════════════════════════════════════════════════
#  2.  TASK ORDERING
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
#  3.  STATS TABLE  (dedicated bottom panel)  — identical to v9.0
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

        s  = stats_map.get(task, {})
        ws = s.get("worst_slack")

        warn_wcet = s.get("wcet", 0) > (s.get("deadline_ms") if s.get("deadline_ms") else 1e12)
        warn_util = s.get("utilization", 0) > 0.90
        warn_pre  = s.get("n_preempt", 0) > 0
        warn_miss = s.get("n_misses", 0) > 0
        warn_ws   = ws is not None and ws < 0
        drift = (s.get("measured_ms") is not None and s.get("period_ms") is not None
                 and abs(s["measured_ms"] - s["period_ms"]) > s["period_ms"] * 0.05)

        vals = [
            task,
            fm(s.get("period_ms")),
            fm(s.get("measured_ms")),
            fm(s.get("deadline_ms")),
            fm(s.get("wcet")),
            fm(s.get("acet")),
            fm(s.get("jitter_ms")),
            fp(s.get("utilization")),
            fi(s.get("n_run", 0)),
            fi(s.get("n_preempt", 0)),
            fi(s.get("n_sleep", 0)),
            fi(s.get("n_misses", 0)),
            fm(ws, 1) if ws is not None else "—",
        ]
        clrs = [
            C["HEADER"],
            C["ACCENT"] if s.get("period_ms") else C["DIM"],
            C["WARN"] if drift else (C["TEXT"] if s.get("measured_ms") else C["DIM"]),
            C["ACCENT"] if s.get("deadline_ms") else C["DIM"],
            C["WARN"] if warn_wcet else C["TEXT"],
            C["TEXT"],
            C["DIM"],
            C["WARN"] if warn_util else C["OK"],
            C["TEXT"],
            C["WARN"] if warn_pre else C["TEXT"],
            C["TEXT"],
            C["MISS"] if warn_miss else C["OK"],
            C["MISS"] if warn_ws else (C["SLACK"] if ws is not None else C["DIM"]),
        ]
        bolds = [True, True, drift,
                 True, warn_wcet, False, False,
                 True, False, warn_pre, False,
                 warn_miss, warn_ws or (ws is not None)]

        for (lbl, x0), val, col, bold in zip(cols, vals, clrs, bolds):
            ax.text(x0, ry + 0.05, val,
                    fontsize=7.5 if lbl == "Task" else 8,
                    fontweight="bold" if bold else "normal",
                    color=col, va="center", ha="left", clip_on=True)

        # Stacked minibar
        bx, by, bw, bh = 0.005, ry - 0.38, 0.99, 0.20
        total_state = s.get("total_state", 0)
        denom = total_state if total_state > 1e-9 else 1.0
        cx = bx
        for tt in ("RUN", "PREEMPT", "SLEEP"):
            frac = s.get("dur", {}).get(tt, 0.0) / denom
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
#  4.  DEADLINE MISS LOG PANEL  — identical to v9.0
# ═══════════════════════════════════════════════════════════════════════════════
def _draw_miss_log(ax, all_misses: list, t0: float) -> None:
    """Draw a compact table of all deadline misses inside a dedicated axes."""
    ax.set_xlim(0, 1)
    n = len(all_misses)
    row_h = 1.0
    total_h = n + 1.5
    ax.set_ylim(-0.5, total_h)
    ax.set_yticks([])
    ax.xaxis.set_visible(False)
    for sp in ax.spines.values():
        sp.set_visible(False)
    ax.set_facecolor("#FFF8F8")

    ax.text(0.5, total_h - 0.1,
            f"Deadline Miss Log  —  {n} miss{'es' if n != 1 else ''}",
            fontsize=10, fontweight="bold", color=C["MISS"],
            va="center", ha="center", clip_on=False)

    cols = [
        ("#",              0.005),
        ("Task",           0.045),
        ("Period",         0.185),
        ("t_start (abs s)",0.255),
        ("t_start (rel ms)", 0.385),
        ("Deadline (ms)",  0.530),
        ("Response (ms)",  0.655),
        ("Overshoot (ms)", 0.790),
    ]

    header_y = total_h - 0.75
    ax.add_patch(mpatches.Rectangle(
        (0, header_y - 0.15), 1, 0.60,
        facecolor=C["WARN"], zorder=1, clip_on=False))
    for lbl, x0 in cols:
        ax.text(x0, header_y + 0.08, lbl,
                fontsize=7.5, fontweight="bold", color="white",
                va="center", ha="left", zorder=2, clip_on=False)

    for i, m in enumerate(all_misses):
        ry  = (total_h - 1.5) - i * row_h
        bg  = "#FFEBEE" if i % 2 == 0 else "#FFCDD2"
        ax.add_patch(mpatches.Rectangle(
            (0, ry - 0.45), 1, 0.90,
            facecolor=bg, edgecolor="#EF9A9A", linewidth=0.3, zorder=0))

        vals = [
            str(i + 1),
            m["task"],
            str(m["period_idx"]),
            f"{m['ps_abs']:.3f}",
            f"{m['ps_rel'] * 1000:.1f}",
            f"{m['deadline_ms']:.1f}",
            f"{m['response_ms']:.1f}",
            f"+{m['overshoot_ms']:.1f}",
        ]
        clrs = [C["MISS"], C["HEADER"], C["TEXT"], C["TEXT"],
                C["TEXT"], C["ACCENT"], C["WARN"], C["MISS"]]
        bolds = [True, True, False, False, False, False, True, True]

        for (lbl, x0), val, col, bold in zip(cols, vals, clrs, bolds):
            ax.text(x0, ry + 0.05, val,
                    fontsize=8, fontweight="bold" if bold else "normal",
                    color=col, va="center", ha="left", clip_on=True)


# ═══════════════════════════════════════════════════════════════════════════════
#  5.  MAIN CHART  — identical to v9.0  (reads stats_map instead of computing)
# ═══════════════════════════════════════════════════════════════════════════════
def build_chart(df: pd.DataFrame, output: str,
                stats_map: dict, all_misses: list) -> None:

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

    # ── Compute miss overlay positions (relative to t0) ──────────────────
    for m in all_misses:
        m["ps_rel_adj"] = m["ps_abs"] - t0   # relative to trace start
        dl_s = m["deadline_ms"] / 1000.0
        m["dl_x"]  = m["ps_rel_adj"] + dl_s
        m["fe_x"]  = m["ps_rel_adj"] + m["response_ms"] / 1000.0

    # ── Print deadline miss log to console ───────────────────────────────
    if all_misses:
        print("\n══ DEADLINE MISS LOG ═══════════════════════════════════════════════")
        print(f"  {'#':<4} {'Task':<14} {'Period':>6}  {'t_start(abs)':>14}  "
              f"{'Deadline':>10}  {'Response':>10}  {'Overshoot':>10}")
        print("  " + "─" * 72)
        for i, m in enumerate(all_misses, 1):
            print(f"  {i:<4} {m['task']:<14} {m['period_idx']:>6}  "
                  f"{m['ps_abs']:>14.3f}s  "
                  f"{m['deadline_ms']:>9.1f}ms  "
                  f"{m['response_ms']:>9.1f}ms  "
                  f"+{m['overshoot_ms']:>8.1f}ms")
        print("══════════════════════════════════════════════════════════════════════\n")
    else:
        print("[OK] No deadline misses detected.\n")

    # ── Figure ──────────────────────────────────────────────────────────────
    timeline_h = max(5, n * 0.85 + 2.5)
    table_h    = max(3, n * 0.70 + 2.5)
    miss_log_h = max(1.8, len(all_misses) * 0.28 + 1.0) if all_misses else 0
    fig_h      = timeline_h + table_h + miss_log_h
    fig_w      = 30

    fig = plt.figure(figsize=(fig_w, fig_h), facecolor="white")
    if all_misses:
        gs = gridspec.GridSpec(3, 1, figure=fig,
                               height_ratios=[timeline_h, miss_log_h, table_h],
                               hspace=0.18)
        ax        = fig.add_subplot(gs[0])
        ax_miss   = fig.add_subplot(gs[1])
        ax_stat   = fig.add_subplot(gs[2])
    else:
        gs = gridspec.GridSpec(2, 1, figure=fig,
                               height_ratios=[timeline_h, table_h],
                               hspace=0.15)
        ax        = fig.add_subplot(gs[0])
        ax_miss   = None
        ax_stat   = fig.add_subplot(gs[1])
    ax.set_facecolor("white")

    # ── Zebra stripes ─────────────────────────────────────────────────────
    for i, task in enumerate(all_tasks):
        yc = task_to_y[task]
        ax.axhspan(yc - LANE_H/2, yc + LANE_H/2,
                   facecolor=C["BG_EVEN"] if i % 2 == 0 else C["BG_ODD"],
                   alpha=0.60, zorder=0)

    # ── State blocks ──────────────────────────────────────────────────────
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

    # ── Period / Deadline lines — IDEAL GRID from stats ──────────────────
    use_ms = x_span < 0.5
    MIN_LBL_GAP = x_span * 0.008
    placed_p: dict = defaultdict(list)
    placed_d: dict = defaultdict(list)

    for task in all_tasks:
        if task not in task_to_y:
            continue
        yc = task_to_y[task]
        s = stats_map.get(task, {})
        period_ms   = s.get("period_ms")
        deadline_ms = s.get("deadline_ms")

        if period_ms is None:
            continue

        period_s = period_ms / 1000.0

        task_states = states[states["task"] == task]
        if task_states.empty:
            continue
        t0_task = float(task_states["start"].min())

        k = 1
        while True:
            fp_x = t0_task + k * period_s
            if fp_x > x_max + x_span * 0.01:
                break

            # ── Fine Periodo line (black solid) ───────────────────────
            ax.plot([fp_x, fp_x], [yc - LANE_H/2, yc + LANE_H/2],
                    color=C["PEND"], linewidth=1.8,
                    solid_capstyle="butt", zorder=5)

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

    # ── Deadline Miss markers on the Gantt ───────────────────────────────
    task_miss_counter: dict = defaultdict(int)
    for m in all_misses:
        task = m["task"]
        if task not in task_to_y:
            continue
        yc      = task_to_y[task]
        dl_x    = m["dl_x"]
        fe_x    = m["fe_x"]
        task_miss_counter[task] += 1
        cnt = task_miss_counter[task]

        ax.plot(dl_x, yc + BAR_H / 2 + 0.05,
                marker="v", color=C["MISS"], markersize=10,
                markeredgecolor="white", markeredgewidth=0.6,
                zorder=9, clip_on=False)

        if fe_x > dl_x:
            ax.hlines(yc, dl_x, fe_x,
                      colors=C["MISS"], linewidth=4.5,
                      zorder=8, alpha=0.75)
            mid_x = (dl_x + fe_x) / 2.0
            ov_ms = m["overshoot_ms"]
            ax.text(mid_x, yc + BAR_H / 2 + 0.25,
                    f"+{ov_ms:.0f}ms",
                    fontsize=6.5, color=C["MISS"], fontweight="bold",
                    va="bottom", ha="center", zorder=10, clip_on=False)

        ax.text(dl_x, yc + BAR_H / 2 + 0.42,
                f"!{cnt}",
                fontsize=6, color=C["MISS"], fontweight="bold",
                va="bottom", ha="center", zorder=10, clip_on=False)

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

    # ── Legend ────────────────────────────────────────────────────────────
    handles = [
        mpatches.Patch(facecolor=C["RUN"],     edgecolor="black", lw=0.4, label="RUN"),
        mpatches.Patch(facecolor=C["PREEMPT"], edgecolor="black", lw=0.4, label="PREEMPT"),
        mpatches.Patch(facecolor=C["SLEEP"],   edgecolor="black", lw=0.4, label="SLEEP"),
        Line2D([0],[0], color=C["PEND"], lw=2.0, ls="-",  label="Fine Periodo"),
        Line2D([0],[0], color=C["WARN"], lw=1.2, ls="--", label="Deadline"),
        Line2D([0],[0], color=C["MISS"], lw=0, marker="v", markersize=7, label="Deadline Miss"),
    ]
    leg = ax.legend(handles=handles,
                    loc="lower center", bbox_to_anchor=(0.5, 1.01),
                    ncol=6, frameon=True, framealpha=0.95,
                    fontsize=9, borderpad=0.9, handlelength=2.2)
    for txt in leg.get_texts():
        txt.set_fontweight("bold")

    # ── Deadline Miss Log panel ──────────────────────────────────────────
    if ax_miss is not None:
        _draw_miss_log(ax_miss, all_misses, t0)

    # ── Stats table ──────────────────────────────────────────────────────
    draw_stats_table(ax_stat, all_tasks, stats_map)

    # ── Save & Show ──────────────────────────────────────────────────────
    plt.savefig(output, dpi=150, bbox_inches="tight", facecolor="white")
    print(f"[OK]  Salvato  →  {output}")
    plt.show()


# ═══════════════════════════════════════════════════════════════════════════════
#  6.  VALIDATION
# ═══════════════════════════════════════════════════════════════════════════════
def validate(df: pd.DataFrame) -> None:
    STATE = {"RUN", "PREEMPT", "SLEEP"}
    states = df[df["type"].isin(STATE)].sort_values("start")

    print("\n── CSV Validation Report ─────────────────────────────────────────")
    print(f"   Rows:       {len(df)}")
    print(f"   Span:       {(df['end'].max() - df['start'].min()) * 1000:.1f} ms")
    print(f"   Tasks:      {sorted(states['task'].unique())}")
    print(f"   State cnt:  { {t: int((states['type']==t).sum()) for t in STATE} }")
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

    if not os.path.exists(csv_path):
        print(f"[!] Errore: file {csv_path} non trovato.")
        sys.exit(1)

    print(f"[..] CSV:     {csv_path}")

    df, stats_map, all_misses = load_csv(csv_path)

    if stats_map:
        print(f"[..] Stats trovate per: {list(stats_map.keys())}")
    else:
        print("[..] Nessuna statistica STAT_* trovata nel CSV.")

    validate(df)
    build_chart(df, out_path, stats_map, all_misses)
