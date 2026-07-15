#!/usr/bin/env python3
"""
Work 4 — PC client: truy cập SD qua UART (USART1 / ST-Link VCP).

Cài: pip install pyserial

Ví dụ:
  python tools/uart_pc_cli.py COM5
  python tools/uart_pc_cli.py COM5 LIST
  python tools/uart_pc_cli.py COM5 WRITE demo.txt Hello from PC
  python tools/uart_pc_cli.py COM5 READ demo.txt
"""

from __future__ import annotations

import argparse
import sys
import time

try:
    import serial
except ImportError:
    print("Thiếu pyserial. Chạy: pip install pyserial", file=sys.stderr)
    sys.exit(1)


def open_port(port: str, baud: int = 115200) -> serial.Serial:
    ser = serial.Serial(port, baudrate=baud, timeout=0.3)
    time.sleep(0.2)
    ser.reset_input_buffer()
    return ser


def send_cmd(ser: serial.Serial, line: str, wait_s: float = 3.0) -> str:
    ser.reset_input_buffer()
    ser.write((line.strip() + "\r\n").encode("utf-8", errors="replace"))
    ser.flush()

    deadline = time.time() + wait_s
    chunks: list[bytes] = []
    idle_rounds = 0

    while time.time() < deadline:
        data = ser.read(256)
        if data:
            chunks.append(data)
            idle_rounds = 0
            text = b"".join(chunks).decode("utf-8", errors="replace")
            if "END\r\n" in text or "END\n" in text:
                break
            if text.startswith("ERR"):
                # chờ thêm một chút rồi thoát
                idle_rounds = 2
            if any(
                k in text
                for k in ("OK PONG", "OK written=", "OK\r\n", "OK\n")
            ) and ("READ" not in line.upper()) and ("LIST" not in line.upper()) and ("HELP" not in line.upper()):
                idle_rounds = 2
        else:
            if chunks:
                idle_rounds += 1
                if idle_rounds >= 3:
                    break
    return b"".join(chunks).decode("utf-8", errors="replace")


def main() -> int:
    ap = argparse.ArgumentParser(description="Work 4 UART PC client")
    ap.add_argument("port", help="COM port, ví dụ COM5")
    ap.add_argument(
        "cmd",
        nargs=argparse.REMAINDER,
        help="Lệnh một lần; bỏ trống = chế độ tương tác",
    )
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    ser = open_port(args.port, args.baud)
    try:
        if args.cmd:
            sys.stdout.write(send_cmd(ser, " ".join(args.cmd)))
            return 0

        print("Work4 UART CLI — HELP / LIST / READ / WRITE / …  (quit để thoát)")
        time.sleep(0.3)
        leftover = ser.read(2048)
        if leftover:
            sys.stdout.write(leftover.decode("utf-8", errors="replace"))

        while True:
            try:
                line = input("> ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                break
            if not line:
                continue
            if line.lower() in {"quit", "exit"}:
                break
            sys.stdout.write(send_cmd(ser, line))
            sys.stdout.flush()
    finally:
        ser.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
