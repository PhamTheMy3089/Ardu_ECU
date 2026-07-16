# Hướng Dẫn Quy Trình Test & Check Nhiễu Trước Khi Test Động Cơ

**Firmware**: ECU_TestV1_EGT_DRY_START_PATCH  
**Phiên bản**: 2026-07-16  
**Yêu cầu**: Đọc hết tài liệu này TRƯỚC khi kết nối nguồn điện

---

## ⚠️ CẢNH BÁO AN TOÀN

- Luôn test trong không gian thoáng khí
- Tháo nhiên liệu ra khỏi hệ thống khi test điện tử
- Giữ khoảng cách an toàn ≥ 3m khi động cơ quay
- Có người thứ 2 giám sát khi test engine thực
- Chuẩn bị bình chữa cháy gần tầm tay

---

## 📋 GIAI ĐOẠN 0: Kiểm Tra Vật Lý Trước Khi Cấp Điện

### 0.1 Kiểm Tra Đấu Nối

| Điểm kiểm tra | Yêu cầu | Kết quả |
|---------------|---------|---------|
| ESP32 GPIO 18 → MAX31855 CLK | Không ngược | ☐ Pass ☐ Fail |
| ESP32 GPIO 5 → MAX31855 CS | Không ngược | ☐ Pass ☐ Fail |
| ESP32 GPIO 19 → MAX31855 DO | Không ngược | ☐ Pass ☐ Fail |
| Thermocouple loại K | Đúng loại, không J hoặc T | ☐ Pass ☐ Fail |
| ESP32 GPIO 33 → Hall sensor | Nguồn 5V qua level converter | ☐ Pass ☐ Fail |
| GPIO 26 → Pump ESC signal | Dây tín hiệu, không phải nguồn | ☐ Pass ☐ Fail |
| GPIO 25 → Starter ESC signal | Dây tín hiệu, không phải nguồn | ☐ Pass ☐ Fail |
| GPIO 17 → Valve 1 | MOSFET driver, không nối thẳng | ☐ Pass ☐ Fail |
| GPIO 16 → Valve 2 | MOSFET driver, không nối thẳng | ☐ Pass ☐ Fail |
| GPIO 32 → Ignition/Glow | MOSFET driver | ☐ Pass ☐ Fail |
| GPIO 22 → User Button | Pull-up, active LOW | ☐ Pass ☐ Fail |
| GPIO 2 → Status LED | Resistor hạn dòng | ☐ Pass ☐ Fail |
| SD Card: CS=13 SCK=14 MOSI=23 MISO=27 | Đúng pinout | ☐ Pass ☐ Fail |

### 0.2 Kiểm Tra Nguồn Điện

```
☐ Điện áp ESP32 đầu vào: 5V ± 0.2V (đo bằng đồng hồ)
☐ Điện áp 3.3V rail: 3.3V ± 0.1V (từ AMS1117 trên board)
☐ Nguồn ESC tách biệt với nguồn ESP32 (BEC riêng)
☐ Ground chung giữa ESP32 và ESC
☐ Không có ngắn mạch nào (đo điện trở GND-VCC khi chưa cấp điện)
```

---

## 📋 GIAI ĐOẠN 1: Kiểm Tra Khởi Động Đầu Tiên

### 1.1 Cấp Điện & Quan Sát Serial

**Kết nối Serial Monitor**: 115200 baud, Line ending: Newline (`\n`)

**Sau khi cấp điện, trong 5 giây đầu phải thấy:**
```
ECU TestV1 booting...
MAX31855 OK  (hoặc "MAX31855 OPEN - dry start mode")
SD init OK   (hoặc "SD init FAILED - logging disabled")
WiFi AP started: ECU_TestV1
Listening on 192.168.4.1
ECU mode: WAITING
```

**Nếu không thấy output gì**: Kiểm tra lại baudrate và cáp USB.  
**Nếu thấy "MAX31855 OPEN"**: Thermocouple bị lỏng hoặc chưa cắm — kiểm tra kết nối SPI.

### 1.2 Kiểm Tra EGT Sensor

Gõ command trong Serial Monitor:
```
status
```

Kỳ vọng:
```
EGT: 25.3°C (nhiệt độ phòng ± 5°C)
EGT OK: yes
```

**Chuẩn pass**: EGT đọc được trong khoảng 15–35°C (nhiệt độ phòng).  
**Nếu EGT = NaN hoặc 0**: SPI wiring sai — kiểm tra CLK/CS/DO.  
**Nếu EGT hiển thị giá trị bất thường (>100°C)**: Thermocouple bị chạm nóng hoặc phân cực ngược.

### 1.3 Kiểm Tra WiFi Web UI

```
1. Kết nối điện thoại/laptop vào WiFi: ECU_TestV1 (pass: admin1234)
2. Mở browser: http://192.168.4.1
3. Dashboard phải hiện EGT, RPM, Mode = WAITING
4. Kiểm tra Event Log có dòng "System boot"
```

---

## 📋 GIAI ĐOẠN 2: CHECK NHIỄU RPM (Quan Trọng Nhất)

> **Mục đích**: Phân biệt tín hiệu RPM thật từ cảm biến Hall vs nhiễu điện từ (EMI) từ ESC/igniter

### 2.1 Bật Chế Độ RPM Detail

Gõ vào Serial Monitor:
```
rpmdetail on
```

Kỳ vọng khi không có gì quay:
```
RPM window: edges=0 accepted=0 rejected=0 rpm=0
Quality: REST_GUARD (no signal - OK)
```

**Đây là trạng thái bình thường** — REST_GUARD đang lọc nhiễu khi tất cả output OFF.

### 2.2 Test Nhiễu Tĩnh (Không Có Motor)

**Quan sát liên tục 30 giây, ghi lại:**

| Chỉ số | Giá trị đo được | Chuẩn Pass |
|--------|----------------|------------|
| `edges` (raw edges) | | 0–3 trong 30s |
| `accepted` (pulses qua filter) | | 0 trong 30s |
| `rejected` (pulses bị lọc) | | ≤ 5 trong 30s |
| `rpm` hiển thị | | = 0 |
| Quality | | REST_GUARD hoặc NO_SIGNAL |

**Nếu `accepted > 0` khi không có gì quay**: Có nhiễu từ nguồn điện hoặc wiring — cần sửa trước khi tiếp tục.

### 2.3 Test Nhiễu Khi Glow Plug Bật (EMI Test)

> **Cảnh báo**: Chỉ test này khi KHÔNG có nhiên liệu. Glow plug sẽ nóng ngay.

Gõ lệnh test glow (nếu có):
```
test ign on
```
Quan sát RPM trong 5 giây:

| Chỉ số | Chuẩn Pass |
|--------|-----------|
| RPM tăng đột ngột? | Không (< 100 RPM) |
| `accepted` tăng? | Không (≤ 2 pulses) |
| `rejected` tăng? | OK nếu có (filter đang làm việc) |

```
test ign off
```

**Nếu RPM tăng khi bật glow**: Cần tăng `rpmMinPulseUs` lên 200–500µs:
```
set rpmfilter 300
```
Và lặp lại test trên.

### 2.4 Test Nhiễu Khi Starter ESC Chạy (EMI Test Quan Trọng Nhất)

> **Cảnh báo**: Tháo cánh/impeller khỏi starter motor trước khi test

Dùng Test Wizard trên Web UI hoặc:
```
test starter 30
```
(Chạy starter 30% throttle trong 5 giây)

Trong lúc starter chạy, quan sát Serial Monitor:

| Chỉ số | Chuẩn Pass |
|--------|-----------|
| `rejectPct` | < 30% |
| `jitterPct` | Bất kỳ (starter chưa gắn hall sensor) |
| Không có tín hiệu RPM giả | RPM = 0 hoặc NO_SIGNAL |

**Nếu RPM nhảy loạn khi starter chạy**: 
- Kiểm tra dây tín hiệu RPM (GPIO33) đi xa dây ESC
- Thêm tụ 100nF từ GPIO33 xuống GND gần ESP32
- Bọc dây tín hiệu bằng chống nhiễu

### 2.5 Test Tín Hiệu Hall Sensor Thực (Dùng Tay Quay)

Dùng tay quay spinner magnet qua cảm biến Hall với tốc độ chậm:

```
rpmreset
rpmdetail on
```

Quay chậm (khoảng 2–3 vòng/giây), quan sát:

| Chỉ số | Chuẩn Pass |
|--------|-----------|
| RPM hiển thị | > 0 khi quay |
| `accepted > 0` | Có (đang nhận tín hiệu) |
| `rejectPct` | < 10% khi quay đều |
| Quality | CLEAN hoặc RPM_WARN |

**Chuẩn chấp nhận tín hiệu Hall**:
- `rejectPct` < 10%: CLEAN — Tốt
- `rejectPct` 10–30%: WARN — Chấp nhận được, theo dõi  
- `rejectPct` > 30%: NOISY — Cần sửa wiring trước khi test engine

### 2.6 Test Filter RPM

Kiểm tra filter đang ở mức nào:
```
status
```
Tìm dòng `rpmFilter: 120us` (hoặc giá trị hiện tại).

| Giá trị filter | Tình huống |
|---------------|-----------|
| 120µs (default) | Tốt cho tín hiệu sạch |
| 200–300µs | Khi có nhiễu nhẹ từ igniter |
| 300–500µs | Khi nhiễu mạnh, giới hạn RPM max ~200k RPM |

---

## 📋 GIAI ĐOẠN 3: Test Checklist (Test Wizard Web UI)

Truy cập Web UI → Tab "Test Wizard" và thực hiện từng bước:

### Thứ tự thực hiện:

```
☐ Step 1: EGT Sensor — Kiểm tra EGT đọc giá trị hợp lý
☐ Step 2: RPM Sensor — Quay magnet qua Hall sensor, verify RPM > 0
☐ Step 3: Pump ESC — Chạy pump ở mức thấp nhất (1160µs = 50ml/min)
☐ Step 4: Starter ESC — Chạy starter ở 20%, verify motor quay
☐ Step 5: Valve 1 — Bật/tắt van 1, nghe click solenoid
☐ Step 6: Valve 2 — Bật/tắt van 2, nghe click solenoid  
☐ Step 7: Ignition/Glow — Bật igniter ngắn (2s), kiểm tra không có lửa bất thường
☐ Step 8: Button — Nhấn button vật lý, xác nhận response
☐ Step 9: SD Card — Kiểm tra file CSV được tạo trong SD
```

**Quy tắc**: Phải PASS tất cả 9 bước trước khi `startidle` cho phép chạy.  
**Nếu bước nào fail**: Sửa hardware trước khi tiếp tục.

---

## 📋 GIAI ĐOẠN 4: DRY START TEST (Không Nhiên Liệu)

> Đây là test cuối cùng trước khi đưa nhiên liệu vào

### 4.1 Điều Kiện Dry Start

Dry start tự động kích hoạt khi **một trong hai điều kiện**:
1. Thermocouple bị OPEN (ngắt kết nối)
2. Lệnh `drystart` (nếu được implement)

**Trong dry start, firmware TỰ ĐỘNG**:
- Chạy starter motor
- GIỮ OFF tất cả: Pump, Valve 1, Valve 2, Igniter
- Theo dõi và báo cáo RPM

### 4.2 Quy Trình Dry Start

```
1. Tháo cắm thermocouple hoặc để EGT ở trạng thái OPEN
2. Gõ: startidle
3. Quan sát Serial Monitor:
```

Kỳ vọng:
```
DRY START mode: EGT sensor OPEN
Starter: ON
Pump: OFF (safety lock - no EGT)
Valve 1: OFF
Valve 2: OFF  
Ignition: OFF
RPM: xxx (tăng dần theo starter)
```

**Quan sát trong 30 giây dry start**:

| Chỉ số | Chuẩn Pass |
|--------|-----------|
| Starter chạy | Motor quay nghe rõ |
| Pump hoàn toàn OFF | Không có tín hiệu pump ESC |
| RPM tăng dần | Theo starter throttle |
| `rejectPct` | < 20% (nhiễu từ starter) |
| Không lửa, không nhiên liệu | Kiểm tra bằng mắt |

### 4.3 Dừng Dry Start

```
soft stop
```
hoặc nhấn button vật lý.

**Sau stop, quan sát cooldown**:
- Starter tiếp tục chạy ~10–15 giây (cooldown airflow)
- Sau đó tất cả off, trở về WAITING

---

## 📋 GIAI ĐOẠN 5: WET START PREPARATION (Với Nhiên Liệu)

> **Chỉ thực hiện sau khi Giai Đoạn 0–4 đều PASS**

### 5.1 Kiểm Tra Nhiên Liệu

```
☐ Loại nhiên liệu: Kerosene / Jet-A phù hợp
☐ Bình nhiên liệu được cố định chắc chắn
☐ Đường ống không bị gấp khúc
☐ Pump ESC được test ở mức thấp nhất: 1160µs (50ml/min)
☐ Kiểm tra không có rò rỉ nhiên liệu ở tất cả khớp nối
```

### 5.2 Kiểm Tra Hệ Thống Đánh Lửa

```
☐ Glow plug được kiểm tra (đo điện trở: 0.5–2 Ohm)
☐ Driver glow plug hoạt động (test ign ngắn 2s, cẩn thận nóng)
☐ Igniter spark (nếu có spark plug) hoạt động
```

### 5.3 Bảng Hiệu Chuẩn Pump (Từ Đo Thực Tế)

| ESC (µs) | Lưu Lượng (ml/phút) | Ghi Chú |
|----------|---------------------|---------|
| 1000 | 0 | Pump tắt hoàn toàn |
| 1160 | 50 | Mức tối thiểu |
| 1175 | 80 | |
| 1250 | 265 | |
| 1260 | 280 | |
| 1265 | 360 | |
| 1270 | 560 | |
| 1300 | 600 | Mức tối đa |

*Nguồn: Đo thực tế từ file Luu_Luong_Bom_Thuc_Te.txt — khớp với bảng trong firmware.*

### 5.4 Cài Đặt Tham Số Trước Wet Start

Truy cập Web UI → Tab "Config", kiểm tra:

| Tham số | Giá trị Khuyến Nghị | Lý Do |
|---------|--------------------|-|
| `maxEgtC` | 700°C | Giới hạn an toàn EGT |
| `targetRpm` | RPM idle của engine | |
| `accelToIdleTimeoutS` | 20s | Timeout an toàn |
| `minRpmForFlameout` | > 0 | Phát hiện flameout |
| `rpmMinPulseUs` | 120–300µs | Tùy theo noise test |

---

## 📋 MA TRẬN QUYẾT ĐỊNH GO/NO-GO

### Trước Mỗi Lần Start Engine Thực:

| Hạng mục | Kết quả | GO | NO-GO |
|----------|---------|-----|-------|
| EGT đọc nhiệt độ phòng (15–35°C) | | ✅ | ❌ |
| RPM = 0 khi không quay | | ✅ | ❌ |
| `rejectPct` < 10% khi quay tay | | ✅ | ❌ |
| 9/9 Test Wizard PASS | | ✅ | ❌ |
| Dry Start hoàn thành không lỗi | | ✅ | ❌ |
| Không có rò rỉ nhiên liệu | | ✅ | ❌ |
| Pump test OK ở 1160µs | | ✅ | ❌ |
| Glow plug OK (resistance test) | | ✅ | ❌ |
| SD Card ghi log được | | ✅ | ⚠️ Optional |
| Không có ABORT chưa được giải thích | | ✅ | ❌ |

**Quy tắc GO**: Tất cả ✅ trước khi nhấn START.

---

## 📋 QUY TRÌNH XỬ LÝ LỖI PHỔ BIẾN

### Lỗi: "RPM nhiễu cao (rejectPct > 30%)"
```
1. Gõ: set rpmfilter 300
2. Lặp lại test nhiễu
3. Nếu vẫn fail: kiểm tra wiring, thêm tụ lọc 100nF tại GPIO33
4. Nếu vẫn fail: tăng filter lên 500µs
```

### Lỗi: "EGT không đọc được (NaN/OPEN)"
```
1. Kiểm tra thermocouple loại K
2. Kiểm tra kết nối MAX31855: CLK=18, CS=5, DO=19
3. Đo điện áp 3.3V tại chân VCC của MAX31855
4. Thử swap thermocouple khác
```

### Lỗi: "Pump ESC không phản hồi"
```
1. Kiểm tra ESC được arming (thấy beep khi cấp điện)
2. Đảm bảo ESC nhận tín hiệu PWM 1000µs khi startup
3. Kiểm tra GPIO26 → ESC signal wire
4. Đo PWM signal bằng oscilloscope hoặc servo tester
```

### Lỗi: "Starter chạy nhưng RPM không tăng"
```
1. Kiểm tra Hall sensor: vị trí, khoảng cách từ magnet
2. Kiểm tra magnet trong spinner nut còn nguyên
3. Gõ: rpmdetail on — xem raw edges có tăng không
4. Kiểm tra nguồn 5V cho Hall sensor
```

### Lỗi: "Stage2 không bắt đầu (hết timeout)"
```
1. Kiểm tra RPM đã đạt ngưỡng prove trong thời gian quy định
2. Tăng timeout nếu starter yếu: set prove_timeout 3000
3. Kiểm tra starter ESC cấu hình đúng
```

---

## 📋 LỆNH SERIAL MONITOR CẦN BIẾT

| Lệnh | Chức năng |
|------|-----------|
| `status` | In trạng thái hiện tại (EGT, RPM, Mode) |
| `rpmdetail on` | Bật chi tiết RPM (edges, rejected, quality) |
| `rpmdetail off` | Tắt chi tiết RPM |
| `rpmreset` | Reset counters RPM |
| `set rpmfilter 120` | Đặt filter pulse (µs) |
| `set rpmedge rising` | Dùng rising edge cho Hall sensor |
| `set rpmedge falling` | Dùng falling edge |
| `startidle` | Bắt đầu start sequence |
| `soft stop` | Dừng engine an toàn |
| `off` | Emergency stop (KHÔNG dùng khi EGT nóng) |
| `test ign on/off` | Test igniter/glow plug |
| `test starter <pct>` | Test starter (0–100%) |
| `pumptest <us>` | Test pump tại giá trị µs cụ thể |

---

## 📝 Nhật Ký Test (Điền Vào Khi Test)

```
Ngày test: _______________
Location: _______________
Firmware: ECU_TestV1_EGT_DRY_START_PATCH
SD Log file: ECU___.CSV

Nhiệt độ phòng: ___°C
Độ ẩm: ___%

Kết quả Giai Đoạn 0 (Kiểm tra vật lý): PASS / FAIL
Kết quả Giai Đoạn 1 (Khởi động): PASS / FAIL
Kết quả Giai Đoạn 2 (Check nhiễu):
  - rpmMinPulseUs cuối cùng: ___µs
  - rejectPct khi quay tay: ___%
  - rejectPct khi glow bật: ___%
  - Kết luận: PASS / FAIL

Kết quả Giai Đoạn 3 (Test Wizard): PASS / FAIL
Kết quả Giai Đoạn 4 (Dry Start): PASS / FAIL
GO / NO-GO cho wet start: ___

Ghi chú:
_______________________________________________
_______________________________________________
```

---

**Tài liệu này nên được in ra và điền vào trong quá trình test thực tế.**  
**Phiên bản**: 1.0 — 2026-07-16
