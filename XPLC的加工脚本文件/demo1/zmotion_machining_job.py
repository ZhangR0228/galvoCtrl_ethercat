#!/usr/bin/env python3
"""
Generate and optionally run a ZMotion machining job.

Default behavior:
    python zmotion_machining_job.py

This writes output/laser_rectangle_job.bas and does not connect to hardware.

Run on a controller:
    python zmotion_machining_job.py --run --ip 192.168.0.11
"""

from __future__ import annotations

import argparse
import ctypes
import os
from pathlib import Path
from typing import Iterable, List, Sequence, Tuple


ROOT = Path(__file__).resolve().parent
DEFAULT_OUTPUT = ROOT / "output" / "laser_rectangle_job.bas"


Point = Tuple[float, float]


def default_tool_path() -> List[Point]:
    return [
        (0.0, 0.0),
        (40.0, 0.0),
        (40.0, 25.0),
        (0.0, 25.0),
        (0.0, 0.0),
    ]


def fmt(value: float) -> str:
    text = f"{value:.4f}".rstrip("0").rstrip(".")
    return text if text else "0"


def build_basic_program(
    path: Sequence[Point],
    *,
    axis_x: int,
    axis_y: int,
    laser_output: int,
    rapid_speed: float,
    work_speed: float,
    accel: float,
    decel: float,
) -> List[str]:
    if len(path) < 2:
        raise ValueError("The machining path needs at least two XY points.")

    start_x, start_y = path[0]
    lines = [
        "' ZMotion laser machining job",
        "' Units: mm, mm/s",
        f"BASE({axis_x},{axis_y})",
        "ATYPE=1",
        f"SPEED={fmt(rapid_speed)}",
        f"ACCEL={fmt(accel)}",
        f"DECEL={fmt(decel)}",
        f"MOVEABS({fmt(start_x)},{fmt(start_y)})",
        "WAIT IDLE",
        f"SPEED={fmt(work_speed)}",
        f"OP({laser_output},ON)",
    ]

    for x, y in path[1:]:
        lines.append(f"MOVEABS({fmt(x)},{fmt(y)})")
        lines.append("WAIT IDLE")

    lines.extend(
        [
            f"OP({laser_output},OFF)",
            f"SPEED={fmt(rapid_speed)}",
            f"MOVEABS({fmt(start_x)},{fmt(start_y)})",
            "WAIT IDLE",
            "END",
            "",
        ]
    )
    return lines


class ZMotionController:
    def __init__(self, ip: str) -> None:
        dll_matches = list(ROOT.glob("*/zauxdll.dll"))
        if not dll_matches:
            raise FileNotFoundError(f"ZMotion DLL not found under: {ROOT}")
        dll_path = dll_matches[0]

        if hasattr(os, "add_dll_directory"):
            os.add_dll_directory(str(dll_path.parent))
        self._dll = ctypes.WinDLL(str(dll_path))
        self._handle = ctypes.c_void_p()

        self._dll.ZAux_OpenEth.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
        self._dll.ZAux_OpenEth.restype = ctypes.c_int32
        self._dll.ZAux_Close.argtypes = [ctypes.c_void_p]
        self._dll.ZAux_Close.restype = ctypes.c_int32
        self._dll.ZAux_DirectCommand.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_uint32,
        ]
        self._dll.ZAux_DirectCommand.restype = ctypes.c_int32

        rc = self._dll.ZAux_OpenEth(ip.encode("ascii"), ctypes.byref(self._handle))
        if rc != 0:
            raise RuntimeError(f"ZAux_OpenEth failed, rc={rc}, ip={ip}")

    def close(self) -> None:
        if self._handle:
            self._dll.ZAux_Close(self._handle)
            self._handle = ctypes.c_void_p()

    def command(self, line: str) -> str:
        response = ctypes.create_string_buffer(2048)
        rc = self._dll.ZAux_DirectCommand(
            self._handle,
            line.encode("ascii"),
            response,
            ctypes.sizeof(response),
        )
        decoded = response.value.decode("gbk", errors="replace")
        if rc != 0:
            raise RuntimeError(f"Command failed, rc={rc}, command={line}, response={decoded}")
        return decoded

    def run_program(self, lines: Iterable[str]) -> None:
        for raw_line in lines:
            line = raw_line.strip()
            if not line or line.startswith("'"):
                continue
            if line.upper() == "END":
                continue
            self.command(line)

    def __enter__(self) -> "ZMotionController":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:  # type: ignore[no-untyped-def]
        self.close()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate and optionally run a ZMotion machining job.")
    parser.add_argument("--ip", default="192.168.0.11", help="ZMotion controller IP address.")
    parser.add_argument("--run", action="store_true", help="Connect to the controller and execute the job.")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUTPUT, help="Output BASIC machining file.")
    parser.add_argument("--axis-x", type=int, default=0, help="Machine X axis number.")
    parser.add_argument("--axis-y", type=int, default=1, help="Machine Y axis number.")
    parser.add_argument("--laser-output", type=int, default=0, help="Digital output used for laser enable.")
    parser.add_argument("--rapid-speed", type=float, default=80.0, help="Rapid speed in mm/s.")
    parser.add_argument("--work-speed", type=float, default=25.0, help="Machining speed in mm/s.")
    parser.add_argument("--accel", type=float, default=500.0, help="Axis acceleration.")
    parser.add_argument("--decel", type=float, default=500.0, help="Axis deceleration.")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    program = build_basic_program(
        default_tool_path(),
        axis_x=args.axis_x,
        axis_y=args.axis_y,
        laser_output=args.laser_output,
        rapid_speed=args.rapid_speed,
        work_speed=args.work_speed,
        accel=args.accel,
        decel=args.decel,
    )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(program), encoding="utf-8")
    print(f"Wrote machining file: {args.out}")

    if args.run:
        with ZMotionController(args.ip) as controller:
            controller.run_program(program)
        print(f"Executed job on controller: {args.ip}")
    else:
        print("Dry run only. Add --run to execute on the controller.")


if __name__ == "__main__":
    main()
