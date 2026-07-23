#!/usr/bin/env python3
"""
Bridge GSU cho firmware ECU_TestV1_EGT_DRY_START_PATCH.

Phục vụ giao diện điều khiển (page.html, UI duy nhất - firmware ESP32 không còn
trang web riêng) trên PC, và nói chuyện với ECU theo MỘT trong hai kiểu kết nối:

  * Dây (Serial/USB-TTL nối vào GSU_DEBUG_UART1): thấy được TẤT CẢ dữ liệu Serial
    thô (log khởi động/brownout, phản hồi lệnh) trong tab Terminal.
  * WiFi (SoftAP của ECU): gọi thẳng /api và /cmd qua HTTP. Terminal chỉ thấy
    Event Log của ECU (WiFi không sống sót qua lúc reset nên không bắt được log đó).

Cần: pip install pyserial  (chỉ cho chế độ Dây; chế độ WiFi dùng thư viện chuẩn).

Chạy:
    python serial_web_bridge.py                      # hỏi kiểu kết nối tương tác
    python serial_web_bridge.py wire COM5            # dây, cổng COM5
    python serial_web_bridge.py wifi 192.168.4.1     # wifi, IP của ECU
Tùy chọn thêm: [--baud 115200] [--http-port 8080]
Rồi mở http://127.0.0.1:8080/
"""
import argparse
import json
import os
import sys
import threading
import time
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs, quote

PAGE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "page.html")
TERM_MAX = 2000            # số dòng terminal giữ lại tối đa
STATUS_POLL_S = 0.4        # chu kỳ gửi "apijson" ở chế độ dây
WIFI_TIMEOUT_S = 1.5

state_lock = threading.Lock()
last_status = '{"error":"chua co du lieu tu ECU"}'
term_lines = []            # ring buffer các dòng terminal (log/phản hồi thô)
term_base = 0              # số dòng đã bị cắt khỏi đầu ring buffer (để đánh chỉ số tuyệt đối)

mode = "wire"
conn_state = "-"
ser = None                 # serial.Serial ở chế độ dây
wifi_base = ""             # http://<ip> ở chế độ wifi

last_rx_ms = 0             # thời điểm (time.time()) nhận được dòng Serial hợp lệ gần nhất, 0 = chưa nhận gì (chế độ dây)
wire_info = ""              # vd "COM5 @ 115200", hiển thị kèm trong conn_state
wire_target = ""           # tên cổng COM đã lưu, cho nút Kết nối lại
wire_baud = 115200         # baud đã lưu, cho nút Kết nối lại
WIRE_STALE_AFTER_S = 2.0    # quá lâu không nhận được dòng nào -> báo mất kết nối


def add_term(line):
    global term_base
    with state_lock:
        term_lines.append(line)
        if len(term_lines) > TERM_MAX:
            drop = len(term_lines) - TERM_MAX
            del term_lines[:drop]
            term_base += drop


def is_status_json(line):
    return line.startswith("{") and '"mode"' in line


# ---------------- Chế độ DÂY (Serial) ----------------

def wire_reader():
    """Đọc liên tục từng dòng Serial: dòng JSON trạng thái -> last_status;
    còn lại -> terminal buffer."""
    global last_status, last_rx_ms
    buf = bytearray()
    while True:
        s = ser
        if s is None:
            buf = bytearray()
            time.sleep(0.2)
            continue
        try:
            chunk = s.read(s.in_waiting or 1)
        except Exception as e:
            add_term("[bridge] loi doc serial: %s" % e)
            time.sleep(0.5)
            continue
        if not chunk:
            continue
        buf.extend(chunk)
        while b"\n" in buf:
            raw, _, rest = buf.partition(b"\n")
            buf = bytearray(rest)
            line = raw.decode("utf-8", errors="replace").rstrip("\r")
            if not line:
                continue
            with state_lock:
                last_rx_ms = time.time()
            if is_status_json(line):
                with state_lock:
                    last_status = line
            else:
                add_term(line)


def wire_watchdog():
    """Biến "mở cổng COM thành công lúc khởi động" thành trạng thái kết nối
    SỐNG: trước đây conn_state đứng yên ở thông báo lúc khởi động mãi mãi, kể
    cả khi ECU ngừng phản hồi hẳn (rút dây, crash, brownout). Suy ra tình
    trạng thật từ việc có nhận được dòng Serial nào gần đây hay không."""
    global conn_state
    while True:
        with state_lock:
            rx = last_rx_ms
        if not rx:
            conn_state = "Dang cho phan hoi... (%s)" % wire_info
        else:
            age = time.time() - rx
            if age < WIRE_STALE_AFTER_S:
                conn_state = "OK (%s)" % wire_info
            else:
                conn_state = "MAT KET NOI - khong phan hoi %ds (%s)" % (int(age), wire_info)
        time.sleep(0.3)


def wire_poller():
    """Định kỳ xin trạng thái JSON."""
    while True:
        s = ser
        if s is not None:
            try:
                s.write(b"apijson\n")
            except Exception:
                pass
        time.sleep(STATUS_POLL_S)


def wire_send(cmd):
    if ser is None:
        add_term("[bridge] chua co ket noi - bam 'Ket noi lai'")
        return
    try:
        ser.write(cmd.encode("utf-8") + b"\n")
    except Exception as e:
        add_term("[bridge] loi gui lenh: %s" % e)


def reconnect_wire():
    """Đóng và mở lại cổng COM đã lưu, để phục hồi sau khi rút/cắm lại dây
    hoặc ECU brownout mà không phải khởi động lại bridge."""
    global ser, last_rx_ms, conn_state
    try:
        import serial
    except ImportError:
        add_term("[bridge] thieu pyserial")
        return
    old = ser
    ser = None
    if old is not None:
        try:
            old.close()
        except Exception:
            pass
    try:
        ser = serial.Serial(wire_target, wire_baud, timeout=0)
        time.sleep(0.3)
        ser.reset_input_buffer()
        with state_lock:
            last_rx_ms = 0
        add_term("[bridge] da mo lai cong %s" % wire_info)
    except Exception as e:
        conn_state = "LOI MO LAI CONG: %s (%s)" % (e, wire_info)
        add_term("[bridge] ket noi lai that bai: %s" % e)


def reconnect():
    """Kết nối lại cho cả 2 chế độ."""
    global conn_state
    if mode == "wire":
        reconnect_wire()
    else:
        conn_state = "dang ket noi lai..."
        add_term("[bridge] thu ket noi lai WiFi %s" % wifi_base)


# ---------------- Chế độ WiFi (HTTP proxy) ----------------

def wifi_get(path):
    url = wifi_base + path
    with urllib.request.urlopen(url, timeout=WIFI_TIMEOUT_S) as r:
        return r.read().decode("utf-8", errors="replace")


def wifi_status_poller():
    """Poll /api để cập nhật trạng thái + rút Event Log vào terminal."""
    global last_status, conn_state
    last_logs = ""
    while True:
        try:
            js = wifi_get("/api?act=1")
            with state_lock:
                last_status = js
            conn_state = "OK"
            try:
                logs = json.loads(js).get("logs", "")
            except Exception:
                logs = ""
            if logs and logs != last_logs:
                # Event log là chuỗi nhiều dòng; chỉ thêm phần mới ở cuối.
                add_term("[event-log] " + logs.replace("\\n", " | ")[-400:])
                last_logs = logs
        except Exception as e:
            conn_state = "MAT KET NOI (%s)" % type(e).__name__
        time.sleep(0.5)


# ---------------- HTTP server (chung cho cả 2 chế độ) ----------------

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/":
            self._page()
        elif u.path == "/api":
            self._respond(200, "application/json", get_status().encode("utf-8"))
        elif u.path == "/cmd":
            c = parse_qs(u.query).get("c", [""])[0]
            if c:
                send_command(c)
            self._respond(200, "text/plain", b"OK")
        elif u.path == "/term":
            since = parse_qs(u.query).get("since", ["0"])[0]
            try:
                since = int(since)
            except ValueError:
                since = 0
            self._respond(200, "application/json", term_payload(since).encode("utf-8"))
        elif u.path == "/reconnect":
            reconnect()
            self._respond(200, "text/plain", b"OK")
        else:
            self._respond(404, "text/plain", b"Not found")

    def _page(self):
        try:
            with open(PAGE_PATH, "rb") as f:
                self._respond(200, "text/html; charset=utf-8", f.read())
        except OSError as e:
            self._respond(500, "text/plain", str(e).encode("utf-8"))

    def _respond(self, code, ctype, body):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def get_status():
    if mode == "wifi":
        # last_status đã được poller cập nhật; trả bản mới nhất.
        with state_lock:
            return last_status
    with state_lock:
        return last_status


def send_command(cmd):
    if mode == "wifi":
        try:
            wifi_get("/cmd?c=" + quote(cmd))
        except Exception as e:
            add_term("[bridge] loi gui lenh WiFi: %s" % e)
    else:
        wire_send(cmd)


def term_payload(since):
    with state_lock:
        total = term_base + len(term_lines)
        start = max(since, term_base)
        idx = start - term_base
        lines = term_lines[idx:] if 0 <= idx <= len(term_lines) else term_lines[:]
        return json.dumps({"mode": mode, "state": conn_state,
                           "lines": lines, "next": total})


def main():
    global mode, ser, wifi_base, conn_state, wire_info, wire_target, wire_baud
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("mode", nargs="?", choices=["wire", "wifi"], help="wire hoac wifi")
    ap.add_argument("target", nargs="?", help="cong COM (wire) hoac IP (wifi)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--http-port", type=int, default=8080)
    args = ap.parse_args()

    print("=== ECU Bridge GSU ===")
    mode = args.mode
    if not mode:
        s = input("Kieu ket noi - go 'wire' (day/Serial) hoac 'wifi' [wire]: ").strip().lower()
        mode = s if s in ("wire", "wifi") else "wire"

    if mode == "wire":
        try:
            import serial  # noqa
        except ImportError:
            print("Che do day can: pip install pyserial", file=sys.stderr)
            sys.exit(1)
        import serial
        target = args.target or input("Cong COM (vd COM5): ").strip()
        ser = serial.Serial(target, args.baud, timeout=0)
        time.sleep(0.3)
        ser.reset_input_buffer()
        wire_target = target
        wire_baud = args.baud
        wire_info = "%s @ %d" % (target, args.baud)
        conn_state = "Dang cho phan hoi... (%s)" % wire_info
        threading.Thread(target=wire_reader, daemon=True).start()
        threading.Thread(target=wire_poller, daemon=True).start()
        threading.Thread(target=wire_watchdog, daemon=True).start()
    else:
        ip = args.target or input("IP cua ECU [192.168.4.1]: ").strip() or "192.168.4.1"
        wifi_base = "http://" + ip
        conn_state = "dang ket noi..."
        threading.Thread(target=wifi_status_poller, daemon=True).start()

    httpd = ThreadingHTTPServer(("127.0.0.1", args.http_port), Handler)
    print("Che do: %s" % mode.upper())
    print("Dashboard: http://127.0.0.1:%d/" % args.http_port)
    print("Ctrl+C de dung.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        httpd.server_close()
        if ser:
            ser.close()


if __name__ == "__main__":
    main()
