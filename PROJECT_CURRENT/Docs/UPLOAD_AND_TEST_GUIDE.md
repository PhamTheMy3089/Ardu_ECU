# 🚀 Hướng Dẫn Upload & Test Firmware Trên ESP32

**Firmware**: ECU_TestV1_EGT_DRY_START_PATCH.ino  
**Board**: ESP32 DevKit V1 / NodeMCU-32S  
**Language**: Arduino C++

---

## 📋 Yêu Cầu Trước Khi Bắt Đầu

### **Hardware**
- [ ] ESP32 DevKit V1 hoặc NodeMCU-32S
- [ ] Cáp USB (USB-A to Micro-USB)
- [ ] Máy tính có Arduino IDE
- [ ] MAX31855 Thermocouple Module
- [ ] K-type Thermocouple
- [ ] RPM Sensor (Hall effect hoặc Optical)
- [ ] 2x ESC (Pump & Starter)
- [ ] 2x Solenoid Valve
- [ ] Glow Plug Driver
- [ ] Micro SD Card (FAT32 format)
- [ ] Power Supply (~12V, 10A)

### **Software**
- [ ] Arduino IDE 1.8.19+ hoặc 2.x
- [ ] ESP32 Board Support Package
- [ ] Required Libraries (xem phần cài đặt)

---

## 🔧 Cài Đặt Arduino IDE & ESP32

### **Bước 1: Cài Arduino IDE**

**Windows/Mac/Linux**:
1. Tải từ: https://www.arduino.cc/en/software
2. Cài đặt theo hướng dẫn
3. Mở Arduino IDE

### **Bước 2: Thêm ESP32 Board**

1. **File** → **Preferences**
2. Thêm URL Board Manager:
   ```
   https://dl.espressif.com/dl/package_esp32_index.json
   ```
3. **Tools** → **Board** → **Board Manager**
4. Tìm "ESP32" → Cài đặt (tác giả: Espressif Systems)
5. Chọn board: **ESP32 Dev Module**

### **Bước 3: Cài Đặt Libraries**

**Sketch** → **Include Library** → **Manage Libraries**

Tìm và cài các library này:

| Library | Tác Giả | Phiên Bản |
|---------|---------|-----------|
| Adafruit MAX31855 | Adafruit Industries | 1.1.0+ |
| ESP32Servo | John K. Bennett | 0.8.0+ |
| ArduinoJson | Benoit Blanchon | 6.19.0+ |

**Lệnh cài nhanh** (nếu dùng CLI):
```bash
arduino-cli lib install "Adafruit MAX31855"
arduino-cli lib install "ESP32Servo"
arduino-cli lib install "ArduinoJson"
```

---

## 🔌 Kết Nối Hardware

### **Sơ Đồ Kết Nối Cảm Biến**

```
┌─────────────────────────────────────────┐
│          ESP32 NodeMCU-32S              │
├─────────────────────────────────────────┤
│                                         │
│  EGT (MAX31855):                        │
│  ├─ CLK  → GPIO 18 (SPI CLK)            │
│  ├─ CS   → GPIO 5  (SPI CS)             │
│  └─ DO   → GPIO 19 (SPI MISO)           │
│                                         │
│  RPM Sensor:                            │
│  └─ Signal → GPIO 33 (Digital Input)    │
│                                         │
│  Pump ESC:                              │
│  └─ Signal → GPIO 26 (PWM)              │
│                                         │
│  Starter ESC:                           │
│  └─ Signal → GPIO 25 (PWM)              │
│                                         │
│  Valves:                                │
│  ├─ Valve 1 → GPIO 17 (Digital)         │
│  └─ Valve 2 → GPIO 16 (Digital)         │
│                                         │
│  Ignition/Glow:                         │
│  └─ Signal → GPIO 32 (Digital)          │
│                                         │
│  User Button:                           │
│  └─ Signal → GPIO 22 (Digital, active-low)
│                                         │
│  Status LED:                            │
│  └─ Onboard LED (GPIO 2, active-low)    │
│                                         │
│  SD Card (SPI):                         │
│  ├─ CS   → GPIO 13                      │
│  ├─ SCK  → GPIO 14                      │
│  ├─ MOSI → GPIO 23                      │
│  └─ MISO → GPIO 27                      │
│                                         │
│  Power:                                 │
│  ├─ GND → Common Ground                 │
│  └─ 5V  → (từ Power Supply)             │
│                                         │
└─────────────────────────────────────────┘
```

### **Kết Nối Cơ Bản (Minimal Setup)**

Để test firmware mà không cần động cơ:

```
1. ESP32 + Cáp USB (để upload & serial monitor)
2. MAX31855 + Thermocouple (test EGT)
3. RPM Sensor (test tín hiệu RPM)
4. LED + Resistor trên GPIO 2 (test status LED)
5. Button trên GPIO 22 (test user input)
6. Micro SD Card trên SPI pins (test logging)
```

**Không cần**: ESC, Solenoid Valves, Glow Plug Driver (để test trong Web UI)

---

## 📥 Upload Firmware

### **Bước 1: Mở File Firmware**

1. **File** → **Open**
2. Chọn: `ECU_TestV1_EGT_DRY_START_PATCH.ino`
3. File sẽ mở cùng với các libraries

### **Bước 2: Cấu Hình Board**

**Tools** menu:
```
Board:           ESP32 Dev Module
Upload Speed:    115200
CPU Frequency:   240 MHz
Flash Frequency: 80 MHz
Flash Mode:      DIO
Flash Size:      4MB
Partition Scheme: Default 4MB with spiffs
Port:            COM3 (hoặc /dev/ttyUSB0 trên Linux)
```

### **Bước 3: Compile & Upload**

**Cách 1: Button Sketch**
- Click **Upload** button (mũi tên →)
- Chờ "Compiling..." → "Uploading..." → "Done uploading"

**Cách 2: Menu**
- **Sketch** → **Upload** (Ctrl+U)

### **Bước 4: Chờ Upload Hoàn Thành**

```
Compiling sketch...
Linking everything together...
/home/.../xtensa-esp32-elf/bin/ld: ...
...
Leaving...
Hard resetting via RTS pin...

Upload complete!
```

**Thời gian**: ~30-60 giây tùy kích thước file

---

## 🖥️ Test Qua Serial Monitor

Sau khi upload xong:

### **Bước 1: Mở Serial Monitor**

**Tools** → **Serial Monitor** (Ctrl+Shift+M)

Hoặc dùng bất kỳ serial tool nào:
- PuTTY
- CoolTerm
- `minicom`/`screen` trên Linux

### **Bước 2: Thiết Lập Serial**

```
Baud Rate: 115200
Data Bits: 8
Stop Bits: 1
Parity: None
Line Ending: "Newline"
```

### **Bước 3: Xem Output**

Bạn sẽ thấy:
```
ESP32 Test ECU V1 - Starting up...
Web UI starting: ECU_TestV1 / admin1234
SD logging initialized...
Status: WAITING
RPM: 0, EGT: 25C, Pump: 1000us, Start: 1000us
```

### **Bước 4: Test Các Lệnh**

Gõ các lệnh vào Serial Monitor:

**Xem Trạng Thái**:
```
?
```
Kết quả:
```
=== CHECKLIST ===
EGT: NOT_RUN
RPM_NOISE: NOT_RUN
IGN_PULSE: NOT_RUN
...
```

**Xem Chi Tiết RPM**:
```
rpmdetail
```

**Đổi Filter RPM** (nếu nhiễu):
```
set rpmfilter 150
```

**Reset Bộ Đếm RPM**:
```
rpmreset
```

**Test Cảm Biến EGT**:
```
test EGT
```
Kết quả:
```
[TEST_EGT] Running... EGT reading OK: 25.3C
[TEST_EGT] PASS
```

---

## 🌐 Test Qua Web UI

### **Bước 1: Kết Nối WiFi**

Trên điện thoại/laptop:
1. Tìm WiFi network: **ECU_TestV1**
2. Nhập password: **admin1234**
3. IP tự động: **192.168.4.1**

### **Bước 2: Mở Dashboard**

1. Mở browser
2. Truy cập: **http://192.168.4.1**
3. Xem dashboard thực tế

### **Bước 3: Test Các Nút**

**ARM Button**:
- Stage2Armed = true
- Có thể nhấn START trong 10s

**START Button**:
- Bắt đầu trình tự khởi động
- Xem Starter ESC di chuyển (nếu có kết nối)

**STOP Button**:
- Dừng ngay lập tức
- Vào chế độ COOLDOWN

**Test Wizard**:
- Chạy 9 test liên tiếp
- Kiểm tra từng thành phần

---

## 🧪 Quy Trình Test Chi Tiết

### **Test 1: Power & Serial Communication**

```
✓ ESP32 được cấp điện
✓ Nhìn thấy output từ Serial Monitor
✓ LEDs trên board nhấp nháy
```

Nếu không thấy output:
- [ ] Kiểm tra cáp USB
- [ ] Kiểm tra driver CH340 (Windows)
- [ ] Kiểm tra port đúng không
- [ ] Reset board (nút Reset)

### **Test 2: Status LED**

```
Serial output: "STATUS_LED init..."
Nhìn thấy LED GPIO 2 nhấp nháy
```

Ngoài màn hình:
- Nhấn nút User (GPIO 22) → LED thay đổi
- Ngắn: Status → LED nhấp nháy
- Giữ 2s: ARM → LED chớp nhanh

### **Test 3: EGT Sensor**

**Command**:
```
test EGT
```

**Kết Quả Tốt**:
```
[TEST_EGT] Running...
[TEST_EGT] EGT: 25.3C
[TEST_EGT] PASS
```

**Nếu FAIL**:
```
[TEST_EGT] Fault: OPEN / SHORT_GND / SHORT_VCC
[TEST_EGT] FAIL
```

**Khắc Phục**:
- Kiểm tra kết nối thermocouple
- Kiểm tra dây SPI (CLK 18, CS 5, DO 19)
- Thử MAX31855 demo khác để verify module

### **Test 4: RPM Sensor**

**Command**:
```
test RPM_NOISE
```

**Kết Quả Tốt** (với magnet/pulse):
```
[TEST_RPM] Running...
[TEST_RPM] Raw edges: 45, Accepted: 43, Rejected: 2
[TEST_RPM] Jitter: 3%, Reject%: 4%
[TEST_RPM] PASS (CLEAN signal)
```

**Nếu FAIL**:
```
[TEST_RPM] Raw: 0, Accepted: 0
[TEST_RPM] FAIL - NO_SIGNAL
```

**Khắc Phục**:
- Kiểm tra cảm biến hall/optical trên GPIO 33
- Thử đổi cấu hình edge: `set rpmedge falling`
- Di chuyển magnet gần sensor hơn

### **Test 5: Button & LED**

**User Button**:
```
Nhấn nút GPIO 22 (giữ LOW)
Xem Serial output & LED thay đổi
```

**Status LED** (GPIO 2):
```
WAITING: Slow heartbeat
ARMED: Quick blink (2 Hz)
STARTING: Solid ON
COOLDOWN: Faster blink
```

### **Test 6: SD Card Logging**

**Check**:
```
1. Micro SD card Format FAT32
2. Insert vào module
3. Xem Serial: "SD initialized: ECU000.CSV"
4. Test START sequence
5. Rút card, xem file CSV trên PC
```

**Kết Quả**:
```
ECU000.CSV:
Time_ms,Mode,RPM,EGT_C,Pump_us,Start_us,Ign,V1,V2
0,WAITING,0,25.3,1000,1000,0,0,0
500,PURGE,150,26.1,1100,1100,0,0,0
1000,PURGE,300,27.0,1100,1100,0,0,0
...
```

### **Test 7: Web UI**

**Kết Nối**:
```
WiFi: ECU_TestV1
Pass: admin1234
URL: http://192.168.4.1
```

**Dashboard hiển thị**:
- [ ] Real-time RPM, EGT, Pump%
- [ ] Nút ARM, START, STOP
- [ ] Event log
- [ ] Parameter adjustment
- [ ] Test Wizard

### **Test 8: Dry-Start Mode** (Nếu EGT bị OPEN)

**Setup**:
```
1. Ngắt kết nối thermocouple
2. Power on ESP32
3. Serial output: "EGT Fault: OPEN"
4. Nhấn nút START
```

**Kỳ Vọng**:
```
✓ Vào Dry Start mode (chỉ starter chạy)
✓ PUMP: OFF
✓ VALVES: OFF
✓ IGN: OFF
✓ Sau đó không thể bắt được nhiên liệu
✓ Yêu cầu phải kết nối EGT trở lại mới start với xăng
```

---

## 🔧 Troubleshooting & Debug

### **Vấn Đề: Không kết nối USB**

**Nguyên Nhân Có Thể**:
- Driver CH340 chưa cài (Windows)
- Cáp USB bị lỏng
- Port COM sai

**Khắc Phục**:
```bash
# Linux: Kiểm tra port
ls /dev/ttyUSB*

# Windows: Kiểm tra Device Manager
# Cài driver CH340 từ: http://www.wch.cn/download/ch341ser_exe.zip
```

### **Vấn Đề: Upload thất bại**

**Lỗi phổ biến**:
```
A fatal error occurred: Failed to connect to ESP32: Timed out waiting for packet header
```

**Khắc Phục**:
1. Nhấn nút **Boot** + **Reset** trên board
2. Thử lại upload
3. Giảm Upload Speed: **115200** → **9600**

### **Vấn Đề: Serial Monitor trống**

**Nguyên Nhân**:
- Baud rate sai (phải 115200)
- Chưa upload sketch
- Board bị reboot

**Khắc Phục**:
```
1. Kiểm tra baud rate: 115200
2. Upload sketch lại
3. Nhấn Reset button
4. Mở Serial Monitor
```

### **Vấn Đề: EGT đọc NAN**

**Nguyên Nhân**:
- Kết nối SPI sai
- MAX31855 bị hỏng
- Thermocouple lỏng

**Kiểm Tra**:
```cpp
// Thêm vào setup() để debug:
Serial.println("Checking MAX31855...");
float temp = thermo.readCelsius();
uint8_t fault = thermo.readFault();
Serial.print("Temp: "); Serial.println(temp);
Serial.print("Fault: "); Serial.println(fault);
```

### **Vấn Đề: RPM đọc 0 hoặc sai**

**Nguyên Nhân**:
- GPIO 33 không nhận tín hiệu
- Magnet/sensor quá xa
- Polarity sai
- Nhiễu quá lớn

**Kiểm Tra**:
```
1. Dùng oscloscope đo GPIO 33 xem có xung không
2. Thử lệnh: set rpmfilter 100 (giảm filter)
3. Thử đổi edge: set rpmedge falling
4. Kiểm tra khoảng cách magnet-sensor
```

---

## 📊 Bảng Kiểm Tra Test

Sử dụng để verify từng thành phần:

| # | Thành Phần | Serial Test | Web UI | Status |
|---|-----------|------------|--------|--------|
| 1 | Power/USB | ✓ Output | - | ✓ |
| 2 | Status LED | ✓ Nhấp nháy | - | ✓ |
| 3 | User Button | ✓ Đọc press | ✓ ARM/START | ⚠️ |
| 4 | EGT Sensor | test EGT | ✓ Show value | ⚠️ |
| 5 | RPM Sensor | test RPM_NOISE | ✓ Show RPM | ⚠️ |
| 6 | SD Card | ✓ File created | - | ⚠️ |
| 7 | Web UI | - | ✓ Dashboard | ⚠️ |
| 8 | Dry-Start | ✓ Log msg | ✓ Mode | ⚠️ |

Điền: ✓ OK / ⚠️ Testing / ✗ Fail

---

## 🎯 Test Scenarios

### **Scenario 1: Chỉ Test Sensor**

```
1. Kết nối: USB + MAX31855 + RPM sensor
2. Upload firmware
3. test EGT
4. test RPM_NOISE
5. Verify Serial output & Web UI
Thời gian: 10 phút
```

### **Scenario 2: Full System Test**

```
1. Kết nối: Tất cả hardware
2. Upload firmware
3. Chạy Test Wizard từ Web UI
4. Xem từng test pass/fail
5. Kiểm tra SD log
Thời gian: 30 phút
```

### **Scenario 3: Start Sequence Dry-Run**

```
1. Chuẩn bị hardware nhưng không có máy bay/động cơ
2. Test ARM button
3. Test START button (không chạy động cơ)
4. Quan sát:
   - LED patterns
   - Serial output
   - Web UI status changes
   - SD log data
5. Test STOP button
Thời gian: 15 phút
```

---

## 📝 Ghi Chép Test Results

**Template để ghi lại kết quả**:

```
Date: ___________
Hardware: ESP32 / ______
Firmware: TestV1_EGT_DRY_START_PATCH

Test Results:
□ Power & Serial: ✓ / ✗ / ⚠️
□ Status LED: ✓ / ✗ / ⚠️
□ EGT Sensor: ✓ / ✗ / ⚠️ (value: _____°C)
□ RPM Sensor: ✓ / ✗ / ⚠️ (value: _____ RPM)
□ User Button: ✓ / ✗ / ⚠️
□ Web UI: ✓ / ✗ / ⚠️
□ SD Logging: ✓ / ✗ / ⚠️
□ Dry-Start: ✓ / ✗ / ⚠️

Issues Found:
1. ___________
2. ___________
3. ___________

Notes:
___________________________________________________________________________

Next Steps:
- [ ] Fix issues
- [ ] Re-test
- [ ] Ready for engine test?
```

---

## 🚀 Bước Tiếp Theo (Sau Khi Test Thành Công)

1. **Kết Nối Động Cơ Thực Tế**
   - Follow pin diagram
   - Đảm bảo an toàn trước khi cấp điện

2. **Calibrate Cảm Biến**
   - EGT: Verify temperature reading
   - RPM: Verify pulse count accuracy
   - Pump: Measure actual flow rate vs µs

3. **Fine-Tune Parameters**
   - Adjust idle RPM target
   - Calibrate fuel points
   - Test start timing

4. **Real Engine Start**
   - Thực hiện start sequence
   - Giám sát EGT & RPM
   - Kiểm tra log data

---

## 📚 Tài Liệu Tham Khảo

- **CODE_ARCHITECTURE.md** - Chi tiết firmware
- **README.md** - Project overview
- **Arduino IDE Docs** - https://www.arduino.cc/en/Guide
- **ESP32 Docs** - https://docs.espressif.com/
- **MAX31855 Datasheet** - Adafruit library documentation
- **Arduino Serial Monitor** - Built-in tool

---

**Phiên Bản**: 1.0  
**Cập Nhật**: 2026-07-16  
**Firmware**: ECU_TestV1_EGT_DRY_START_PATCH
