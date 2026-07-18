# PROJECT_CURRENT — Dự Án ECU Đang Phát Triển

**Trạng thái**: Đang phát triển tích cực  
**Cập nhật**: 2026-07-18

---

## Cấu Trúc Thư Mục

```
PROJECT_CURRENT/
│
├── Firmware/                          ← Code ESP32 (upload bằng Arduino IDE)
│   └── ECU_TestV1_EGT_DRY_START_PATCH/
│       └── ECU_TestV1_EGT_DRY_START_PATCH.ino   ← Firmware chính
│
├── Hardware/                          ← Thiết kế mạch
│   ├── RPM_Sensor_20260709.net        ← Netlist cảm biến RPM (PADS PCB)
│   └── SCH_MinijetengineECU_20260709.json ← Sơ đồ mạch ECU (JSON)
│
├── Docs/                              ← Tài liệu kỹ thuật
│   ├── CODE_ARCHITECTURE.md           ← Kiến trúc firmware (state machine, ISR...)
│   ├── CODE_REVIEW_FINDINGS.md        ← Kết quả review code (các lỗi đã fix)
│   └── COMMISSIONING_GUIDE.md         ← Hướng dẫn toàn diện: upload, hiệu chỉnh RPM,
│                                          Test Wizard, dry/wet-start, tăng throttle
│
└── README.md                          ← File này
```

---

## Bắt Đầu Nhanh

### Upload Firmware
1. Mở `Firmware/ECU_TestV1_EGT_DRY_START_PATCH/ECU_TestV1_EGT_DRY_START_PATCH.ino`  
   bằng **Arduino IDE**
2. Board: **ESP32 Dev Module** (hoặc NodeMCU-32S)
3. Thư viện cần: `Adafruit_MAX31855`, `WiFi`, `WebServer`, `SD` (ESC PWM dùng LEDC có sẵn trong ESP32 core — **không** cần `ESP32Servo`, xem `CLAUDE.md`)
4. Upload → Mở Serial Monitor 115200 baud

### Truy Cập Web UI
- WiFi SSID: **ECU_TestV1**
- Password: **admin1234**
- URL: **http://192.168.4.1**

---

## Pinout Chính

| Chức năng | GPIO |
|-----------|------|
| MAX31855 CLK | 18 |
| MAX31855 CS | 5 |
| MAX31855 DO | 19 |
| RPM Sensor | **33** |
| Pump ESC | 26 |
| Starter ESC | 25 |
| Valve 1 | 17 |
| Valve 2 | 16 |
| Ignition/Glow | 32 |
| User Button | 22 |
| Status LED | 2 |
| SD CS/SCK/MOSI/MISO | 13/14/23/27 |

---

## Tài Liệu Đọc Theo Thứ Tự

| # | File | Khi nào đọc |
|---|------|------------|
| 1 | `Docs/COMMISSIONING_GUIDE.md` | Từ lần đầu upload ESP32 tới chạy thật có throttle (gồm cả hiệu chỉnh RPM DSO152) |
| 2 | `Docs/CODE_ARCHITECTURE.md` | Khi cần hiểu sâu firmware |
| 3 | `Docs/CODE_REVIEW_FINDINGS.md` | Tham khảo các lỗi đã biết/đã fix |

---

## Cảnh Báo An Toàn

- ESP32 chỉ chịu **3.3V** — dùng level converter cho tín hiệu 5V
- Chỉ dùng **thermocouple loại K**
- Cảm biến RPM KMZ10A cần hiệu chỉnh trimpot **RP1/RP2/RP3** trước khi dùng
- Đọc `COMMISSIONING_GUIDE.md` trước khi test có nhiên liệu

---

**Trạng thái**: 🟢 Đang phát triển tích cực  
**Độ ổn định**: 🟠 Phiên bản test (dùng cẩn thận)
