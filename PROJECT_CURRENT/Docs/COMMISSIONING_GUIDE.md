# Hướng Dẫn Toàn Diện: Upload, Hiệu Chỉnh & Commissioning ECU

**Firmware**: `ECU_TestV1_EGT_DRY_START_PATCH.ino`
**Board**: ESP32 DevKit V1 / NodeMCU-32S
**Đối tượng**: Từ upload firmware lần đầu tới chạy thật có tăng throttle, đi từng bước an toàn.

> Nguyên tắc xuyên suốt: **không nhảy cóc bước**. Mỗi giai đoạn dưới phải
> PASS hết rồi mới sang giai đoạn kế tiếp. Nếu bất kỳ bước nào FAIL — dừng,
> tìm nguyên nhân, không ép qua bằng debug override.
>
> Tài liệu này gộp toàn bộ các hướng dẫn trước đây
> (Upload & Test, Pre-Engine Test, DSO152 RPM Calibration, Commissioning)
> thành **1 file duy nhất** để không phải lật qua nhiều tài liệu.

---

## ⚠️ Cảnh Báo An Toàn Chung

- Luôn test trong không gian thoáng khí
- Tháo nhiên liệu ra khỏi hệ thống khi test điện tử/component
- Giữ khoảng cách an toàn khi động cơ/starter quay
- Có người thứ 2 giám sát khi test engine thực có nhiên liệu
- Chuẩn bị bình chữa cháy **CO2** gần tầm tay (không dùng bột khô)
- Kill switch vật lý (NC) phải lắp và test cơ học độc lập — không dựa vào firmware

---

## Yêu Cầu Trước Khi Bắt Đầu

### Hardware
- [ ] ESP32 DevKit V1 hoặc NodeMCU-32S
- [ ] Cáp USB (USB-A to Micro-USB)
- [ ] Máy tính có Arduino IDE
- [ ] MAX31855 Thermocouple Module + Thermocouple loại K
- [ ] RPM Sensor (KMZ10A AMR + mạch khuếch đại/so sánh — xem Giai đoạn 2)
- [ ] 2× ESC (Pump & Starter)
- [ ] 2× Solenoid Valve (Start + Main, xem `CODE_REVIEW_FINDINGS.md`)
- [ ] Glow plug/igniter + driver
- [ ] Micro SD Card (FAT32 format)
- [ ] Nguồn cấp đủ dòng cho starter/pump/glow (xem phần Nguồn & Pin bên dưới)

### Software
- [ ] Arduino IDE 1.8.19+ hoặc 2.x
- [ ] ESP32 Board Support Package
- [ ] Library: **Adafruit MAX31855**, **ESP32Servo**
  (không cần ArduinoJson — firmware hiện tại không dùng thư viện này)

---

## Cài Đặt Arduino IDE & Upload Firmware

### Bước 1: Cài Arduino IDE
1. Tải từ https://www.arduino.cc/en/software, cài đặt theo hướng dẫn

### Bước 2: Thêm ESP32 Board
1. **File → Preferences** → thêm URL Board Manager:
   `https://dl.espressif.com/dl/package_esp32_index.json`
2. **Tools → Board → Board Manager** → tìm "ESP32" (tác giả Espressif Systems) → cài
3. Chọn board: **ESP32 Dev Module**

### Bước 3: Cài Library
**Sketch → Include Library → Manage Libraries** — tìm và cài:

| Library | Tác giả |
|---------|---------|
| Adafruit MAX31855 | Adafruit Industries |
| ESP32Servo | John K. Bennett |

### Bước 4: Cấu Hình Board & Upload
```
Board:           ESP32 Dev Module
Upload Speed:    115200
Flash Size:      4MB
Port:            COM3 (Windows) hoặc /dev/ttyUSB0 (Linux)
```
Mở `PROJECT_CURRENT/Firmware/ECU_TestV1_EGT_DRY_START_PATCH/ECU_TestV1_EGT_DRY_START_PATCH.ino`
→ Click **Upload** (hoặc Ctrl+U) → chờ "Done uploading" (~30-60s).

**Nếu upload thất bại** ("Failed to connect to ESP32: Timed out"):
1. Giữ nút **Boot** trên board trong lúc upload, thả ra khi thấy "Connecting..."
2. Thử lại, hoặc giảm Upload Speed xuống 9600 để chẩn đoán
3. Kiểm tra driver CH340 đã cài (Windows): http://www.wch.cn/download/ch341ser_exe.zip

### Bước 5: Mở Serial Monitor
**Tools → Serial Monitor** (Ctrl+Shift+M): Baud **115200**, Line ending **Newline**.

Output kỳ vọng ngay sau boot:
```
Test ECU V1 WebUI/TestWizard booted.
MAX31855 begin() = OK (hoặc CHECK_WIRING)
RPM edge = RISING
SD_LOG: OK file=/ECU000.CSV (hoặc FAIL file=...)
Outputs safe. Auto-start disabled. Type help.
```

**Nếu Serial Monitor trống**: kiểm tra baudrate = 115200, upload lại sketch, nhấn nút Reset trên board.

---

## Sơ Đồ Đấu Nối GPIO (Pinout)

```
MAX31855 (EGT):    CLK=18   CS=5    DO=19
RPM sensor:        Signal=33
Pump ESC:          Signal=26 (PWM)
Starter ESC:       Signal=25 (PWM)
Valve 1 (Start):   17 (digital)
Valve 2 (Main):    16 (digital)
Ignition/Glow:     32 (digital)
User Button:       22 (digital, active-LOW, pull-up nội bộ)
Status LED:        2 (onboard, active-LOW)
SD Card (SPI):     CS=13  SCK=14  MOSI=23  MISO=27
```

**Kết nối tối thiểu để test không cần động cơ**: ESP32 + MAX31855 + thermocouple
+ RPM sensor + SD card. Không cần ESC/valve/glow để test qua Web UI cơ bản.

---

## Nguồn & Pin — Lưu Ý Bắt Buộc

- **Không cắm thẳng pin LiPo (2S/3S) vào rail 5V logic của ESP32/mạch RPM sensor**
  — rail này cần nguồn **đã ổn áp qua UBEC 5V** riêng, không phải điện áp pin thô.
- **Starter/Pump ESC**: lấy điện trực tiếp từ pin chính — kiểm tra ESC ghi rõ
  dải điện áp hỗ trợ (ví dụ "2S-4S") trước khi chọn pin.
- **Van solenoid + glow plug**: nếu ghi định mức thấp (ví dụ "DC:6V"), phải
  cấp qua UBEC riêng phù hợp dòng (glow plug thường kéo dòng lớn, 3-8A — cần
  UBEC công suất cao hơn UBEC nhỏ dùng cho ESP32/logic).
- **Chọn dòng xả pin (C-rating)** đủ cho tải đỉnh lúc starter cranking + glow
  + bơm chạy cùng lúc — ước tính và kiểm tra thực tế bằng cách đo sụt áp khi
  `test starter_ign` chạy.
- **Nguồn dòng thấp (ví dụ adapter 12V 2A) KHÔNG đủ chạy starter thật** —
  motor sẽ khựng/giật giật/dừng dù firmware vẫn gửi PWM đều đặn (đây là
  triệu chứng thiếu dòng cấp nguồn, không phải lỗi firmware). Dùng ATX PSU
  hoặc bench PSU chịu dòng ≥15-20A, hoặc pin LiPo phù hợp để test thật.

---

## Giai đoạn 0 — Chuẩn bị trước khi cấp điện

### Checklist chung
- [ ] Kill switch vật lý (NC) đã lắp và test cơ học (không dựa vào firmware)
- [ ] Bình lửa CO2 trong tầm với
- [ ] Bàn test cố định động cơ chắc chắn, không ai đứng ngay trục quay
- [ ] Chưa gắn ống dẫn nhiên liệu vào engine thật (test bench trước)
- [ ] Thẻ SD đã format FAT32, gắn vào ECU
- [ ] Firmware mới nhất đã nạp qua Arduino IDE

### Checklist đấu nối vật lý (kiểm tra bằng mắt/đồng hồ trước khi cấp điện)

| Điểm kiểm tra | Yêu cầu |
|---|---|
| GPIO18/5/19 → MAX31855 CLK/CS/DO | Đúng thứ tự, không ngược |
| Thermocouple | Loại K, không phải J hoặc T |
| GPIO33 → RPM sensor | Qua chuỗi khuếch đại/so sánh (xem Giai đoạn 2), không nối thẳng KMZ10A |
| GPIO26/25 → Pump/Starter ESC | Dây tín hiệu PWM, không phải dây nguồn |
| GPIO17/16 → Valve 1/2 | Qua MOSFET/relay driver, không nối thẳng cuộn solenoid |
| GPIO32 → Ignition/Glow | Qua MOSFET driver phù hợp dòng glow plug |
| GPIO22 → User Button | Pull-up, active LOW |
| SD: CS=13 SCK=14 MOSI=23 MISO=27 | Đúng pinout |
| Nguồn ESP32 | 5V ±0.2V qua UBEC riêng, tách biệt nguồn ESC (xem phần Nguồn & Pin) |
| Không ngắn mạch | Đo điện trở GND-VCC khi CHƯA cấp điện |

---

## Giai đoạn 1 — Kiểm tra tĩnh (không cấp nhiên liệu, không quay starter)

Kết nối Serial Monitor 115200 baud hoặc mở Web UI `192.168.4.1`
(WiFi: **ECU_TestV1**, pass: **admin1234**).

| Bước | Lệnh | Kỳ vọng |
|------|------|---------|
| 1.1 | `status` | ECU in ra MODE=WAITING, không lỗi |
| 1.2 | `showcfg` | Xem toàn bộ config mặc định, ghi lại để đối chiếu sau |
| 1.3 | `test egt` | EGT=OK và nhiệt độ phòng (~20-30°C). Nếu FAULT → kiểm tra dây MAX31855 trước khi đi tiếp |
| 1.4 | `test rpm_noise` | RNOISE=CLEAN hoặc NO_SIGNAL (không quay gì cả → NO_SIGNAL là bình thường) |
| 1.5 | `sdstatus` rồi `sdtest` | SD=OK, file ECU0xx.CSV được tạo, event ghi được |

**PASS điều kiện**: cả 5 mục trên không có lỗi. Đây là bước bắt buộc trước
khi chạm vào bất kỳ thứ gì có chuyển động.

**Web UI**: mở dashboard, kiểm tra hiển thị EGT/RPM/Mode=WAITING, Event Log
có dòng boot, Test Wizard tab hiện đủ 9 mục `NOT_RUN`.

---

## Giai đoạn 2 — Hiệu Chỉnh & Xác Nhận Cảm Biến RPM (KMZ10A)

> KMZ10A **không phải Hall sensor thông thường** — đây là cảm biến
> **từ trở anisotropic (AMR)** dạng cầu Wheatstone, output analog vi sai
> mV, cần qua toàn bộ chuỗi khuếch đại + so sánh trước khi thành xung
> digital cho ESP32. Giai đoạn này gồm 2 phần: **A** (oscilloscope DSO152,
> chỉnh phần cứng analog) và **B** (firmware bench, quét toàn dải PWM).

### Sơ Đồ Signal Chain

```
KMZ10A (U17, header 4 chân)
  Pin 1: +V0 → SENSOR_P [TP: U13]   Pin 3: −V0 → SENSOR_N [TP: U12]
  Pin 4: VCC → +5V                   Pin 2: GND → GND
        │ R41/R42 (100Ω) + C19 (1nF lọc vi sai)
        ▼
   INA826 U20 — Gain = 1 + 100k/2.7k ≈ 38×, REF=GND, V+ pin6=VREF(2.5V)
        │ INA_OUT [TP: U3]
        ▼
   C15(10µF) + R35(1M) + R37(4K7) — AC coupling + LPF
        ▼
   LMV358 U18 Op-Amp 1 ← RP1 100K (OFFSET)
        ▼
   LMV358 U18 Op-Amp 2 ← RP2 100K (GAIN)
        │ LMV358_OUT [TP: U1]
        ▼
   R32(10K) + C2(4.7nF LPF)
        ▼
   LMV393 U19 Comparator (Schmitt, hysteresis R36 470K)
   IN− = VTH_RPM [TP: U21] ← RP3 10K (THRESHOLD)
        │ RPM_OUT [TP: U2] ← ĐIỂM ĐO QUAN TRỌNG NHẤT
        ▼
   J_RPM_OUT1 → cáp ngoài → RPM_PIN_IN1
        │ R10(10K pull-up 3.3V) + R2(1K series) + D1(TVS 3.3V)
        ▼
   GPIO33 (ESP32 NodeMCU-32S)
```

### Bản Đồ Testpoint

| Ký hiệu | Tên | Loại tín hiệu | Dùng cho |
|---------|-----|--------------|---------|
| U15 | TP_GND | 0V reference | Kẹp GND probe suốt quá trình đo |
| U16 | TP_5V | DC +5V | Kiểm tra nguồn mạch analog |
| U14 | TP_VREF | DC ~2.5V | Kiểm tra mid-rail |
| U13/U12 | TP_SENSOR_P/N | Analog mV | Output cầu KMZ10A |
| U3 | TP_INA_OUT | Analog ×38 | Output INA826 |
| U1 | TP_LMV358_OUT | Analog trimmed | Trước comparator |
| U21 | TP_VTH_RPM | DC 0–5V | Ngưỡng comparator (chỉnh RP3) |
| U2 | **TP_RPM_OUT** | **Digital 0/5V** | **Xung RPM cuối — quan trọng nhất** |
| GPIO33 | Trực tiếp | Digital 0/3.3V | Vào ESP32 |

### 3 Trimpot

| Trimpot | Giá trị | Chức năng |
|---------|---------|----------|
| **RP1** | 100K | OFFSET — cân bằng DC tín hiệu |
| **RP2** | 100K | GAIN — biên độ trước comparator |
| **RP3** | 10K | THRESHOLD — ngưỡng kích comparator |

### Cài Đặt DSO152

| Bước | Probe tại | Coupling | Volt/div | Time/div | Trigger |
|------|-----------|----------|----------|----------|---------|
| A1a | TP_5V (U16) | DC | 2V | 10ms | Auto |
| A1b | TP_VREF (U14) | DC | 1V | 10ms | Auto |
| A2 | TP_INA_OUT (U3) | AC | 500mV | 2ms | Auto |
| A3/A4 | TP_LMV358_OUT (U1) | DC | 1V | 2ms | Auto |
| A5 | TP_RPM_OUT (U2) | DC | 2V | 2ms | Rising |
| A-cuối | GPIO33 | DC | 2V | 2ms | Rising |

> GND probe **luôn cắm vào U15 (TP_GND)**.

### GIAI ĐOẠN A — Hiệu Chỉnh Analog Bằng Oscilloscope

**A1 — Kiểm tra nguồn** (không cần quay magnet)
- U16: DC phẳng +5.0V ±0.2V. U14: DC ổn định 2.4-2.6V.
- Sai → kiểm tra BEC/hàn R40/R39/C23/C1 trước khi tiếp tục.

**A2 — Xem tín hiệu INA826 (U3)**
- Quay magnet ~1-3 vòng/giây, AC coupling.
- Kỳ vọng: sóng sin/quasi-sin, biên độ 200mV-1Vpp.
- Không thấy gì → kiểm tra khoảng cách magnet-sensor (≤3mm), thử xoay KMZ10A 90°.

**A3 — Chỉnh RP1 (Offset) tại U1**
- DC coupling, quay magnet đều ~2 vòng/giây, vặn RP1 cho đỉnh+/đỉnh− **cách đều 2.5V**.

**A4 — Chỉnh RP2 (Gain) tại U1**
- Vặn RP2 đạt biên độ **1-2Vpp**, không clipping (đỉnh phẳng → giảm RP2).

**A5 — Chỉnh RP3 (Threshold) tại U2, xác nhận xung RPM_OUT**
- Mục tiêu: xung vuông sạch 0/5V, đúng 1 xung/cực magnet.
- RP3 quá thấp → double-pulse (tăng RP3). RP3 quá cao → mất xung (giảm RP3).
- **Margin**: tăng RP3 tới khi xung vừa mất → lùi lại 1/4 vòng → điểm làm việc.
- **Test EMI**: bật starter ESC ~20%, xem RPM_OUT có spike lạ không.
- Đọc VTH_RPM tại U21, ghi lại điện áp ngưỡng tại điểm làm việc.

**A-cuối — Xác nhận GPIO33**: cắm J_RPM_OUT1→RPM_PIN_IN1, probe GPIO33,
kỳ vọng xung 0-3.3V (D1 TVS clamp). Nếu vẫn thấy 5V → D1 lỗi, thay ngay
trước khi cấp nguồn ESP32.

### GIAI ĐOẠN B — Quét Toàn Dải PWM Bằng Firmware TEST_STARTER

Dùng `TEST/TEST_STARTER/TEST_STARTER.ino` (firmware bench riêng, không
cần ARM, không bảo vệ) để quét **toàn dải PWM starter thật**, thay vì chỉ
quan sát bằng tay quay chậm trên oscilloscope.

**Đấu nối**: Starter ESC signal → GPIO25, RPM sensor → GPIO33 (giống mạch chính).

**⚠️ An toàn**: THÁO cánh/impeller khỏi starter trước khi test. Nguồn ESC
tách riêng nguồn ESP32, đủ dòng (xem phần Nguồn & Pin).

**Upload**: mở `TEST/TEST_STARTER/TEST_STARTER.ino`, board ESP32 Dev Module,
cần lib ESP32Servo, Serial Monitor 115200/Newline.

**Bảng lệnh**:

| Lệnh | Chức năng |
|------|-----------|
| `+` / `-` | Tăng/giảm PWM starter 1 bước (mặc định 10µs) |
| `0` hoặc `s` | Dừng starter (PWM về 1000µs) |
| `step <us>` | Đổi bước tăng/giảm (1..200) |
| `pwm <us>` | Đặt PWM trực tiếp (1000..2000) |
| `ppr <n>` | Số xung/vòng (1=nam châm, 2=quang học) |
| `filter <us>` | Bộ lọc glitch RPM (20..2000), mặc định 120 |
| `edge rising\|falling` | Cạnh kích RPM |
| `reset` | Xóa bộ đếm & lịch sử RPM |
| `status` / `help` | In trạng thái / menu lệnh |

**Đọc dòng trạng thái** (in 2 lần/giây):
```
PWM=1200us | RPM=8450 (win 8410) | raw=141 acc=141 rej=0 (0.0%) | jit=4.2% cv=1.1% | NOISE=CLEAN STAB=STABLE
```

| Trường | Ý nghĩa |
|--------|---------|
| `PWM` | Mức PWM hiện tại. `(OFF)` khi =1000µs |
| `RPM` / `win` | RPM tức thời / trung bình cửa sổ 100ms |
| `raw`/`acc`/`rej` | Cạnh thô / chấp nhận / bị lọc |
| `jit` | Jitter chu kỳ (max-min)/avg |
| `cv` | Hệ số biến thiên RPM (stddev/mean) |
| `NOISE`/`STAB` | Đánh giá nhiễu / độ ổn định tự động |

**Ngưỡng NOISE**: `CLEAN` (reject≤5%, jit≤15%) · `WARN` (reject>5% hoặc jit>15%)
· `NOISY` (reject>20% hoặc jit>40%) · `NO_SIGNAL` (không cạnh nào).

**Ngưỡng STAB** (sau khi PWM ổn định 1.5s): `STABLE` (CV≤2%) · `WARN` (2-5%)
· `UNSTABLE` (>5%) · `SETTLING` (mới đổi PWM) · `OFF` (PWM=1000µs).

**Quy trình quét**:
1. Test tĩnh (PWM=OFF) 30s: kỳ vọng `raw≈0`, `NOISE=NO_SIGNAL`. Có `acc>0` → nhiễu nền, quay lại A5.
2. Rà quét `+` từng bước nhỏ (vd +10µs từ 1100µs), chờ `STAB` khỏi `SETTLING` rồi ghi `NOISE`/`STAB`.
3. Ghi bảng PWM→RPM→NOISE→STAB (mẫu bên dưới).
4. `NOISY` ở PWM cao → tăng `filter 300`, thêm tụ 100nF tại GPIO33, tách dây RPM xa dây ESC. Vẫn không hết → quay lại A5 tăng margin RP3.
5. **GO/NO-GO**: toàn dải phải đạt `NOISE=CLEAN` và `STAB=STABLE` trước khi chuyển sang firmware ECU chính.

**Bảng ghi kết quả quét** (điền khi test):

| PWM (µs) | RPM | raw | acc | rej% | jit% | cv% | NOISE | STAB |
|----------|-----|-----|-----|------|------|-----|-------|------|
| 1000 (OFF) | | | | | | | | |
| 1100 | | | | | | | | |
| 1150 | | | | | | | | |
| 1200 | | | | | | | | |
| 1250 | | | | | | | | |

**Xác nhận cuối** (trên firmware ECU chính, không phải TEST_STARTER):
```
rpmdetail on
```
Quay tay đều ~1 vòng/giây → Serial phải hiện RPM ≈ 60 (ppr=1), `NOISE=CLEAN`,
`rej%` < 5%.

---

## Giai đoạn 3 — Test từng bộ phận (Test Wizard, KHÔNG nhiên liệu)

Tất cả lệnh dưới cần `arm2` trước (khóa tự nhả sau 10 giây, phải làm ngay
sau khi arm).

```
arm2
test ign          -> glow plug bật 1s, quan sát dòng điện/nhiệt bằng tay (cẩn thận nóng)
arm2
test starter      -> starter quay 3s ở us cấu hình, xem RPM có tăng lên không
arm2
test starter_ign  -> starter + glow cùng lúc, kiểm tra EMI có làm nhiễu RPM không (resetRpmStats() trước bước này đã fix trong CODE_REVIEW_FINDINGS.md)
arm2
test valve1        -> Valve 1 = Start solenoid valve
arm2
test valve2        -> Valve 2 = Main oil valve
```

Với mỗi test, dùng `checklist` để xem kết quả — tất cả phải chuyển từ
`NOT_RUN` → `PASS`. Nếu `FAIL` — sửa phần cứng trước khi tiếp tục (đừng
chỉnh firmware để né test).

**Component test khác (tùy chọn, bench-only)**:
```
starttest <us> <ms>   -> vd: starttest 1100 3000
ignpulse <ms>          -> vd: ignpulse 1500
valve1 on/off | valve2 on/off
```

---

## Giai đoạn 4 — Test bơm nhiên liệu riêng (KHÔNG gắn ống vào engine)

⚠️ Tháo ống dẫn nhiên liệu ra khỏi engine, xả vào cốc hứng riêng.

```
arm2
pumptest 1100 1500   -> bơm chạy 1500ms ở 1100us, đo ml thực tế đối chiếu bảng calib
```

**Bảng hiệu chuẩn pump** (đo thực tế, khớp firmware):

| ESC (µs) | Lưu lượng (ml/phút) |
|----------|---------------------|
| 1000 | 0 (tắt) |
| 1160 | 50 (tối thiểu) |
| 1175 | 80 |
| 1250 | 265 |
| 1260 | 280 |
| 1265 | 360 |
| 1270 | 560 |
| 1300 | 600 (tối đa) |

Lặp lại 2-3 mức us khác nhau để xác nhận độ tuyến tính. Đây là bước cuối
để pass `TEST_PUMP` trong checklist:
```
test pump
```

Sau khi xong, `confirmkill` để xác nhận kill-switch vật lý đã test cơ học:
```
confirmkill
```

**Kiểm tra checklist tổng**:
```
checklist
```
→ `STARTIDLE_CHECK=PASS: OK` (không phải OK_DRY_START_EGT_BYPASS, vì đó là
chế độ né checklist khi cảm biến EGT hỏng — chỉ dùng khi cố ý test khô).

---

## Giai đoạn 5 — Dry-start (quay engine thật, KHÔNG nhiên liệu/lửa)

Mục đích: xác nhận starter đủ mạnh quay trục thật + RPM sensor đọc đúng khi
lắp lên engine, trước khi cho nổ.

Cách kích hoạt dry-start: rút giắc/để hở mạch cảm biến EGT tạm thời (hoặc
dùng `set egtstart dry` để cho phép), lúc đó `startidle` sẽ tự động thành
dry-run (không nhiên liệu/van/mồi lửa vì `dryStartActive=true`). **Không có
lệnh `drystart` riêng** — đây là hành vi tự động của `startidle` khi EGT lỗi.

```
set egtstart dry
arm2
autostart on
arm2
startidle
```

Quan sát: starter quay đúng `dryStartRunMs`, RPM tăng theo, không có mùi
nhiên liệu, không có tia lửa. Dùng `rpmdetail on` để xem RPM có track đúng
tốc độ quay thật không.

Xong bước này, trả cảm biến EGT về đúng dây và:
```
set egtstart strict
```
để khóa lại — từ giờ mọi lần EGT lỗi sẽ chặn start thay vì lặng lẽ dry-run.

**Dừng dry-start**: `stop` (soft stop, vào cooldown) hoặc nhấn button vật lý.

---

## Giai đoạn 6 — Wet-start (khởi động thật, có nhiên liệu + lửa)

**Chỉ làm khi**: Giai đoạn 0-5 đều PASS, EGT gắn đúng và đọc chính xác,
engine cố định chắc chắn, có người đứng cạnh kill switch, có bình chữa cháy.

```
arm2
autostart on
arm2
startidle
```

Firmware tự chạy chuỗi:
`PURGE → SPINUP_PREHEAT (glow) → INTRO_FUEL (đánh lửa) → POST_IGNITION_HEAT
→ ACCEL_TO_IDLE → IDLING`

Theo dõi bằng `status` liên tục (auto in mỗi ~1s) hoặc Web UI:
- `STAGE=` chuyển đúng thứ tự trên
- `EGT=` tăng dần khi có lửa
- `RPM=` tăng dần đến gần idleRpm

Nếu bất kỳ đâu bị **ABORT** (OVER_TEMP, NO_IGNITION, NO_RPM_RISE,
RPM_SIGNAL_LOST, OVERSPEED, COMM_TIMEOUT...) — **đọc kỹ lý do trên Serial
trước khi `clearabort`**. Không type `clearabort` theo phản xạ.

### ⚠️ Xả nhiên liệu tồn dư trước khi thử start lại (sau NO_IGNITION / ACCEL_TO_IDLE_TIMEOUT)

Theo manual EnJet E80/E100 (`REFERENCES/Documentation/Engine_Manual_E80_E100.pdf`):
nếu lần start trước bị fail sau khi đã phun dầu nhưng không bắt lửa,
nhiên liệu chưa cháy **vẫn còn đọng lại trong buồng đốt**. Cố start lại
ngay có thể gây cháy lớn khi lượng dầu tồn dư này bắt lửa đột ngột cùng
lúc với lần mồi mới.

**Quy trình xả trước khi start lại**:
1. Nghiêng động cơ, **đuôi (exhaust) hướng xuống dưới**
2. Dùng `starttest <us> <ms>` để quay rotor vài giây, không phun thêm
   nhiên liệu — thổi bay dầu tồn đọng ra khỏi buồng đốt qua đuôi
3. Sau đó mới `clearabort` và thử `startidle` lại

Bỏ qua bước này nếu lần fail là do abort SỚM (`NO_STARTER_RPM`, `OVER_TEMP`
trước khi có nhiên liệu) — chỉ áp dụng khi đã ở stage `ST_INTRO_FUEL` trở
lên (đã phun dầu) mà chưa đánh lửa thành công.

Khi vào `MODE_IDLING` ổn định vài chục giây, không dao động RPM bất
thường → **giai đoạn wet-start thành công**.

---

## Giai đoạn 7 — Tăng throttle (chạy thật có tải)

Chỉ làm khi IDLING đã ổn định. Lệnh throttle:

```
set throttle <0..100>
```

Cơ chế: `targetRpm = idleRpm + (maxRpm - idleRpm) * throttlePct / 100`,
ECU tự động chuyển `MODE_IDLING → MODE_OPERATING` khi `throttlePct > 0`
và closed-loop điều khiển bơm để đạt RPM mục tiêu.

**Quy trình tăng dần, không nhảy vọt**:

| Bước | Lệnh | Giữ tối thiểu | Quan sát |
|------|------|--------------|---------|
| 7.1 | `set throttle 10` | 15-30s | RPM tăng nhẹ, EGT tăng từ từ, không dao động |
| 7.2 | `set throttle 25` | 15-30s | RPM ổn định ở target mới, EGT < maxEgtC |
| 7.3 | `set throttle 50` | 15-30s | Kiểm tra `rpmdetail` — jitter thấp, không NOISY |
| 7.4 | `set throttle 75` | 15-30s | EGT gradient theo dõi kỹ (dEGT) |
| 7.5 | `set throttle 100` | Chỉ khi đã quen động cơ | Full power — luôn sẵn sàng `stop` hoặc kill switch |

**Giảm ga về idle**: `set throttle 0` → tự động `MODE_OPERATING → MODE_IDLING`.

**Dừng máy an toàn (có làm mát)**: `stop` → vào `MODE_COOLDOWN`, starter
quay làm mát, tắt nhiên liệu/van, chờ `cooldownTargetC` rồi mới tắt hẳn.

**Dừng khẩn cấp**: nhấn kill switch vật lý (NC) — bypass hoàn toàn
firmware, ngắt điện trực tiếp. Lưu ý `set commwatchdog` sẽ tự abort nếu
mất kết nối Serial/Web quá `commTimeoutMs` lúc IDLING/OPERATING — xem
`CODE_REVIEW_FINDINGS.md`.

---

## Ma Trận Quyết Định GO/NO-GO (trước mỗi lần start engine thực)

| Hạng mục | GO | NO-GO |
|----------|-----|-------|
| EGT đọc nhiệt độ phòng (15-35°C) khi test tĩnh | ✅ | ❌ |
| RPM = 0 khi không quay (test tĩnh) | ✅ | ❌ |
| Giai đoạn 2B: toàn dải PWM đạt NOISE=CLEAN, STAB=STABLE | ✅ | ❌ |
| 9/9 Test Wizard PASS | ✅ | ❌ |
| Dry-start hoàn thành không lỗi | ✅ | ❌ |
| Không rò rỉ nhiên liệu, pump test khớp bảng calib | ✅ | ❌ |
| Kill switch đã confirmkill | ✅ | ❌ |
| SD card ghi log được | ✅ | ⚠️ Tùy chọn |
| Không có ABORT chưa được giải thích rõ | ✅ | ❌ |
| Nguồn/pin đủ dòng (đã đo sụt áp lúc test starter_ign) | ✅ | ❌ |

**Quy tắc GO**: tất cả ✅ trước khi nhấn `startidle` cho wet-start thật.

---

## Xử Lý Lỗi Thường Gặp

**RPM nhiễu cao (rejectPct > 20-30%)**
```
set rpmfilter 300
```
Lặp lại Giai đoạn 2B. Vẫn fail → kiểm tra wiring, thêm tụ 100nF tại GPIO33,
tách dây RPM xa dây công suất ESC. Vẫn fail → quay lại Giai đoạn 2A tăng
margin RP3.

**EGT không đọc được (FAULT/NaN)**
1. Kiểm tra thermocouple loại K, kết nối MAX31855 (CLK=18, CS=5, DO=19)
2. Đo điện áp cấp cho MAX31855
3. Thử thermocouple khác để loại trừ hỏng cảm biến

**Pump ESC không phản hồi**
1. Kiểm tra ESC arming (nghe beep khi cấp điện)
2. Đảm bảo nhận đúng tín hiệu PWM tại GPIO26
3. Đo PWM bằng oscilloscope/servo tester nếu nghi ngờ

**Starter chạy nhưng RPM không tăng**
1. Kiểm tra vị trí/khoảng cách sensor-magnet
2. `rpmdetail on` — xem `raw` có tăng theo starter không
3. Kiểm tra nguồn 5V cho mạch RPM sensor (qua UBEC riêng)

**Starter khựng/giật giật rồi dừng dù firmware vẫn gửi PWM đều**
→ Đây là dấu hiệu **thiếu dòng cấp nguồn**, không phải lỗi firmware. Xem
phần "Nguồn & Pin" ở trên — đổi sang nguồn đủ dòng (ATX PSU, bench PSU,
hoặc pin LiPo đã tính margin phù hợp).

**Upload thất bại / không kết nối USB**
→ Xem phần "Cài Đặt Arduino IDE & Upload Firmware" ở trên.

---

## Nhật Ký Test (điền khi test thực tế)

```
Ngày test: _______________
Firmware: ECU_TestV1_EGT_DRY_START_PATCH
SD Log file: ECU___.CSV
Nhiệt độ phòng: ___°C

Giai đoạn 0 (đấu nối vật lý): PASS / FAIL
Giai đoạn 1 (test tĩnh): PASS / FAIL
Giai đoạn 2A (trimpot RP1/RP2/RP3): PASS / FAIL
  VTH_RPM tại điểm làm việc = _____ V
Giai đoạn 2B (quét PWM TEST_STARTER):
  rejectPct khi quay tay = ___%   NOISE cuối = _______
  Toàn dải CLEAN/STABLE? PASS / FAIL
Giai đoạn 3 (Test Wizard 9 bước): PASS / FAIL
Giai đoạn 4 (Pump test + confirmkill): PASS / FAIL
Giai đoạn 5 (Dry-start): PASS / FAIL
Giai đoạn 6 (Wet-start): PASS / FAIL
Giai đoạn 7 (Throttle tối đa đã test an toàn): ____%

GO / NO-GO cho lần chạy tiếp theo: _______

Ghi chú / Vấn đề gặp phải:
_______________________________________________
_______________________________________________
```

---

## Bảng Tóm Tắt Thứ Tự Toàn Bộ

```
0. Chuẩn bị an toàn + kiểm tra đấu nối vật lý
1. Test tĩnh: EGT, RPM_NOISE, SD                          → không quay gì
2. RPM sensor: 2A (trimpot DSO152) + 2B (quét PWM TEST_STARTER)
3. Test Wizard từng bộ phận (ign/starter/starter_ign/valve1/valve2)
4. Test bơm riêng (pumptest, không gắn engine) + confirmkill
5. Dry-start (quay thật, không nhiên liệu/lửa)
6. Wet-start (đánh lửa thật, có bước xả nhiên liệu tồn dư nếu fail) → IDLING ổn định
7. Tăng throttle từ 10% → 100% từng nấc, theo dõi EGT/RPM
```

---

## Lưu Ý An Toàn Xuyên Suốt

- **Không** dùng `set checklist off` khi test thật — chỉ dùng khi debug bench.
- **Không** dùng `clearabort force` trừ khi chắc chắn máy đã nguội và cảm
  biến EGT hỏng vĩnh viễn — lệnh này bỏ qua xác nhận nhiệt độ qua sensor.
- Sau **mọi ABORT**, đọc lý do (`lastAbortReason`) trước khi re-arm, đừng
  `clearabort` phản xạ.
- Sau `NO_IGNITION`/abort có phun dầu nhưng chưa cháy — xả nhiên liệu tồn
  dư (nghiêng đuôi xuống + quay rotor không tải) trước khi thử lại, xem
  Giai đoạn 6.
- Không gõ lệnh Serial chậm/ngắt quãng khi engine đang chạy thật.
- Không để SD đầy 1000 file log.
- Throttle luôn tăng/giảm từng nấc nhỏ, không set thẳng 100% từ IDLING.
- Nguồn/pin phải đủ dòng xả — nguồn yếu gây triệu chứng giống lỗi cơ khí
  (starter khựng) nhưng thực chất là thiếu dòng, xem phần "Nguồn & Pin".

---

**Tài liệu liên quan**:
- `PROJECT_CURRENT/Docs/CODE_ARCHITECTURE.md` — hiểu sâu kiến trúc firmware
- `PROJECT_CURRENT/Docs/CODE_REVIEW_FINDINGS.md` — các lỗi đã fix qua các vòng review, ảnh hưởng độ tin cậy các bước trên
- `REFERENCES/Documentation/Engine_Manual_E80_E100.pdf` — nguồn quy trình xả nhiên liệu tồn dư (Giai đoạn 6)
- `REFERENCES/Documentation/Engine_Manual_NewerModel.pdf` / `_Chinese.pdf` — manual EnJet G3/E86, nguồn valve1/valve2 role split
- `TEST/TEST_STARTER/TEST_STARTER.ino` — firmware bench dùng ở Giai đoạn 2B

**Phiên bản**: 3.0 — 2026-07-16 (gộp Upload/Test + Pre-Engine-Test + DSO152 + Commissioning thành 1 file)
