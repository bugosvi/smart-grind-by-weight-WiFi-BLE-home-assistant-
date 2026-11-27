#!/usr/bin/env python3
"""
Automated pre-grinder bench test runner for Smart Grind-by-Weight (ESP32-S3).

Runs directly on the host computer connected to the Waveshare ESP32-S3 board
(no mains power / no grinder motor required). Uses only Python 3 stdlib for
all HTTP & WebSocket checks, plus optional `pyserial` for USB Serial capture.

Examples:
  # 1. Flash V2 over USB, inspect boot logs, auto-detect IP, run all API/WS checks
  python3 tools/bench_test.py --flash --hardware v2

  # 2. Board already flashed & on Wi-Fi — run all HTTP + WebSocket checks
  python3 tools/bench_test.py --host smartgrind.local
  python3 tools/bench_test.py --host 192.168.1.42

  # 3. Capture USB Serial boot logs only (no Wi-Fi yet)
  python3 tools/bench_test.py --serial /dev/ttyACM0 --skip-network
"""

from __future__ import annotations

import argparse
import base64
import datetime
import json
import os
import re
import socket
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional

ROOT = Path(__file__).resolve().parents[1]

CRASH_PATTERNS = (
    "Guru Meditation Error",
    "Backtrace:",
    "Brownout detector was triggered",
    "Task watchdog got triggered",
    "CORRUPT HEAP",
    "LittleFS Mount Failed",
    "Failed to mount LittleFS",
)

WARN_PATTERNS = (
    "HX711 sample rate detected",
    "near ADC rail",
    "Failed to open adaptive coast NVS",
)


@dataclass
class CheckResult:
    name: str
    status: str  # PASS | WARN | FAIL | SKIP
    detail: str


class BenchRunner:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.results: list[CheckResult] = []
        self.host: Optional[str] = args.host

    def record(self, name: str, status: str, detail: str) -> None:
        self.results.append(CheckResult(name, status, detail))
        icon = {"PASS": "[PASS]", "WARN": "[WARN]", "FAIL": "[FAIL]", "SKIP": "[SKIP]"}[status]
        print(f"  {icon} {name}: {detail}")

    # ------------------------------------------------------------------
    # Step 1: Optional USB Flash
    # ------------------------------------------------------------------
    def run_usb_flash(self) -> bool:
        if not self.args.flash:
            return True
        if not self.args.hardware:
            self.record("USB Flash", "FAIL", "--hardware v1 or v2 is required with --flash")
            return False

        py = ROOT / "tools/venv/bin/python3"
        python_exe = str(py) if py.exists() else sys.executable
        cmd = [python_exe, str(ROOT / "tools/grinder.py"), "usb", "--hardware", self.args.hardware]
        if self.args.serial:
            cmd.extend(["--port", self.args.serial])
        print(f"\n=== Flashing {self.args.hardware.upper()} via USB ===")
        print("Running:", " ".join(cmd))
        try:
            subprocess.run(cmd, cwd=str(ROOT), check=True)
            self.record("USB Flash", "PASS", f"Flashed hardware={self.args.hardware}")
            return True
        except subprocess.CalledProcessError as exc:
            self.record("USB Flash", "FAIL", f"grinder.py usb exited with code {exc.returncode}")
            return False

    # ------------------------------------------------------------------
    # Step 2: Optional USB Serial Boot Log Capture & Auto-IP Detection
    # ------------------------------------------------------------------
    def _detect_serial_port(self) -> Optional[str]:
        if self.args.serial:
            return self.args.serial
        try:
            import serial.tools.list_ports  # type: ignore
        except ImportError:
            return None
        for p in serial.tools.list_ports.comports():
            desc = f"{p.device} {p.description} {p.hwid}".lower()
            if any(tok in desc for tok in ("esp32", "usb jtag", "ch340", "cp210", "ttyacm", "ttyusb", "usbmodem")):
                return p.device
        return None

    def run_serial_boot_check(self) -> None:
        if not self.args.serial and not self.args.flash:
            return
        try:
            import serial  # type: ignore
        except ImportError:
            self.record("Serial Boot Log", "SKIP", "pyserial not installed (run inside tools/venv or pip install pyserial)")
            return

        port = self._detect_serial_port()
        if not port:
            self.record("Serial Boot Log", "WARN", "No ESP32 USB serial port auto-detected; pass --serial <port>")
            return

        print(f"\n=== Capturing USB Serial boot log on {port} ({self.args.serial_seconds}s) ===")
        lines: list[str] = []
        try:
            with serial.Serial(port, baudrate=115200, timeout=0.25) as ser:
                deadline = time.monotonic() + self.args.serial_seconds
                while time.monotonic() < deadline:
                    raw = ser.readline()
                    if not raw:
                        continue
                    line = raw.decode("utf-8", errors="replace").rstrip()
                    if line:
                        lines.append(line)
                        if self.args.verbose:
                            print("  | " + line)
        except Exception as exc:
            self.record("Serial Boot Log", "FAIL", f"Could not read {port}: {exc}")
            return

        joined = "\n".join(lines)
        crashes = [pat for pat in CRASH_PATTERNS if pat in joined]
        warns = [pat for pat in WARN_PATTERNS if pat in joined]

        if not self.host:
            m_ip = re.search(r"http://([0-9]{1,3}(?:\.[0-9]{1,3}){3})", joined)
            if not m_ip:
                m_ip = re.search(r"\bIP(?:\s+address)?[:=]\s*([0-9]{1,3}(?:\.[0-9]{1,3}){3})", joined, re.I)
            if m_ip:
                self.host = m_ip.group(1)
                print(f"  [INFO] Auto-detected device IP from serial: {self.host}")

        if crashes:
            self.record("Serial Boot Log", "FAIL", f"Crash/mount error detected: {', '.join(crashes)}")
        elif warns:
            self.record("Serial Boot Log", "WARN", f"Captured {len(lines)} lines; warnings: {', '.join(warns)}")
        elif lines:
            self.record("Serial Boot Log", "PASS", f"Captured {len(lines)} clean serial lines (no crash/panic)")
        else:
            self.record("Serial Boot Log", "WARN", f"0 lines received on {port} (press Reset on board or check port)")

    # ------------------------------------------------------------------
    # HTTP Helpers
    # ------------------------------------------------------------------
    def _http(
        self,
        method: str,
        path: str,
        *,
        body: Optional[bytes] = None,
        content_type: Optional[str] = None,
        include_origin: bool = True,
        timeout: float = 6.0,
    ) -> tuple[int, str]:
        assert self.host
        url = f"http://{self.host}{path}"
        headers: dict[str, str] = {}
        if include_origin:
            headers["Origin"] = f"http://{self.host}"
        if content_type:
            headers["Content-Type"] = content_type
        req = urllib.request.Request(url, data=body, headers=headers, method=method)
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                return resp.status, resp.read().decode("utf-8", errors="replace")
        except urllib.error.HTTPError as err:
            return err.code, err.read().decode("utf-8", errors="replace")

    # ------------------------------------------------------------------
    # Pure-stdlib RFC6455 WebSocket Client (/ws)
    # ------------------------------------------------------------------
    def _ws_exchange(self, send_payload: Optional[dict[str, Any]] = None, timeout: float = 5.0) -> list[dict[str, Any]]:
        assert self.host
        host_part, _, port_str = self.host.partition(":")
        port = int(port_str) if port_str else 80
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        handshake = (
            f"GET /ws HTTP/1.1\r\n"
            f"Host: {self.host}\r\n"
            f"Origin: http://{self.host}\r\n"
            f"Upgrade: websocket\r\n"
            f"Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            f"Sec-WebSocket-Version: 13\r\n\r\n"
        ).encode("ascii")

        def send_text(sock: socket.socket, text: str) -> None:
            data = text.encode("utf-8")
            mask = os.urandom(4)
            header = bytearray([0x81])
            n = len(data)
            if n < 126:
                header.append(0x80 | n)
            elif n < 65536:
                header.append(0x80 | 126)
                header.extend(struct.pack("!H", n))
            else:
                header.append(0x80 | 127)
                header.extend(struct.pack("!Q", n))
            header.extend(mask)
            masked = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
            sock.sendall(bytes(header) + masked)

        leftover = bytearray()

        def recv_exact(sock: socket.socket, n: int) -> bytes:
            while len(leftover) < n:
                chunk = sock.recv(max(4096, n - len(leftover)))
                if not chunk:
                    raise EOFError("WebSocket closed")
                leftover.extend(chunk)
            out = bytes(leftover[:n])
            del leftover[:n]
            return out

        def recv_frame(sock: socket.socket) -> Optional[str]:
            b1, b2 = recv_exact(sock, 2)
            opcode = b1 & 0x0F
            masked = (b2 & 0x80) != 0
            length = b2 & 0x7F
            if length == 126:
                length = struct.unpack("!H", recv_exact(sock, 2))[0]
            elif length == 127:
                length = struct.unpack("!Q", recv_exact(sock, 8))[0]
            mask = recv_exact(sock, 4) if masked else b""
            payload = recv_exact(sock, length)
            if masked:
                payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
            if opcode == 0x1:
                return payload.decode("utf-8", errors="replace")
            if opcode == 0x8:
                return None
            return ""

        messages: list[dict[str, Any]] = []
        with socket.create_connection((host_part, port), timeout=timeout) as sock:
            sock.settimeout(timeout)
            sock.sendall(handshake)
            resp = b""
            while b"\r\n\r\n" not in resp:
                chunk = sock.recv(1024)
                if not chunk:
                    raise RuntimeError("Empty WebSocket handshake response")
                resp += chunk
            head, tail = resp.split(b"\r\n\r\n", 1)
            leftover.extend(tail)
            first_line = head.split(b"\r\n", 1)[0].decode("ascii", errors="replace")
            if "101" not in first_line:
                raise RuntimeError(f"WebSocket handshake failed: {first_line}")

            if send_payload is not None:
                send_text(sock, json.dumps(send_payload))

            deadline = time.monotonic() + timeout
            want = 2 if send_payload is not None else 1
            while len(messages) < want and time.monotonic() < deadline:
                txt = recv_frame(sock)
                if txt is None:
                    break
                if not txt:
                    continue
                try:
                    messages.append(json.loads(txt))
                except json.JSONDecodeError:
                    pass
        return messages

    # ------------------------------------------------------------------
    # Step 3: Automated Network / API / WebSocket Checks
    # ------------------------------------------------------------------
    def run_network_checks(self) -> None:
        if self.args.skip_network:
            return
        if not self.host:
            self.host = "smartgrind.local"
            print(f"\n[INFO] No --host provided; defaulting to {self.host}")

        print(f"\n=== Running Automated Bench API & WebSocket Checks against http://{self.host} ===")

        # 1. Smart Grind Studio HTML
        try:
            code, html = self._http("GET", "/")
            ok = code == 200 and "Smart Grind" in html and "History &amp; Coffee Stats" in html
            self.record(
                "Web Studio (GET /)",
                "PASS" if ok else "FAIL",
                f"HTTP {code}, {len(html)} bytes" + ("" if ok else " (missing expected Studio markers)"),
            )
        except Exception as exc:
            self.record("Web Studio (GET /)", "FAIL", f"Unreachable: {exc}")
            return

        # 2. Device Status
        try:
            code, body = self._http("GET", "/api/v1/status")
            data = json.loads(body) if code == 200 else {}
            ok = code == 200 and "firmware" in data and "hardware" in data
            detail = (
                f"HTTP {code} · fw={data.get('firmware')} hw={data.get('hardware')} ip={data.get('ip')}"
                if ok
                else f"HTTP {code}: {body[:120]}"
            )
            self.record("Status (GET /api/v1/status)", "PASS" if ok else "FAIL", detail)
        except Exception as exc:
            self.record("Status (GET /api/v1/status)", "FAIL", str(exc))

        # 3. Diagnostic Logs
        try:
            code, logs = self._http("GET", "/api/v1/logs")
            crashes = [p for p in CRASH_PATTERNS if p in logs]
            warns = [p for p in WARN_PATTERNS if p in logs]
            if code != 200:
                self.record("Logs (GET /api/v1/logs)", "FAIL", f"HTTP {code}")
            elif crashes:
                self.record("Logs (GET /api/v1/logs)", "FAIL", f"Crash pattern in logs: {', '.join(crashes)}")
            elif warns:
                self.record("Logs (GET /api/v1/logs)", "WARN", f"{len(logs)}B · warnings: {', '.join(warns)}")
            else:
                self.record("Logs (GET /api/v1/logs)", "PASS", f"{len(logs)}B clean diagnostic buffer")
        except Exception as exc:
            self.record("Logs (GET /api/v1/logs)", "FAIL", str(exc))

        # 4. Settings Schema (including manual_hold_mode & grind_layout)
        try:
            code, body = self._http("GET", "/api/v1/settings")
            data = json.loads(body) if code == 200 else {}
            has_keys = all(k in data for k in ("profiles", "swipe_enabled", "manual_hold_mode", "display"))
            layout = data.get("display", {}).get("grind_layout")
            self.record(
                "Settings (GET /api/v1/settings)",
                "PASS" if (code == 200 and has_keys) else "FAIL",
                f"HTTP {code} · manual_hold={data.get('manual_hold_mode')} layout={layout} profiles={len(data.get('profiles', []))}",
            )
        except Exception as exc:
            self.record("Settings (GET /api/v1/settings)", "FAIL", str(exc))

        # 5. Safety Config + Origin CSRF & Bounds Guard (non-destructive)
        safety_data: dict[str, Any] = {}
        try:
            code, body = self._http("GET", "/api/v1/safety")
            safety_data = json.loads(body) if code == 200 else {}
            ok = code == 200 and "remote_max_weight_g" in safety_data and "require_cup_on_remote" in safety_data
            self.record(
                "Safety GET (/api/v1/safety)",
                "PASS" if ok else "FAIL",
                f"HTTP {code} · require_cup={safety_data.get('require_cup_on_remote')} max_g={safety_data.get('remote_max_weight_g')} coast_ms={safety_data.get('learned_coast_time_ms')}",
            )

            # Verify missing Origin is blocked with 403
            c403, _ = self._http(
                "POST",
                "/api/v1/safety",
                body=b'{"remote_max_weight_g":30}',
                content_type="application/json",
                include_origin=False,
            )
            self.record(
                "Safety CSRF Origin Guard",
                "PASS" if c403 == 403 else "FAIL",
                f"POST without Origin returned HTTP {c403} (expected 403)",
            )

            # Verify invalid out-of-range weight is rejected with 400
            c400, _ = self._http(
                "POST",
                "/api/v1/safety",
                body=b'{"remote_max_weight_g":999}',
                content_type="application/json",
                include_origin=True,
            )
            self.record(
                "Safety Bounds Guard",
                "PASS" if c400 == 400 else "FAIL",
                f"POST remote_max_weight_g=999 returned HTTP {c400} (expected 400)",
            )
        except Exception as exc:
            self.record("Safety API (/api/v1/safety)", "FAIL", str(exc))

        # 6. Lifetime Stats Endpoint (/api/v1/stats)
        try:
            code, body = self._http("GET", "/api/v1/stats")
            data = json.loads(body) if code == 200 else {}
            ok = code == 200 and all(k in data for k in ("total_grinds", "total_weight_kg", "motor_runtime_sec"))
            self.record(
                "Lifetime Stats (GET /api/v1/stats)",
                "PASS" if ok else "FAIL",
                f"HTTP {code} · grinds={data.get('total_grinds')} total_kg={data.get('total_weight_kg')} motor_s={data.get('motor_runtime_sec')}",
            )
        except Exception as exc:
            self.record("Lifetime Stats (GET /api/v1/stats)", "FAIL", str(exc))

        # 7. History Archive (/api/v1/history)
        try:
            code, body = self._http("GET", "/api/v1/history")
            data = json.loads(body) if code == 200 else {}
            ok = code == 200 and isinstance(data.get("sessions"), list)
            self.record(
                "History (GET /api/v1/history)",
                "PASS" if ok else "FAIL",
                f"HTTP {code} · {len(data.get('sessions', []))} saved sessions",
            )
        except Exception as exc:
            self.record("History (GET /api/v1/history)", "FAIL", str(exc))

        # 8. Morning Note Status + Clock Sync (/api/v1/note & /api/v1/note/clock)
        try:
            code, body = self._http("GET", "/api/v1/note")
            data = json.loads(body) if code == 200 else {}
            ok = code == 200 and "has_image" in data
            self.record(
                "Morning Note (GET /api/v1/note)",
                "PASS" if ok else "FAIL",
                f"HTTP {code} · has_image={data.get('has_image')} clock_synced={data.get('clock_synced')}",
            )

            epoch = int(time.time())
            local_now = datetime.datetime.now().astimezone()
            tz_offset_min = int((local_now.utcoffset() or datetime.timedelta()).total_seconds() // 60)
            form = urllib.parse.urlencode({"epoch": str(epoch), "tz_offset_min": str(tz_offset_min)}).encode("ascii")
            c_clk, clk_body = self._http(
                "POST",
                "/api/v1/note/clock",
                body=form,
                content_type="application/x-www-form-urlencoded",
                include_origin=True,
            )
            clk_data = json.loads(clk_body) if c_clk == 200 else {}
            ok_clk = c_clk == 200 and bool(clk_data.get("clock_synced"))
            self.record(
                "Note Clock Sync (POST /api/v1/note/clock)",
                "PASS" if ok_clk else "FAIL",
                f"HTTP {c_clk} · tz_offset_min={tz_offset_min} clock_synced={clk_data.get('clock_synced')}",
            )
        except Exception as exc:
            self.record("Morning Note API", "FAIL", str(exc))

        # 9. Live WebSocket Telemetry (/ws)
        try:
            msgs = self._ws_exchange(timeout=5.0)
            if msgs:
                m = msgs[0]
                self.record(
                    "WebSocket Telemetry (/ws)",
                    "PASS",
                    f"phase={m.get('phase')} weight={m.get('weight')} mode={m.get('mode')} scale_ok={not m.get('scale_fault', False)}",
                )
            else:
                self.record("WebSocket Telemetry (/ws)", "FAIL", "Connected but received 0 telemetry frames")
        except Exception as exc:
            self.record("WebSocket Telemetry (/ws)", "FAIL", str(exc))

        # 10. Optional Cup-Interlock Refusal Test over /ws
        if self.args.test_cup_interlock:
            if not safety_data.get("require_cup_on_remote"):
                self.record(
                    "Cup Interlock Refusal (/ws)",
                    "SKIP",
                    "require_cup_on_remote is OFF; enable it first to test refusal with empty scale",
                )
            else:
                try:
                    msgs = self._ws_exchange(
                        send_payload={"action": "start", "request_id": 9901, "target_weight": 18.0},
                        timeout=4.0,
                    )
                    ack = next((x for x in msgs if x.get("type") == "ack" or "accepted" in x), None)
                    if ack and ack.get("accepted") is False:
                        self.record(
                            "Cup Interlock Refusal (/ws)",
                            "PASS",
                            f"Refused cleanly: reason={ack.get('reason')}",
                        )
                    else:
                        self.record("Cup Interlock Refusal (/ws)", "FAIL", f"Unexpected WS response: {msgs}")
                except Exception as exc:
                    self.record("Cup Interlock Refusal (/ws)", "FAIL", str(exc))

    def print_summary(self) -> int:
        passes = sum(1 for r in self.results if r.status == "PASS")
        warns = sum(1 for r in self.results if r.status == "WARN")
        fails = sum(1 for r in self.results if r.status == "FAIL")
        skips = sum(1 for r in self.results if r.status == "SKIP")
        print("\n=== Bench Test Summary ===")
        print(f"  PASS: {passes}  |  WARN: {warns}  |  FAIL: {fails}  |  SKIP: {skips}")
        print("\nManual visual checks remaining on the desk (see docs/BENCH_TESTING.md):")
        print("  1. Power-cycle USB & verify relay output stays completely OFF during boot.")
        print("  2. Verify AMOLED touch accuracy, Portafilter screen, and Morning Note preview.")
        print("  3. If HX711 is wired: verify tare/calibration stability and '+' pulse button on cup lift.")
        return 1 if fails else 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Automated Smart Grind-by-Weight bench tester")
    parser.add_argument("--flash", action="store_true", help="Flash firmware over USB before testing")
    parser.add_argument("--hardware", choices=("v1", "v2"), help="Hardware revision (required with --flash)")
    parser.add_argument("--serial", help="Serial port (e.g. /dev/ttyACM0 or COM5)")
    parser.add_argument("--serial-seconds", type=float, default=10.0, help="Seconds to capture boot serial log")
    parser.add_argument("--host", help="Device IP or hostname (default: auto from serial or smartgrind.local)")
    parser.add_argument("--skip-network", action="store_true", help="Skip HTTP/WebSocket tests (USB serial only)")
    parser.add_argument(
        "--test-cup-interlock",
        action="store_true",
        help="Send a remote start over /ws with empty scale to verify Cup Interlock refusal",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Print all captured serial lines")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    runner = BenchRunner(args)
    if not runner.run_usb_flash():
        return runner.print_summary()
    runner.run_serial_boot_check()
    runner.run_network_checks()
    return runner.print_summary()


if __name__ == "__main__":
    raise SystemExit(main())
