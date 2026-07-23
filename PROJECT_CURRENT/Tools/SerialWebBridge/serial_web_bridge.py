#!/usr/bin/env python3
"""
Serial<->Web bridge for the ECU_TestV1_EGT_DRY_START_PATCH firmware.

Serves the exact same dashboard page the ECU's own WiFi web server serves,
but talks to the ECU over a Serial/USB-TTL link (GSU_DEBUG_UART1) instead of
WiFi. Useful as a control channel that keeps working if the ECU's WiFi/SoftAP
becomes unresponsive (e.g. under a brownout-adjacent voltage sag), since it
never depends on the ESP32's WiFi radio at all.

Requires: pip install pyserial

Usage:
    python serial_web_bridge.py <serial-port> [--baud 115200] [--http-port 8080]

Example:
    python serial_web_bridge.py COM5
    python serial_web_bridge.py /dev/ttyUSB0 --baud 115200 --http-port 8080

Then open http://127.0.0.1:8080/ in a browser - it is the same page/controls
as the ECU's own http://192.168.4.1/, just routed over Serial.
"""
import argparse
import json
import os
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

try:
    import serial
except ImportError:
    print("Missing dependency: pip install pyserial", file=sys.stderr)
    sys.exit(1)

PAGE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "page.html")
STATUS_TIMEOUT_S = 1.0     # how long to wait for the "apijson" reply line
DRAIN_AFTER_CMD_S = 0.15   # time to drain a command's human-readable Serial.println output

ser_lock = threading.Lock()
ser = None  # serial.Serial, opened in main()
last_status_json = json.dumps({"error": "no response from ECU yet"})


def _read_line(timeout_s):
    """Read one line from the serial port within timeout_s, or return None."""
    deadline = time.time() + timeout_s
    buf = bytearray()
    while time.time() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf.extend(chunk)
            if b"\n" in buf:
                line, _, _rest = buf.partition(b"\n")
                return line.decode("utf-8", errors="replace").strip()
    return None


def poll_status():
    """Send 'apijson' and return the JSON text the firmware prints back."""
    global last_status_json
    with ser_lock:
        ser.reset_input_buffer()
        ser.write(b"apijson\n")
        line = _read_line(STATUS_TIMEOUT_S)
    if line and line.startswith("{"):
        last_status_json = line
    return last_status_json


def send_command(cmd: str):
    """Send a plain command (mirrors the web /cmd?c=... route) and drain its
    human-readable Serial.println() reply so it doesn't pollute the next
    status poll."""
    with ser_lock:
        ser.write(cmd.encode("utf-8") + b"\n")
        deadline = time.time() + DRAIN_AFTER_CMD_S
        while time.time() < deadline:
            n = ser.in_waiting
            if n:
                ser.read(n)
            else:
                time.sleep(0.01)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass  # keep console output quiet; comment out to debug HTTP traffic

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/":
            self._serve_page()
        elif parsed.path == "/api":
            body = poll_status().encode("utf-8")
            self._respond(200, "application/json", body)
        elif parsed.path == "/cmd":
            c = parse_qs(parsed.query).get("c", [""])[0]
            if c:
                send_command(c)
            self._respond(200, "text/plain", b"OK")
        else:
            self._respond(404, "text/plain", b"Not found")

    def _serve_page(self):
        try:
            with open(PAGE_PATH, "rb") as f:
                body = f.read()
            self._respond(200, "text/html; charset=utf-8", body)
        except OSError as e:
            self._respond(500, "text/plain", str(e).encode("utf-8"))

    def _respond(self, code, content_type, body: bytes):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    global ser
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("port", help="Serial port, e.g. COM5 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--http-port", type=int, default=8080)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0)
    time.sleep(0.3)  # let the port settle
    ser.reset_input_buffer()

    httpd = ThreadingHTTPServer(("127.0.0.1", args.http_port), Handler)
    print(f"Serial: {args.port} @ {args.baud}")
    print(f"Dashboard: http://127.0.0.1:{args.http_port}/")
    print("Ctrl+C to stop.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        httpd.server_close()
        ser.close()


if __name__ == "__main__":
    main()
