# Ardu_ECU — Arduino ECU cho Động Cơ Phản Lực Mô Hình

ECU (Engine Control Unit) dễ tự chế tạo cho động cơ phản lực mô hình,
chạy trên nền ESP32 với giao diện cấu hình và giám sát qua web browser.

---

## Cấu Trúc Project

```
Ardu_ECU/
│
├── PROJECT_CURRENT/           ← Dự án đang phát triển (bắt đầu từ đây)
│   ├── Firmware/              ← Code ESP32
│   │   └── ECU_TestV1_EGT_DRY_START_PATCH/
│   │       └── *.ino          ← Mở bằng Arduino IDE
│   ├── Hardware/              ← Thiết kế mạch
│   │   ├── RPM_Sensor_20260709.net
│   │   └── SCH_MinijetengineECU_20260709.json
│   └── Docs/                  ← Tài liệu kỹ thuật
│       ├── COMMISSIONING_GUIDE.md   ← Hướng dẫn toàn diện: upload, hiệu chỉnh RPM,
│       │                               Test Wizard, dry/wet-start, tăng throttle
│       ├── CODE_ARCHITECTURE.md
│       └── CODE_REVIEW_FINDINGS.md
│
├── TEST/                      ← Firmware test riêng lẻ (bench test)
│   └── TEST_STARTER/
│       └── TEST_STARTER.ino   ← Test RPM sensor + starter (không cần ARM)
│
└── REFERENCES/                ← Tài liệu tham khảo (không dùng cho sản xuất)
    ├── Firmware/              ← Firmware cũ: Rev9, Rev10, Rev11, Rev12
    ├── Hardware/              ← Sơ đồ mạch cũ, ảnh linh kiện, file 3D
    └── Documentation/         ← Manual cũ + tài liệu động cơ
```

---

## Bắt Đầu Nhanh

### 1. Upload Firmware Chính
```
Mở: PROJECT_CURRENT/Firmware/ECU_TestV1_EGT_DRY_START_PATCH/
          ECU_TestV1_EGT_DRY_START_PATCH.ino
Board: ESP32 Dev Module (hoặc NodeMCU-32S)
Baudrate Serial: 115200
```

### 2. Kết Nối Web UI
```
WiFi: ECU_TestV1  |  Pass: admin1234
URL:  http://192.168.4.1
```

### 3. Đọc Tài Liệu Theo Thứ Tự

| Thứ tự | File | Mục đích |
|--------|------|---------|
| 1 | `PROJECT_CURRENT/Docs/COMMISSIONING_GUIDE.md` | Upload, hiệu chỉnh RPM, test, dry/wet-start, tăng throttle — tất cả trong 1 file |
| 2 | `PROJECT_CURRENT/Docs/CODE_ARCHITECTURE.md` | Hiểu sâu firmware |
| 3 | `PROJECT_CURRENT/Docs/CODE_REVIEW_FINDINGS.md` | Các lỗi đã biết và đã fix |

---

## Tính Năng Firmware Hiện Tại

- **EGT open dry-start**: khi thermocouple bị hở, tự động chuyển sang chế độ test RPM (starter chạy, nhiên liệu/van/lửa tắt hoàn toàn)
- **Hybrid fuel control**: bảng hiệu chuẩn pump (us ↔ ml/min) + điều chỉnh ±1µs/bước theo RPM
- **EGT gradient lookahead 3s**: ngăn tăng nhiên liệu nếu dự đoán EGT sẽ vượt ngưỡng
- **RPM noise guard**: lọc nhiễu tĩnh khi tất cả output OFF (REST_GUARD)
- **Test Wizard 9 bước**: phải pass trước khi cho phép start
- **ACCEL_TO_IDLE timeout**: tự dừng nếu không đạt idle sau 20s
- **Cooldown**: chạy starter làm mát sau khi dừng/abort
- **SD logging**: CSV 2 dòng/giây
- **Web UI**: Dashboard, Controls, Test Wizard, Event Log

---

## Phần Cứng

**Vi điều khiển**: ESP32 DevKit V1 (NodeMCU-32S)

| Cảm biến / Thiết bị | Giao tiếp | Chân GPIO |
|--------------------|----------|----------|
| Thermocouple K (MAX31855) | SPI | CLK=18, CS=5, DO=19 |
| RPM sensor (KMZ10A) | Digital | 33 |
| Pump ESC | PWM | 26 |
| Starter ESC | PWM | 25 |
| Valve 1 | Digital | 17 |
| Valve 2 | Digital | 16 |
| Ignition / Glow plug | Digital | 32 |
| User button | Digital | 22 (active LOW) |
| Status LED | Digital | 2 (active LOW) |
| MicroSD (SPI) | SPI | CS=13, SCK=14, MOSI=23, MISO=27 |

**Mạch cảm biến RPM**: KMZ10A → INA826 (gain ×38) → LMV358 (op-amp trim) → LMV393 (comparator Schmitt) → GPIO33  
Ba trimpot: RP1 (offset), RP2 (gain), RP3 (threshold). Xem `Docs/COMMISSIONING_GUIDE.md` (Giai đoạn 2).

---

## Firmware Test Riêng (TEST/)

| Firmware | Mục đích |
|----------|---------|
| `TEST/TEST_STARTER/TEST_STARTER.ino` | Test RPM sensor + starter PWM: dùng `+`/`-` tăng/giảm PWM, đánh giá NOISE và STAB tự động |

---

## Thư Viện Cần Cài (Arduino IDE)

```
Adafruit_MAX31855
WiFi         (có sẵn trong ESP32 core)
WebServer    (có sẵn trong ESP32 core)
SD           (có sẵn trong ESP32 core)
```

ESC PWM dùng thẳng LEDC có sẵn trong ESP32 core (`escAttach`/`escWriteUs`) —
**không** cần cài `ESP32Servo`. Xem `CLAUDE.md` để biết lý do (ESP32Servo từng
gây khựng starter độc lập với nguồn điện).

---

## Tài Liệu Tham Khảo (REFERENCES/)

| Thư mục | Nội dung |
|---------|---------|
| `Firmware/Rev11/` | Firmware ổn định Rev11 (tham khảo kiến trúc) |
| `Firmware/Rev12_TC10/` | Rev12 nhiều cảm biến (pressure, load cell) |
| `Hardware/Schematics/` | Sơ đồ mạch Rev9–11 |
| `Hardware/3D Printable Files/` | File STL motor mount, Bendix sleeve |
| `Documentation/Engine_Manual_NewerModel.pdf` | Tài liệu động cơ thế hệ mới |
| `Documentation/Luu_Luong_Bom_Thuc_Te.txt` | Lưu lượng bơm đo thực tế |

---

## Liên Hệ & Đóng Góp

- **Tác giả gốc**: Jehanzeb Khan — jehanzeb@digipak.org
- **License**: GNU AGPLv3 — xem [LICENSE.md](LICENSE.md)
- Mọi đóng góp đều được chào đón. Xem thêm tại [GitHub Issues](https://github.com/phamthemy3089/Ardu_ECU/issues).
