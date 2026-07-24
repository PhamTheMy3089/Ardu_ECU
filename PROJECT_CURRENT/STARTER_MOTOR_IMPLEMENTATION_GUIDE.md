# Hướng Dẫn Triển Khai Khối Điều Khiển Motor Đề (Starter Motor)

## Mục Tiêu

Giải quyết triệt để **lỗi reset tại mức xung ~1465µs** của mạch ECU khi khởi động động cơ phản lực Enjet E86, bằng cách đồng bộ hóa **Phần cứng (Hardware)**, **Cài đặt ESC (Configuration)**, và **Phần mềm (Firmware Code)**.

---

## 1. Chuẩn Bị Phần Cứng (Hardware Setup)

### 1.1 Tách Rời Nguồn Điện (Power Isolation) — **BẮT BUỘC**

**Vấn đề gốc rễ**: Khi motor đề chạy ở tốc độ cao và chịu tải nặng (cánh nén), dòng điện tức thời có thể lên tới 30–50A. Nếu dùng chung pin 3S cho cả ESC lẫn vi điều khiển (Arduino/ESP32):
- Dây dẫn từ pin → ESC → GND chung với mạch vi điều khiển sẽ tạo **sụt áp (voltage droop)** hàng chục mV
- Vi điều khiển nhạy cảm với sụt áp, khi VDD xuống dưới ngưỡng brownout (~3V) → **reset**

**Giải pháp**:

| Thành phần | Cấu hình |
|-----------|---------|
| **Pin LiPo 3S khởi động motor đề** | Cấp riêng cho ESC 30A |
| **Nguồn vi điều khiển** | BEC 5V từ pin 2S/3S khác HOẶC sạc dự phòng 5V USB riêng |
| **Nối chung GND** | ✅ Nối GND của ESC + GND của vi điều khiển cùng với pin (điểm tham chiếu chung) |
| **Không nối chung VDD** | ❌ Tuyệt đối không dùng chung pin 3S trực tiếp |

**Sơ đồ nối nguồn**:
```
Pin 3S (Motor)
  ├─ Dây (+) → ESC 30A (cực dương IN)
  ├─ Dây (-) → ESC GND (cực âm IN)
  └─ GND chung tới vi điều khiển

Pin 5V / BEC (Vi điều khiển)
  ├─ Dây 5V → Arduino/ESP32 VCC
  ├─ Dây GND → Arduino/ESP32 GND
  └─ GND chung tới pin motor (điểm tham chiếu duy nhất)

(Không có dây VDD trực tiếp từ pin 3S motor tới vi điều khiển)
```

### 1.2 Tụ Lọc Nguồn Lớn (Buffer Capacitor) — **KHUYẾN CÁO**

**Mục đích**: Bù áp tức thời khi dòng motor đột ngột tăng, ngăn sụt áp đột ngột.

**Thông số**:
- Loại: **Electrolytic capacitor**
- Dung lượng: **1000µF hoặc lớn hơn** (2200µF tốt nhất)
- Điện áp: **25V** trở lên (vì pin 3S → max ~12.6V)
- Vị trí: **Hàn song song ngay tại 2 cực vào (Âm/Dương) của ESC**

**Cách hàn**:
```
Pin 3S (+) ──────┬──→ ESC IN (+)
                 │
                [C] 1000µF / 25V
                 │
Pin 3S (-) ──────┴──→ ESC IN (-)
```

**Lưu ý an toàn**:
- Kiểm tra cực tính capacitor (+ và -)
- Để dây dẫn càng ngắn càng tốt (< 5cm)
- Không được hàn capacitor ngược cực → nổ

### 1.3 Mạch Gộp Dây CH340 (Để Flash ESC) — **CẦN CHUẨN BỊ**

Bạn đã có mạch CH340 với jumper 3.3V từ ảnh trước → **dùng tốt**.

Để flash firmware BLHeli_S vào ESC:
- ESC BLHeli_S có cổng UART hoặc bootloader hỗ trợ firmware update
- Nối 3 dây: **GND + TXD + RXD** từ CH340 tới chân UART của ESC
- Dùng phần mềm **BLHeliSuite** (Windows hoặc Linux via Wine) để nạp file `.hex` mới

---

## 2. Cài Đặt ESC BLHeli_S (Configuration)

### 2.1 Quy Trình Nạp Firmware ESC

**Bước 1**: Chuẩn bị
- Tải phần mềm **BLHeliSuite** từ https://github.com/bitdump/BLHeli (hoặc phiên bản cũ hơn từ trang cũ của BLHeli)
- Chuẩn bị mạch CH340 + 3 dây (GND, TXD, RXD)
- Cắm ESC vào mạch (hoặc chỉ nối dây bootload, không cần cấp điện ESC)

**Bước 2**: Kết nối ESC qua CH340
- Mở BLHeliSuite
- Chọn **Programmer** → chọn cổng COM của CH340
- Click **Read** để đọc firmware hiện tại từ ESC
- Nếu kết nối thành công → sẽ thấy tên ESC, phiên bản firmware, loại motor

**Bước 3**: Sửa đổi 3 thông số quan trọng
(Xem **Mục 2.2** dưới đây)

**Bước 4**: Click **Write** để nạp lại cấu hình
- Phần mềm sẽ báo "Write Complete" khi xong
- ESC sẽ reset, phát ra tiếng nhạc chào

### 2.2 Ba Thông Số Cần Chỉnh Sửa Trong BLHeliSuite

Sau khi đọc firmware ESC lên, bạn sẽ thấy cửa sổ **Settings** chứa hàng chục tham số. Chỉ cần sửa **3 thông số sau**:

#### Thông Số 1: **Startup Power** (Công suất khởi động)
| Hiện tại | Thay đổi thành | Lý do |
|----------|---|---|
| **0.5 – 1.0** (mặc định drone) | **0.125 – 0.25** | Giảm dòng khởi động, motor không bị "búng" mạnh → tránh sốc dòng, sụt áp |

**Chi tiết**:
- Giá trị thấp = dòng khởi động thấp = tăng tốc từ từ
- Giá trị cao = dòng khởi động cao = tăng tốc nhanh (lý tưởng cho drone nhẹ, nhưng tai hại cho tải nặng)

**Đề xuất**: Chọn **0.125** (thấp nhất) để test an toàn lần đầu. Nếu motor khởi động quá yếu, tăng dần lên 0.25 / 0.375 / 0.5.

#### Thông Số 2: **Motor Timing** (Góc đánh lửa / Phase Advance)
| Hiện tại | Thay đổi thành | Lý do |
|----------|---|---|
| **Medium** | **High** | Chip Silabs tính toán pha chuẩn hơn khi motor kéo tải nặng → chống desync |

**Chi tiết**:
- **Low**: Phase lag lớn → thiếu mô-men xoắn khi tua thấp
- **Medium**: Cân bằng (mặc định drone)
- **High**: Advance chuẩn xác hơn → mô-men xoắn tốt ở tua thấp (lý tưởng cho starter)

**Đề xuất**: Để **High** để có sức mạnh tối đa khi khởi động.

#### Thông Số 3: **Low RPM Power Protect** (Bảo vệ tua thấp)
| Hiện tại | Thay đổi thành | Lý do |
|----------|---|---|
| **Enabled** (mặc định) | **Disabled** (Tắt) | Tắt tính năng này để motor có đủ lực kéo cánh nén khí ở tua thấp |

**Chi tiết**:
- **Enabled** (mặc định): Nếu motor cảm thấy quay nặng ở tua thấp, ESC sẽ tự ngắt để bảo vệ. Điều này **rất hữu ích cho drone** (tránh motor bị khỏa quá tức thì), nhưng **tai hại cho motor đề** (ngắt đột ngột → motor không thể kéo cánh nén).
- **Disabled**: Motor sẽ cố gắng đẩy mạnh nhất có thể, miễn là dòng không vượt giới hạn firmware.

**Đề xuất**: **Disabled** (Tắt) để motor đề có full power.

### 2.3 Các Thông Số Khác (Giữ Mặc Định)

| Thông số | Giữ nguyên | Ghi chú |
|---------|-----|----|
| Max Throttle | 2000µs | Chuẩn |
| Min Throttle | 1000µs | Chuẩn |
| Motor Poles | (Tùy motor đề) | Không thay đổi |
| Battery Type | LiPo (3S) | OK |
| Braking | Enabled hoặc Disabled | Tùy chọn |

---

## 3. Lập Trình Phần Mềm (Firmware Code)

### 3.1 Nguyên Tắc: Thuật Toán Ramping (Tăng Ga Mịn)

**Vấn đề**: Nếu code ghi `escWriteUs(1450)` một cách trực tiếp (nhảy từ 1000µs → 1450µs tức thì), dòng motor sẽ tăng từ 0A → 30–50A trong **vài milisecond**, gây sốc dòng → sụt áp → reset.

**Giải pháp**: Tăng xung **từng chút một** theo thời gian, để dòng tăng dần mà pin kịp cấp.

**Công thức**:
```
Thời gian để tăng từ A µs → B µs với bước ∆ µs mỗi T ms:

Số bước = (B - A) / ∆
Thời gian tổng = số bước × T

Ví dụ:
  Tăng từ 1000µs → 1450µs
  Bước tăng: 1µs
  Khoảng thời gian: 10ms
  → Số bước = 450 / 1 = 450
  → Thời gian = 450 × 10ms = 4500ms = 4.5 giây
```

### 3.2 Đoạn Code Triển Khai

Dưới đây là code Arduino/ESP32 để test thuật toán ramping:

#### **Phiên Bản 1: Dùng Servo library (Đơn giản)**

```cpp
#include <Servo.h>

Servo starterMotor;
int currentPWM = 1000;      // Giá trị PWM hiện tại (đơn vị: µs)
int targetPWM = 1450;       // Giá trị PWM đích muốn thử
unsigned long previousMillis = 0;
const int rampInterval = 10; // Cứ mỗi 10ms thì tăng xung 1 lần (1µs)

void setup() {
  Serial.begin(115200);
  
  // Gắn servo vào chân GPIO (ví dụ: chân 9 trên Arduino, GPIO 27 trên ESP32)
  starterMotor.attach(9);
  
  // Khởi tạo ESC ở mức 0% ga (1000µs)
  starterMotor.writeMicroseconds(1000);
  Serial.println("ESC initialized at 1000us (0% throttle)");
  
  // Chờ ESC nhận tay ga (~3 giây, ESC phát tiếng nhạc chào)
  delay(3000);
  Serial.println("Ready to ramp up...");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Kiểm tra khoảng thời gian để tăng xung
  if (currentMillis - previousMillis >= rampInterval) {
    previousMillis = currentMillis;
    
    // Nếu chưa đạt target, tăng thêm 1µs
    if (currentPWM < targetPWM) {
      currentPWM++;
      starterMotor.writeMicroseconds(currentPWM);
      
      // In ra Serial để theo dõi
      Serial.print("PWM: ");
      Serial.print(currentPWM);
      Serial.println("us");
    } else {
      // Đã đạt target, giữ nguyên
      Serial.println("Target reached! Holding at ");
      Serial.println(targetPWM);
      
      // Giữ mãi mãi (hoặc set timeout để dừng)
      while (true) {
        delay(100);
      }
    }
  }
}
```

#### **Phiên Bản 2: Dùng LEDC (ESP32 native, không cần Servo library)**

```cpp
// ESP32 LEDC PWM
const int MOTOR_PIN = 27;        // Chân GPIO điều khiển motor
const int LEDC_CHANNEL = 0;
const int LEDC_FREQ = 50;        // 50Hz cho servo/ESC
const int LEDC_RES = 16;         // 16-bit resolution
const int PWM_MIN = 1000;        // µs
const int PWM_MAX = 2000;        // µs

int currentPWM = 1000;
int targetPWM = 1450;
unsigned long previousMillis = 0;
const int rampInterval = 10;     // ms

void setup() {
  Serial.begin(115200);
  
  // Cấu hình LEDC
  ledcAttach(MOTOR_PIN, LEDC_FREQ, LEDC_RES);
  
  // Khởi tạo ESC
  writePWM(1000);
  Serial.println("ESC initialized at 1000us");
  
  delay(3000);
  Serial.println("Ready to ramp up...");
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= rampInterval) {
    previousMillis = currentMillis;
    
    if (currentPWM < targetPWM) {
      currentPWM++;
      writePWM(currentPWM);
      
      Serial.print("PWM: ");
      Serial.print(currentPWM);
      Serial.println("us");
    } else {
      Serial.print("Target reached at ");
      Serial.println(targetPWM);
      delay(1000000);  // Dừng forever
    }
  }
}

// Hàm chuyển đổi µs → duty cycle LEDC
void writePWM(int microSeconds) {
  // LEDC 50Hz → 1 chu kỳ = 20ms = 20000µs
  // duty = (microSeconds / 20000) × 2^16
  int duty = map(microSeconds, 1000, 2000, 800, 1600);  // Tương ứng với 5% – 8% duty
  ledcWrite(LEDC_CHANNEL, duty);
}
```

### 3.3 Hướng Dẫn Test Code

1. **Sửa giá trị `targetPWM`** nếu cần:
   - Lần 1: Test với `targetPWM = 1200` (an toàn, tốc độ thấp)
   - Lần 2: `targetPWM = 1300`
   - Lần 3: `targetPWM = 1400`
   - Lần 4: `targetPWM = 1450` (mục tiêu)

2. **Cấp nguồn**: 
   - Pin 3S cho ESC
   - BEC 5V cho Arduino/ESP32
   - Cắm cáp từ GPIO (Arduino pin 9 / ESP32 GPIO 27) vào chân tín hiệu ESC

3. **Mở Serial Monitor** ở 115200 baud → xem in log PWM

4. **Quan sát**:
   - Motor quay hay không?
   - RPM có tăng từ từ không?
   - Có bị reset không?
   - Nếu không reset ở 1450µs → **thành công!**

### 3.4 Nếu Vẫn Gặp Lỗi Reset

| Triệu chứng | Nguyên nhân | Cách khắc phục |
|-----------|----------|---|
| Motor không quay | ESC không nhận lệnh | Kiểm tra cáp nối GPIO → ESC, kiểm tra ESC đã khởi tạo thành công |
| Motor quay rồi dừng + reset | Sụt áp → vi điều khiển reset | Kiểm tra tụ lọc 1000µF đã hàn chưa? Kiểm tra pin 3S có đủ dung lượng không? |
| Reset ở tua cao nhất | Dòng quá lớn | Giảm `targetPWM` xuống, hoặc tăng `rampInterval` (chậm hơn) |
| Motor khởi động rất yếu | Startup Power ESC quá thấp | Tăng từ 0.125 lên 0.25 / 0.375 trong BLHeliSuite |

---

## 4. Checklist Triển Khai Chi Tiết

### ✅ Giai Đoạn 1: Chuẩn Bị Phần Cứng
- [ ] Chuẩn bị 2 pin LiPo riêng (1 cho ESC motor, 1 cho BEC vi điều khiển)
- [ ] Hàn capacitor 1000µF/25V song song vào 2 cực vào ESC
- [ ] Nối dây GND chung giữa pin motor + pin BEC + vi điều khiển (điểm tham chiếu duy nhất)
- [ ] Chuẩn bị mạch CH340 với jumper 3.3V

### ✅ Giai Đoạn 2: Flash & Cấu Hình ESC
- [ ] Tải phần mềm BLHeliSuite
- [ ] Kết nối ESC với máy tính qua mạch CH340 (3 dây: GND, TXD, RXD)
- [ ] Đọc firmware hiện tại từ ESC (Read)
- [ ] **Sửa 3 thông số**:
  - [ ] Startup Power: **0.125 – 0.25**
  - [ ] Motor Timing: **High**
  - [ ] Low RPM Power Protect: **Disabled (Tắt)**
- [ ] Nạp cấu hình mới vào ESC (Write)
- [ ] Nghe tiếng nhạc chào → thành công

### ✅ Giai Đoạn 3: Viết & Test Code
- [ ] Chọn code Arduino hoặc LEDC (tùy vi điều khiển)
- [ ] Nạp vào Arduino/ESP32
- [ ] Mở Serial Monitor (115200 baud)
- [ ] **Test dần từ thấp lên cao**:
  - [ ] targetPWM = 1200 → không reset?
  - [ ] targetPWM = 1300 → không reset?
  - [ ] targetPWM = 1400 → không reset?
  - [ ] targetPWM = 1450 → không reset? ✅ **Mục tiêu đạt được!**

### ✅ Giai Đoạn 4: Tối Ưu (Optional)
- [ ] Giảm `rampInterval` (tăng tốc độ tăng xung) nếu 4.5 giây quá lâu
- [ ] Giảm `rampInterval` = tăng dòng nhanh hơn = kiểm tra độ ổn định pin
- [ ] Nếu vẫn ổn định → tốt!

---

## 5. Tài Liệu & Link Tham Khảo

- **BLHeliSuite**: https://github.com/bitdump/BLHeli
- **BLHeli_S Firmware Configuration**: https://blhelisuite.blogspot.com/ (blog chính thức)
- **Arduino Servo Library**: https://www.arduino.cc/reference/en/libraries/servo/
- **ESP32 LEDC**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html
- **Pin Specs**: Loại LiPo 3S (11.1V nominal) + High discharge rate (30C+)

---

## 6. Ghi Chú Quan Trọng

⚠️ **An toàn điện**:
- Luôn kiểm tra cực tính pin trước khi cắm
- Không được sơ ý hạ dài dây dẫn (có thể gây chập nháp)
- Capacitor 1000µF có năng lượng → tắt pin trước khi tháo

⚠️ **Test tuần tự**:
- Không cố gắng nhảy thẳng lên 1450µs lần đầu
- Test từ 1100µs → 1150µs → 1200µs → ... từ từ
- Mỗi lần test cách nhau ít nhất 5–10 phút (để pin hạ nhiệt)

⚠️ **Motor Đề**:
- Motor đề của Enjet E86 thường là motor brushed DC nhỏ (không phải brushless)
- Nếu ESC của bạn là cho brushless, cần ESC khác hoặc chuyển đổi firmware (nếu ESC hỗ trợ)

---

## Kết Luận

Bằng cách kết hợp:
1. **Tách rời nguồn** (chống sụt áp)
2. **Tuning ESC** (Startup Power + Timing + Low RPM Protect)
3. **Code ramping mịn** (tăng dòng từ từ)

Bạn sẽ **loại bỏ triệt để lỗi reset ở 1465µs** và có một khối điều khiển motor đề ổn định, sẵn sàng cho giai đoạn khởi động động cơ phản lực tiếp theo.

Nếu vẫn gặp khó khăn ở bất kỳ bước nào, hãy báo với mình — chúng ta sẽ debug từng phần một! 🚀
