#!/usr/bin/env python3
"""
Position-velocity joint MPC split demo for single-axis flying laser processing.

Input:
    A simple NC/G-code file with G0/G1 XY moves and optional F feed rates.
    M3/M4 or S>0 turns the laser command on; M5 or S0 turns it off.

Output:
    Fixed-cycle target, axis, galvo, reconstruction error, constraint status,
    laser command, and final laser-on decision.
"""

from __future__ import annotations

import argparse
import csv
import math
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    import numpy as np
except ImportError as exc:
    raise SystemExit(
        "Missing dependency: numpy\n"
        "Install project dependencies with: python -m pip install -r requirements.txt"
    ) from exc

try:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError as exc:
    raise SystemExit(
        "Missing dependency: matplotlib\n"
        "Install project dependencies with: python -m pip install -r requirements.txt"
    ) from exc


WORD_RE = re.compile(r"([A-Z])\s*([-+]?(?:\d+(?:\.\d*)?|\.\d+))", re.IGNORECASE)


@dataclass
class Config:
    Ts: float = 0.001
    Np: int = 60

    default_feed: float = 65.0
    field_half: float = 11.0

    axis_vmax: float = 110.0
    axis_amax: float = 1800.0
    galvo_vmax: float = 4500.0

    # Joint MPC weights
    w_galvo_pos: float = 1.0
    w_galvo_vel: float = 0.0012
    w_axis_vel: float = 0.0006
    w_axis_accel: float = 1.5e-5
    w_axis_jerk: float = 1.0e-4
    w_terminal_pos: float = 0.7
    w_terminal_vel: float = 0.006
    reg: float = 1e-9


@dataclass
class LinearMove:
    start_x: float
    start_y: float
    end_x: float
    end_y: float
    feed: float
    laser_command: int
    rapid: bool


def strip_gcode_comments(line: str) -> str:
    line = re.sub(r"\([^)]*\)", "", line)
    return line.split(";", 1)[0].strip()


def parse_words(line: str) -> Dict[str, float]:
    words: Dict[str, float] = {}
    for letter, value in WORD_RE.findall(line):
        words[letter.upper()] = float(value)
    return words


def parse_nc_file(path: Path, cfg: Config) -> List[LinearMove]:
    x = 0.0
    y = 0.0
    feed = cfg.default_feed
    motion_mode: Optional[int] = None
    laser_command = 0
    moves: List[LinearMove] = []

    for raw_line in path.read_text(encoding="utf-8-sig").splitlines():
        line = strip_gcode_comments(raw_line).upper()
        if not line:
            continue

        words = parse_words(line)
        if "M" in words:
            m_code = int(words["M"])
            if m_code in (3, 4):
                laser_command = 1
            elif m_code == 5:
                laser_command = 0

        if "S" in words:
            laser_command = int(words["S"] > 0.0)

        if "F" in words and words["F"] > 0.0:
            # NC feed is normally mm/min; convert to mm/s for this demo.
            feed = words["F"] / 60.0

        if "G" in words:
            g_code = int(words["G"])
            if g_code in (0, 1):
                motion_mode = g_code

        has_xy = "X" in words or "Y" in words
        if motion_mode in (0, 1) and has_xy:
            next_x = words.get("X", x)
            next_y = words.get("Y", y)
            rapid = motion_mode == 0
            move_feed = cfg.axis_vmax if rapid else feed
            move_laser = 0 if rapid else laser_command
            if math.hypot(next_x - x, next_y - y) > 0.0:
                moves.append(LinearMove(x, y, next_x, next_y, move_feed, move_laser, rapid))
            x = next_x
            y = next_y

    if not moves:
        raise ValueError(f"No G0/G1 XY moves found in NC file: {path}")
    return moves


def interpolate_moves(moves: List[LinearMove], cfg: Config) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    t_values: List[float] = []
    x_values: List[float] = []
    y_values: List[float] = []
    vx_values: List[float] = []
    vy_values: List[float] = []
    laser_values: List[int] = []

    current_t = 0.0
    for move in moves:
        dx = move.end_x - move.start_x
        dy = move.end_y - move.start_y
        length = math.hypot(dx, dy)
        if length <= 0.0:
            continue

        feed = max(move.feed, 1e-9)
        duration = length / feed
        steps = max(1, int(math.ceil(duration / cfg.Ts)))
        vx = dx / duration
        vy = dy / duration

        for i in range(steps):
            alpha = i / steps
            t_values.append(current_t + i * cfg.Ts)
            x_values.append(move.start_x + alpha * dx)
            y_values.append(move.start_y + alpha * dy)
            vx_values.append(vx)
            vy_values.append(vy)
            laser_values.append(move.laser_command)

        current_t += steps * cfg.Ts

    last = moves[-1]
    t_values.append(current_t)
    x_values.append(last.end_x)
    y_values.append(last.end_y)
    vx_values.append(0.0)
    vy_values.append(0.0)
    laser_values.append(0)

    return (
        np.asarray(t_values, dtype=float),
        np.asarray(x_values, dtype=float),
        np.asarray(y_values, dtype=float),
        np.asarray(vx_values, dtype=float),
        np.asarray(vy_values, dtype=float),
        np.asarray(laser_values, dtype=int),
    )


def load_nc_target(cfg: Config, nc_path: Path) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    moves = parse_nc_file(nc_path, cfg)
    return interpolate_moves(moves, cfg)


def prediction_matrices(Np: int, Ts: float) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    Mx = np.zeros((Np, Np))
    Mv = np.zeros((Np, Np))
    for i in range(1, Np + 1):
        for j in range(i):
            Mx[i - 1, j] = Ts * Ts * (i - j - 0.5)
            Mv[i - 1, j] = Ts

    D = np.zeros((Np - 1, Np))
    for i in range(Np - 1):
        D[i, i] = -1.0
        D[i, i + 1] = 1.0
    return Mx, Mv, D


def solve_joint_mpc(
    cfg: Config,
    ref_x: np.ndarray,
    ref_vx: np.ndarray,
    x_now: float,
    v_now: float,
    Mx: np.ndarray,
    Mv: np.ndarray,
    D: np.ndarray,
) -> np.ndarray:
    Np = cfg.Np
    idx = np.arange(1, Np + 1)
    base_x = x_now + idx * cfg.Ts * v_now
    base_v = np.full(Np, v_now)

    H = (
        cfg.w_galvo_pos * (Mx.T @ Mx)
        + cfg.w_galvo_vel * (Mv.T @ Mv)
        + cfg.w_axis_vel * (Mv.T @ Mv)
        + cfg.w_axis_accel * np.eye(Np)
        + cfg.w_axis_jerk * (D.T @ D)
        + cfg.w_terminal_pos * np.outer(Mx[-1], Mx[-1])
        + cfg.w_terminal_vel * np.outer(Mv[-1], Mv[-1])
        + cfg.reg * np.eye(Np)
    )

    b = (
        cfg.w_galvo_pos * (Mx.T @ (ref_x - base_x))
        + cfg.w_galvo_vel * (Mv.T @ (ref_vx - base_v))
        - cfg.w_axis_vel * (Mv.T @ base_v)
        + cfg.w_terminal_pos * Mx[-1] * (ref_x[-1] - base_x[-1])
        + cfg.w_terminal_vel * Mv[-1] * (ref_vx[-1] - base_v[-1])
    )

    return np.linalg.solve(H, b)


def simulate(cfg: Config, nc_path: Path) -> Dict[str, np.ndarray]:
    t, target_x, target_y, target_vx, target_vy, laser_command = load_nc_target(cfg, nc_path)
    K = len(t)
    Mx, Mv, D = prediction_matrices(cfg.Np, cfg.Ts)

    axis_x = np.zeros(K)
    axis_y = np.zeros(K)
    axis_vx = np.zeros(K)
    axis_vy = np.zeros(K)
    axis_a = np.zeros(K)

    galvo_x = np.zeros(K)
    galvo_y = np.zeros(K)
    galvo_vx = np.zeros(K)
    galvo_vy = np.zeros(K)
    galvo_speed = np.zeros(K)

    synth_x = np.zeros(K)
    synth_y = np.zeros(K)
    synth_vx = np.zeros(K)
    synth_vy = np.zeros(K)

    error_x = np.zeros(K)
    error_y = np.zeros(K)
    error_vx = np.zeros(K)
    error_vy = np.zeros(K)

    galvo_in_square = np.zeros(K, dtype=int)
    galvo_speed_ok = np.zeros(K, dtype=int)
    axis_ok = np.zeros(K, dtype=int)
    laser_on = np.zeros(K, dtype=int)

    x_now = target_x[0]
    v_now = target_vx[0]

    for k in range(K):
        future_idx = np.minimum(np.arange(k + 1, k + cfg.Np + 1), K - 1)
        ref_x = target_x[future_idx]
        ref_vx = target_vx[future_idx]

        U = solve_joint_mpc(cfg, ref_x, ref_vx, x_now, v_now, Mx, Mv, D)
        a_raw = float(U[0])
        a_cmd = max(-cfg.axis_amax, min(cfg.axis_amax, a_raw))
        limited = abs(a_cmd - a_raw) > 1e-10

        next_v = v_now + a_cmd * cfg.Ts
        if next_v > cfg.axis_vmax:
            next_v = cfg.axis_vmax
            a_cmd = (next_v - v_now) / cfg.Ts
            limited = True
        elif next_v < -cfg.axis_vmax:
            next_v = -cfg.axis_vmax
            a_cmd = (next_v - v_now) / cfg.Ts
            limited = True

        axis_x[k] = x_now
        axis_y[k] = 0.0
        axis_vx[k] = v_now
        axis_vy[k] = 0.0
        axis_a[k] = a_cmd

        galvo_x[k] = target_x[k] - axis_x[k]
        galvo_y[k] = target_y[k] - axis_y[k]
        galvo_vx[k] = target_vx[k] - axis_vx[k]
        galvo_vy[k] = target_vy[k] - axis_vy[k]
        galvo_speed[k] = math.hypot(galvo_vx[k], galvo_vy[k])

        synth_x[k] = axis_x[k] + galvo_x[k]
        synth_y[k] = axis_y[k] + galvo_y[k]
        synth_vx[k] = axis_vx[k] + galvo_vx[k]
        synth_vy[k] = axis_vy[k] + galvo_vy[k]

        error_x[k] = target_x[k] - synth_x[k]
        error_y[k] = target_y[k] - synth_y[k]
        error_vx[k] = target_vx[k] - synth_vx[k]
        error_vy[k] = target_vy[k] - synth_vy[k]

        in_square = abs(galvo_x[k]) <= cfg.field_half and abs(galvo_y[k]) <= cfg.field_half
        speed_ok = galvo_speed[k] <= cfg.galvo_vmax
        ax_ok = abs(axis_vx[k]) <= cfg.axis_vmax + 1e-9 and abs(axis_a[k]) <= cfg.axis_amax + 1e-9 and not limited

        galvo_in_square[k] = int(in_square)
        galvo_speed_ok[k] = int(speed_ok)
        axis_ok[k] = int(ax_ok)
        laser_on[k] = int(bool(laser_command[k]) and in_square and speed_ok and ax_ok)

        x_now = x_now + v_now * cfg.Ts + 0.5 * a_cmd * cfg.Ts * cfg.Ts
        v_now = next_v

    return {
        "k": np.arange(K),
        "t": t,
        "target_x": target_x,
        "target_y": target_y,
        "target_vx": target_vx,
        "target_vy": target_vy,
        "target_speed": np.sqrt(target_vx**2 + target_vy**2),
        "axis_x": axis_x,
        "axis_y": axis_y,
        "axis_vx": axis_vx,
        "axis_vy": axis_vy,
        "axis_a": axis_a,
        "galvo_x": galvo_x,
        "galvo_y": galvo_y,
        "galvo_vx": galvo_vx,
        "galvo_vy": galvo_vy,
        "galvo_speed": galvo_speed,
        "synth_x": synth_x,
        "synth_y": synth_y,
        "synth_vx": synth_vx,
        "synth_vy": synth_vy,
        "error_x": error_x,
        "error_y": error_y,
        "error_vx": error_vx,
        "error_vy": error_vy,
        "galvo_in_square": galvo_in_square,
        "galvo_speed_ok": galvo_speed_ok,
        "axis_ok": axis_ok,
        "laser_command": laser_command,
        "laser_on": laser_on,
    }


def save_csv(path: Path, data: Dict[str, np.ndarray]) -> None:
    cols = [
        "k",
        "t",
        "target_x",
        "target_y",
        "target_vx",
        "target_vy",
        "target_speed",
        "axis_x",
        "axis_y",
        "axis_vx",
        "axis_vy",
        "axis_a",
        "galvo_x",
        "galvo_y",
        "galvo_vx",
        "galvo_vy",
        "galvo_speed",
        "synth_x",
        "synth_y",
        "synth_vx",
        "synth_vy",
        "error_x",
        "error_y",
        "error_vx",
        "error_vy",
        "galvo_in_square",
        "galvo_speed_ok",
        "axis_ok",
        "laser_command",
        "laser_on",
    ]
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(cols)
        for i in range(len(data["k"])):
            writer.writerow([data[c][i] for c in cols])


def save_plots(cfg: Config, data: Dict[str, np.ndarray], out_dir: Path) -> None:
    t = data["t"]
    h = cfg.field_half

    plt.figure(figsize=(11, 4.2))
    plt.plot(t, data["target_x"], label="target_x[k]")
    plt.plot(t, data["axis_x"], label="axis_x[k]")
    plt.plot(t, data["galvo_x"], label="galvo_x[k]")
    plt.xlabel("Time / s")
    plt.ylabel("Position / mm")
    plt.title("Position split: target_x[k] = axis_x[k] + galvo_x[k]")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "pv_fig_position_split.png", dpi=150)
    plt.close()

    plt.figure(figsize=(11, 4.2))
    plt.plot(t, data["target_vx"], label="target_vx[k]")
    plt.plot(t, data["axis_vx"], label="axis_vx[k]")
    plt.plot(t, data["galvo_vx"], label="galvo_vx[k]")
    plt.xlabel("Time / s")
    plt.ylabel("Velocity / mm/s")
    plt.title("Velocity split: target_vx[k] = axis_vx[k] + galvo_vx[k]")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "pv_fig_velocity_split.png", dpi=150)
    plt.close()

    plt.figure(figsize=(6.4, 6.4))
    square_x = [-h, h, h, -h, -h]
    square_y = [-h, -h, h, h, -h]
    plt.plot(square_x, square_y, label="square galvo field")
    plt.plot(data["galvo_x"], data["galvo_y"], label="galvo XY path")
    plt.xlabel("galvo_x / mm")
    plt.ylabel("galvo_y / mm")
    plt.title("Galvo XY path inside square field")
    plt.axis("equal")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "pv_fig_galvo_square_field.png", dpi=150)
    plt.close()

    plt.figure(figsize=(10.5, 6.2))
    axis_x = data["axis_x"]
    axis_y = data["axis_y"]
    target_x = data["target_x"]
    target_y = data["target_y"]
    field_min_x = float(np.min(axis_x) - h)
    field_max_x = float(np.max(axis_x) + h)
    field_min_y = float(np.min(axis_y) - h)
    field_max_y = float(np.max(axis_y) + h)
    plt.fill(
        [field_min_x, field_max_x, field_max_x, field_min_x],
        [field_min_y, field_min_y, field_max_y, field_max_y],
        color="#dbeafe",
        alpha=0.35,
        label="axis swept galvo field",
    )
    sample_count = min(18, len(axis_x))
    sample_idx = np.unique(np.linspace(0, len(axis_x) - 1, sample_count, dtype=int))
    for i, idx in enumerate(sample_idx):
        cx = float(axis_x[idx])
        cy = float(axis_y[idx])
        field_x = [cx - h, cx + h, cx + h, cx - h, cx - h]
        field_y = [cy - h, cy - h, cy + h, cy + h, cy - h]
        plt.plot(
            field_x,
            field_y,
            color="#2563eb",
            linewidth=0.8,
            alpha=0.25,
            label="galvo field samples" if i == 0 else None,
        )
    plt.plot(target_x, target_y, color="#111827", linewidth=1.8, label="NC target path")
    plt.plot(axis_x, axis_y, color="#dc2626", linewidth=1.3, label="axis center path")
    plt.scatter(axis_x[0], axis_y[0], color="#16a34a", s=30, zorder=5, label="start")
    plt.scatter(axis_x[-1], axis_y[-1], color="#7c3aed", s=30, zorder=5, label="end")
    plt.xlabel("Machine X / mm")
    plt.ylabel("Machine Y / mm")
    plt.title("Overlay of Galvo Field and Axis Field")
    plt.axis("equal")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "pv_fig_field_overlay.png", dpi=150)
    plt.close()

    plt.figure(figsize=(11, 4.2))
    plt.plot(t, data["galvo_vx"], label="galvo_vx[k]")
    plt.plot(t, data["galvo_vy"], label="galvo_vy[k]")
    plt.plot(t, data["galvo_speed"], label="|galvo_v[k]|")
    plt.axhline(cfg.galvo_vmax, linestyle="--", label="galvo_vmax")
    plt.axhline(-cfg.galvo_vmax, linestyle="--", label="-galvo_vmax")
    plt.xlabel("Time / s")
    plt.ylabel("Velocity / mm/s")
    plt.title("Galvo velocity from velocity split")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "pv_fig_galvo_velocity.png", dpi=150)
    plt.close()

    plt.figure(figsize=(11, 4.2))
    plt.plot(t, data["error_x"], label="position error_x")
    plt.plot(t, data["error_y"], label="position error_y")
    plt.plot(t, data["error_vx"], label="velocity error_vx")
    plt.plot(t, data["error_vy"], label="velocity error_vy")
    plt.xlabel("Time / s")
    plt.ylabel("Error")
    plt.title("Per-cycle reconstruction errors")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "pv_fig_reconstruction_error.png", dpi=150)
    plt.close()

    plt.figure(figsize=(11, 4.2))
    plt.plot(t, data["axis_vx"], label="axis_vx[k]")
    plt.plot(t, data["axis_a"] / 20.0, label="axis_a[k] / 20")
    plt.axhline(cfg.axis_vmax, linestyle="--", label="+axis_vmax")
    plt.axhline(-cfg.axis_vmax, linestyle="--", label="-axis_vmax")
    plt.xlabel("Time / s")
    plt.ylabel("Velocity / mm/s, scaled acceleration")
    plt.title("Long-axis velocity and acceleration")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "pv_fig_axis_limits.png", dpi=150)
    plt.close()

    plt.figure(figsize=(11, 3.2))
    plt.step(t, data["laser_command"], where="post", label="laser_command")
    plt.step(t, data["laser_on"], where="post", label="laser_on")
    plt.ylim(-0.1, 1.1)
    plt.xlabel("Time / s")
    plt.ylabel("Laser state")
    plt.title("Laser command and final laser-on output")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "pv_fig_laser_output.png", dpi=150)
    plt.close()


def make_report(cfg: Config, data: Dict[str, np.ndarray], out_dir: Path, nc_path: Path) -> None:
    K = len(data["k"])
    max_pos_err = max(float(np.max(np.abs(data["error_x"]))), float(np.max(np.abs(data["error_y"]))))
    max_vel_err = max(float(np.max(np.abs(data["error_vx"]))), float(np.max(np.abs(data["error_vy"]))))
    max_gx = float(np.max(np.abs(data["galvo_x"])))
    max_gy = float(np.max(np.abs(data["galvo_y"])))
    max_gvx = float(np.max(np.abs(data["galvo_vx"])))
    max_gvy = float(np.max(np.abs(data["galvo_vy"])))
    max_gspeed = float(np.max(data["galvo_speed"]))
    max_axis_v = float(np.max(np.abs(data["axis_vx"])))
    max_axis_a = float(np.max(np.abs(data["axis_a"])))
    max_target_speed = float(np.max(data["target_speed"]))
    field_fail = int(K - np.sum(data["galvo_in_square"]))
    speed_fail = int(K - np.sum(data["galvo_speed_ok"]))
    axis_fail = int(K - np.sum(data["axis_ok"]))
    laser_command_cycles = int(np.sum(data["laser_command"]))
    laser_on_cycles = int(np.sum(data["laser_on"]))

    html = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Position-Velocity Joint MPC Split Demo</title>
<style>
body {{ font-family: Arial, sans-serif; margin: 24px; color: #202124; }}
h1 {{ font-size: 24px; }}
h2 {{ font-size: 18px; margin-top: 28px; }}
code {{ background: #eef2f7; padding: 2px 5px; border-radius: 4px; }}
.grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(230px, 1fr)); gap: 10px; }}
.card {{ border: 1px solid #d0d7de; border-radius: 8px; padding: 12px; background: #f8fafc; }}
.ok {{ color: #137333; font-weight: 700; }}
.bad {{ color: #b42318; font-weight: 700; }}
img {{ width: 100%; max-width: 1040px; border: 1px solid #d0d7de; border-radius: 6px; background: white; }}
</style>
</head>
<body>
<h1>Position-Velocity Joint MPC Split Demo</h1>
<p>This demo reads an NC/G-code file, interpolates fixed-cycle target position and velocity,
then splits the X motion between the long axis and the galvo. The final laser output is
enabled only when the NC laser command is on and all motion constraints are satisfied.</p>
<h2>Input</h2>
<div class="grid">
<div class="card">NC file: <code>{nc_path.name}</code></div>
<div class="card">Cycle time Ts: <code>{cfg.Ts*1e6:.0f} us</code></div>
<div class="card">Samples K: <code>{K}</code></div>
<div class="card">MPC horizon: <code>{cfg.Np}</code> samples, <code>{cfg.Np*cfg.Ts*1000:.1f} ms</code></div>
</div>
<h2>Split Equations</h2>
<div class="grid">
<div class="card"><code>target_x[k] = axis_x[k] + galvo_x[k]</code></div>
<div class="card"><code>target_y[k] = galvo_y[k]</code></div>
<div class="card"><code>target_vx[k] = axis_vx[k] + galvo_vx[k]</code></div>
<div class="card"><code>target_vy[k] = galvo_vy[k]</code></div>
</div>
<h2>Limits</h2>
<div class="grid">
<div class="card">Max target speed: <code>{max_target_speed:.1f} mm/s</code></div>
<div class="card">Axis velocity / acceleration limit: <code>{cfg.axis_vmax:.1f} mm/s</code> / <code>{cfg.axis_amax:.1f} mm/s^2</code></div>
<div class="card">Galvo square field: <code>+/-{cfg.field_half:.1f} mm</code></div>
<div class="card">Galvo velocity limit: <code>{cfg.galvo_vmax:.1f} mm/s</code></div>
</div>
<h2>Run Summary</h2>
<div class="grid">
<div class="card">Max position reconstruction error: <code>{max_pos_err:.3e}</code></div>
<div class="card">Max velocity reconstruction error: <code>{max_vel_err:.3e}</code></div>
<div class="card">Max |galvo_x|: <code>{max_gx:.3f} mm</code></div>
<div class="card">Max |galvo_y|: <code>{max_gy:.3f} mm</code></div>
<div class="card">Max |galvo_vx|: <code>{max_gvx:.1f} mm/s</code></div>
<div class="card">Max |galvo_vy|: <code>{max_gvy:.1f} mm/s</code></div>
<div class="card">Max |galvo_v|: <code>{max_gspeed:.1f} mm/s</code></div>
<div class="card">Max |axis_v|: <code>{max_axis_v:.3f} mm/s</code></div>
<div class="card">Max |axis_a|: <code>{max_axis_a:.3f} mm/s^2</code></div>
<div class="card">Galvo field fail cycles: <span class="{'bad' if field_fail else 'ok'}">{field_fail}</span></div>
<div class="card">Galvo speed fail cycles: <span class="{'bad' if speed_fail else 'ok'}">{speed_fail}</span></div>
<div class="card">Axis fail cycles: <span class="{'bad' if axis_fail else 'ok'}">{axis_fail}</span></div>
<div class="card">Laser command cycles: <code>{laser_command_cycles}</code></div>
<div class="card">Final laser-on cycles: <code>{laser_on_cycles}</code></div>
</div>
<h2>1. Position Split</h2><img src="pv_fig_position_split.png" alt="Position split">
<h2>2. Velocity Split</h2><img src="pv_fig_velocity_split.png" alt="Velocity split">
<h2>3. Galvo XY Field</h2><img src="pv_fig_galvo_square_field.png" alt="Galvo XY field">
<h2>4. Field Overlay</h2><img src="pv_fig_field_overlay.png" alt="Galvo field and axis field overlay">
<h2>5. Galvo Velocity</h2><img src="pv_fig_galvo_velocity.png" alt="Galvo velocity">
<h2>6. Reconstruction Error</h2><img src="pv_fig_reconstruction_error.png" alt="Reconstruction error">
<h2>7. Axis Limits</h2><img src="pv_fig_axis_limits.png" alt="Axis limits">
<h2>8. Laser Output</h2><img src="pv_fig_laser_output.png" alt="Laser output">
</body>
</html>"""
    (out_dir / "position_velocity_joint_mpc_split_report.html").write_text(html, encoding="utf-8")


def default_nc_path() -> Path:
    root = Path(__file__).resolve().parents[1]
    for candidate in root.glob("*/position_velocity_joint_mpc_input.nc"):
        return candidate
    return root / "source" / "position_velocity_joint_mpc_input.nc"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run position-velocity joint MPC split from an NC/G-code file.")
    parser.add_argument(
        "nc_file",
        nargs="?",
        type=Path,
        default=default_nc_path(),
        help="Input NC/G-code file. Default: sibling input directory/position_velocity_joint_mpc_input.nc",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path(__file__).resolve().parent,
        help="Output directory for CSV, plots, and report.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    cfg = Config()
    nc_path = args.nc_file.resolve()
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    data = simulate(cfg, nc_path)
    save_csv(out_dir / "position_velocity_joint_mpc_split_samples.csv", data)
    save_plots(cfg, data, out_dir)
    make_report(cfg, data, out_dir, nc_path)

    K = len(data["k"])
    max_pos_err = max(np.max(np.abs(data["error_x"])), np.max(np.abs(data["error_y"])))
    max_vel_err = max(np.max(np.abs(data["error_vx"])), np.max(np.abs(data["error_vy"])))
    print("Position-velocity joint MPC split demo finished.")
    print(f"input NC file = {nc_path}")
    print(f"K = {K}")
    print(f"same length: target={len(data['target_x'])}, axis={len(data['axis_x'])}, galvo={len(data['galvo_x'])}")
    print(f"max position reconstruction error = {max_pos_err:.3e}")
    print(f"max velocity reconstruction error = {max_vel_err:.3e}")
    print(f"max |galvo_x| = {np.max(np.abs(data['galvo_x'])):.3f} mm")
    print(f"max |galvo_y| = {np.max(np.abs(data['galvo_y'])):.3f} mm")
    print(f"max |galvo_vx| = {np.max(np.abs(data['galvo_vx'])):.3f} mm/s")
    print(f"max |galvo_vy| = {np.max(np.abs(data['galvo_vy'])):.3f} mm/s")
    print(f"max |galvo_v| = {np.max(data['galvo_speed']):.3f} mm/s")
    print(f"max |axis_v| = {np.max(np.abs(data['axis_vx'])):.3f} mm/s")
    print(f"max |axis_a| = {np.max(np.abs(data['axis_a'])):.3f} mm/s^2")
    print(f"galvo field fail cycles = {K - int(np.sum(data['galvo_in_square']))}")
    print(f"galvo speed fail cycles = {K - int(np.sum(data['galvo_speed_ok']))}")
    print(f"axis fail cycles = {K - int(np.sum(data['axis_ok']))}")
    print(f"laser command cycles = {int(np.sum(data['laser_command']))}")
    print(f"final laser-on cycles = {int(np.sum(data['laser_on']))}")
    print("wrote position_velocity_joint_mpc_split_samples.csv")
    print("wrote position_velocity_joint_mpc_split_report.html")


if __name__ == "__main__":
    main()
