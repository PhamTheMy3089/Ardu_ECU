# Hướng Dẫn Dùng DSO152 Chỉnh KMZ10A RPM Sensor

## Tổng Quan

Mạch cảm biến RPM dùng **KMZ10A** (AMR magnetoresistive sensor) cần được chỉnh chính xác trước khi test động cơ. Hướng dẫn này cung cấp quy trình từng bước dùng **DSO152** (oscilloscope 1 kênh, ~200kHz BW) để kiểm tra và tinh chỉnh 3 trimpot trên mạch.

## Sơ Đồ Signal Chain (KMZ10A → GPIO33)

```
KMZ10A (U17)
  SENSOR_P / SENSOR_N  [TP: U13, U12]
        ↓ R41/R42 (100Ω series + C19 1nF diff filter)
   INA826 U20  Gain ≈ 38×  (VREF=2.5V tham chiếu)
        ↓ INA_OUT  [TP: U3]
   C15 (10µF) + R35 (1M) + R37 (4K7)  →  AC coupling + LPF
        ↓
   LMV358 U18 Op-Amp 1  ←  RP1 100K (OFFSET trim)
        ↓
   LMV358 U18 Op-Amp 2  ←  RP2 100K (GAIN trim)
        ↓ LMV358_OUT  [TP: U1]
   R32 (10K) + C2 (4.7nF LPF)
        ↓
   LMV393 U19 Comparator  ←  RP3 10K (THRESHOLD = VTH_RPM)  [TP: U21]
        ↓ RPM_OUT  [TP: U2]   (Schmitt: hysteresis R36 470K)
   J_RPM_OUT1 → cáp → RPM_PIN_IN1
        ↓
   R10 (10K pull-up đến 3.3V) + R2 (1K series) + D1 (TVS 3.3V clamp)
        ↓
   GPIO33 (ESP32)
```

## Các Testpoint Quan Trọng

| TP | Tên | Tín hiệu | Đặc điểm |
|----|-----|---------|----------|
| U15 | TP_GND | 0V reference | **Kẹp GND của probe vào đây cho toàn bộ quá trình** |
| U16 | TP_5V | +5V rail | Phải thấy DC 5V ổn định |
| U14 | TP_VREF | ~2.5V DC | Mid-rail cho toàn mạch analog |
| U13 | TP_SENSOR_P | Analog, mV | Output cầu dương của KMZ10A |
| U12 | TP_SENSOR_N | Analog, mV | Output cầu âm của KMZ10A |
| U3 | TP_INA_OUT | Analog, ×38 | Sau INA826, tâm ≈2.5V |
| U1 | TP_LMV358_OUT | Analog trimmed | Sau op-amp, trước comparator |
| U21 | TP_VTH_RPM | DC 0–5V | Ngưỡng comparator (chỉnh RP3) |
| **U2** | **TP_RPM_OUT** | **Digital 0/5V** | **Xung RPM cuối cùng ← ĐIỂM QUAN TRỌNG NHẤT** |
| GPIO33 | Trực tiếp | Digital 0/3.3V | Sau R2 + D1 clamp |

## 3 Trimpot Cần Chỉnh

| Trimpot | Giá trị | Chức năng | Chỉnh để |
|---------|---------|----------|---------|
| **RP1** | 100K | OFFSET (LMV358 tầng 1) | LMV358_OUT (U1) nằm tâm 2.5V khi magnet quay |
| **RP2** | 100K | GAIN (LMV358 tầng 2) | Biên độ tín hiệu analog đủ lớn qua ngưỡng comparator |
| **RP3** | 10K | THRESHOLD comparator | RPM_OUT (U2) ra xung sạch, không bị "double-pulse" |

---

## Cài Đặt DSO152 Chung

Que **GND** của probe cắm vào **TP_GND (U15)** — **giữ nguyên cho toàn bộ 5 bước**, không cần thay đổi. Chỉ di chuyển đầu dò (probe tip) từ testpoint này sang testpoint khác.

---

## Bước 1 — Kiểm Tra Nguồn Điện

### Mục tiêu
Xác nhận mạch cung cấp điện ổn định trước khi làm việc với tín hiệu.

### Cài DSO152
- **Coupling**: DC
- **Scale**: 2V/div
- **Time base**: 10ms/div
- **Trigger**: Auto

### Thực hiện
1. Probe vào **TP_5V (U16)**: phải thấy **DC ~5V phẳng, ổn định**
2. Probe vào **TP_VREF (U14)**: phải thấy **DC ~2.5V** (chấp nhận 2.3–2.7V)
3. Quan sát khoảng 10 giây — không được có ripple hay spike lớn

### Đánh giá
- **✅ PASS**: Cả hai TP ổn định → chuyển sang Bước 2
- **❌ FAIL**: Không đạt → Kiểm tra:
  - Nguồn +5V có đấu đúng không?
  - IC LDO (nếu có) hoạt động bình thường?
  - Dừng lại và sửa nguồn trước tiếp tục

---

## Bước 2 — Quan Sát INA826 Output (U3)

### Mục tiêu
Xác nhận cảm biến KMZ10A nhận được tín hiệu từ magnet.

### Cài DSO152
- **Coupling**: AC
- **Scale**: 500mV/div
- **Time base**: 2ms/div
- **Trigger**: Auto

### Thực hiện
1. **Quay tay magnet qua cảm biến tốc độ chậm** (~1–3 vòng/giây)
2. Probe vào **TP_INA_OUT (U3)**
3. Quan sát màn hình — phải thấy **dạng sóng hình sin/quasi-sin**

### Kỳ vọng
- Biên độ: **100mV–1Vpp**
- Dạng: Đều đặn, không bị méo nặng
- Tần số: khoảng 1–3 Hz (tương ứng quay tay)

### Chẩn đoán
| Trường hợp | Nguyên nhân | Giải pháp |
|-----------|-----------|---------|
| Không thấy sóng | Magnet quá xa / KMZ10A không nhận | Đưa magnet gần hơn; xoay 90° nếu cần |
| Biên độ < 50mVpp | Magnet quá yếu / khoảng cách không tối ưu | Kiểm tra loại magnet; thử khoảng cách khác |
| Sóng quá biến dạng | KMZ10A hỏng hoặc nhiễu | Kiểm tra linh kiện, đấu nối |

### Quyết định
- **✅ PASS**: Thấy sóng 100mV–1Vpp → chuyển sang Bước 3
- **❌ FAIL**: Không hoặc quá yếu → Sửa setup trước tiếp tục

---

## Bước 3 — Chỉnh RP1 (Offset) tại TP_LMV358_OUT (U1)

### Mục tiêu
Đưa tín hiệu analog sau op-amp về **tâm 2.5V** — đối xứng quanh ngưỡng so sánh.

### Cài DSO152
- **Coupling**: DC
- **Scale**: 1V/div
- **Time base**: 2ms/div
- **Trigger**: Auto

### Thực hiện
1. Chuyển probe sang **TP_LMV358_OUT (U1)**
2. **Quay magnet chậm đều** (1–2 vòng/giây)
3. Quan sát dạng sóng — ghi nhận chiều cao trên và dưới
4. **Vặn RP1 (trimpot offset)** từng từ:
   - Nếu sóng "hơi lên" (cao hơn 2.5V) → vặn RP1 để đẩy xuống
   - Nếu sóng "hơi xuống" (thấp hơn 2.5V) → vặn RP1 để kéo lên
5. Mục tiêu: **phần trên và phần dưới ngưỡng 2.5V phải bằng nhau**

### Kỳ vọng Sau Chỉnh
```
Trước: sóng lệch sang trên    →  RP1 điều chỉnh  →  Sau: sóng đối xứng quanh 2.5V
           tâm ≈3.5V                             →           tâm = 2.5V
```

### Lưu ý
- Vặn **chậm, cẩn thận** — trimpot 100K nhạy
- Nếu vặn hết một chiều vẫn không đúng → thử chiều ngược
- Không quá chặt (không cần "click" hay căng); sẽ "điều chỉnh liên tục"

### Quyết định
- **✅ PASS**: Sóng đối xứng, tâm ≈2.5V → Bước 4
- **❓ ĐIỀU CHỈNH**: Còn lệch → tiếp tục vặn RP1 cho đến khi cân bằng

---

## Bước 4 — Chỉnh RP2 (Gain) tại TP_LMV358_OUT (U1)

### Mục tiêu
Điều chỉnh **biên độ tín hiệu** sao cho đủ lớn để comparator kích hoạt rõ ràng, nhưng không quá lớn dẫn clipping.

### Cài DSO152 (Giữ nguyên từ Bước 3)
- **Coupling**: DC
- **Scale**: 1V/div
- **Time base**: 2ms/div
- **Trigger**: Auto

### Thực hiện
1. **Vẫn probe tại TP_LMV358_OUT (U1)**
2. **Quay magnet với tốc độ test thực tế** (chẳng hạn 3–5 vòng/giây)
3. Quan sát biên độ Vpp
4. **Vặn RP2 (trimpot gain)** để đạt **khoảng 1–2Vpp**
   - Quá nhỏ (<0.5Vpp) → comparator khó kích hoạt → tăng RP2
   - Quá lớn (>2.5Vpp) → nguy cơ clipping → giảm RP2
5. **Kiểm tra không bị cắt phẳng đỉnh** (clipping):
   - Nếu đỉnh sóng "bị chặt" hoặc quá nhọn → giảm RP2
   - Nếu sóng mượt, tròn, không bị cắt → OK

### Kỳ vọng
```
Tốc độ quay 3–5 vòng/giây:
  Biên độ sau RP2: 1–2Vpp
  Dạng: Hình sin mịn, không clipping, không bị cắt cạnh
```

### Quyết định
- **✅ PASS**: Biên độ 1–2Vpp, sóng mịn → Bước 5
- **❓ ĐIỀU CHỈNH**: Chưa đủ hoặc bị cắt → tiếp tục vặn RP2

---

## Bước 5 — Chỉnh RP3 (Threshold) và Xác Nhận Xung RPM_OUT (U2)

### Mục tiêu
Điều chỉnh **ngưỡng comparator** để:
- Ra xung **vuông sạch, 0/5V**
- **Đều đặn, không "double-pulse"** hoặc spike giả
- **1 xung = 1 cực magnet đi qua**

### Cài DSO152
- **Coupling**: DC
- **Scale**: 2V/div
- **Time base**: 2ms/div
- **Trigger**: Rising edge (hoặc Auto)

### Thực hiện
1. Chuyển probe sang **TP_RPM_OUT (U2)** ← **ĐIỂM QUAN TRỌNG NHẤT**
2. **Quay magnet đều, tốc độ vừa phải** (2–4 vòng/giây)
3. Quan sát xung RPM_OUT trên màn hình DSO

### Chỉnh RP3 — Phương Pháp "Mò Biên"

**Kịch bản A: Quá nhiều xung giả / Double-pulse**
```
Trước: Mỗi cực magnet tạo 2–3 xung thay vì 1
Nguyên nhân: Ngưỡng comparator quá thấp → mối nhiễu nhỏ cũng kích hoạt
Cách sửa: Vặn RP3 để **TĂNG ngưỡng** (vặn theo hướng loại bỏ spike)
Sau: 1 xung sạch / cực magnet
```

**Kịch bản B: Xung biến mất hoặc rất yếu**
```
Trước: Xung gần như không có hoặc rất mỏng
Nguyên nhân: Ngưỡng comparator quá cao → tín hiệu không đủ cao để vượt ngưỡng
Cách sửa: Vặn RP3 để **GIẢM ngưỡng** (vặn theo hướng khác)
Sau: Xung sạch xuất hiện
```

### Quy Trình Chi Tiết
1. **Bước 1**: Vặn RP3 từ từ theo một hướng, quan sát Bước 2
2. **Bước 2**: Quay magnet đều, đếm số xung trên DSO
   - Nếu thấy 2+ xung/cực → vặn RP3 theo hướng loại bỏ spike
   - Nếu xung biến mất → dừng, vặn theo hướng ngược lại
3. **Bước 3**: Tìm điểm "ranh giới" — nơi xung từ giả→sạch hoặc sạch→biến mất
4. **Bước 4**: Đặt RP3 tại **1/4 vòng trước điểm biến mất** (margin an toàn)

### Kỳ Vọng
```
TP_RPM_OUT (U2) hiển thị:
  - Dạng: Xung vuông, 0/5V sạch nét
  - Tần số: Đều đặn, 1 xung/cực magnet
  - Không có spike bổ sung hay rung sau xung
  - Thời gian lên/xuống: < 100µs (xung "nhanh")
```

### Kiểm Tra Thêm: Glow Plug / Starter Bật
1. **Bật glow plug** (nếu có on-board) hoặc **gây nhiễu RF** (vòng khoảng cảm biến)
2. Quan sát TP_RPM_OUT — có spike nhiễu thêm không?
   - Nếu **có spike** → vặn RP3 tăng ngưỡng thêm **1/8–1/4 vòng** nữa
   - Nếu **không → OK**, ngưỡng đủ mạnh chống nhiễu
3. Sau kiểm tra: **Dừng glow plug, quay magnet bình thường → xung phải sạch lại**

### Quyết định
- **✅ PASS**: Xung sạch 0/5V, 1/cực, không spike khi glow bật → Chuẩn bị firmware test
- **❓ ĐIỀU CHỈNH**: Còn double-pulse hoặc spike → tiếp tục vặn RP3 từng chút

---

## Bước 6 — Kiểm Tra GPIO33 (Trực Tiếp ESP32)

### Mục tiêu
Xác nhận xung RPM_OUT an toàn vào chân GPIO33 của ESP32 (qua R2 + D1 clamp).

### Cài DSO152 (Như Bước 5)
- **Coupling**: DC
- **Scale**: 2V/div
- **Time base**: 2ms/div
- **Trigger**: Rising edge

### Thực hiện
1. Cắm cáp từ **J_RPM_OUT1** vào **RPM_PIN_IN1** (cable lên ECU board)
2. Probe DSO vào chân **GPIO33** trực tiếp (hoặc testpoint nếu có)
3. Quay magnet đều, quan sát

### Kỳ Vọng
- Dạng: Xung **0–3.3V** (D1 clamp giữ không vượt)
- Tần số: Sạch, đều, 1/cực
- **Ghi chú**: Xung GPIO33 sẽ thấp hơn TP_RPM_OUT (từ 5V xuống 3.3V vì D1 clamp), nhưng dạng vẫn sạch

### Quyết định
- **✅ PASS**: Xung 0–3.3V sạch, đều → Sẵn sàng firmware test
- **❌ FAIL**: Xung không sạch hay suy yếu → Quay lại Bước 5, tăng RP3 thêm

---

## Verification Với Firmware TEST_STARTER

### Chuẩn Bị
1. Upload `TEST_STARTER.ino` lên ESP32
2. Cắm cáp RPM từ mạch vào GPIO33
3. Mở Serial Monitor ở 115200 baud
4. Phát lệnh: `rpmdetail on`

### Kiểm Tra
```
Quay tay magnet:
  - 1 vòng/giây (ppr=1) → Serial phải báo ~60 RPM
  - 2 vòng/giây → ~120 RPM
  - 3 vòng/giây → ~180 RPM
  ↓
Chạy firmware lệnh `rpmdetail on` đối chiếu:
  - NOISE: phải thấy "CLEAN"
  - rejectPct: phải < 5%
  - RPM value: phải khớp với tay quay thực tế
```

### Nếu Không Khớp
- **RPM quá cao hoặc quá thấp**: Kiểm tra firmware có cấu hình `ppr` (pulses per revolution) đúng không
- **NOISE = NOISY**: Quay lại Bước 5, tăng RP3 thêm (tăng ngưỡng chống nhiễu)
- **rejectPct > 5%**: Xem lại xung TP_RPM_OUT (U2) có clean không; có thể cần tinh chỉnh RP3 thêm

---

## Tóm Tắt 5 Bước

| Bước | Testpoint | Trimpot | Kiểm Tra | Target |
|------|-----------|---------|----------|--------|
| 1 | U16, U14 | — | Nguồn +5V, +2.5V | DC ổn định |
| 2 | U3 | — | INA826 output | Sóng sin 100mV–1Vpp |
| 3 | U1 | **RP1** | LMV358 offset | Tâm 2.5V, đối xứng |
| 4 | U1 | **RP2** | LMV358 gain | Biên độ 1–2Vpp, sóng mịn |
| 5 | U2 | **RP3** | Comparator threshold | Xung sạch 0/5V, 1/cực, chống spike |
| 6 | GPIO33 | — | Đầu vào ESP32 | Xung 0–3.3V sạch |

---

## Lưu Ý Quan Trọng

### 🔒 An Toàn
- **Không vặn RP1/RP2/RP3 quá chặt** — chúng là trimpot điều chỉnh, không "tắt/mở" hoàn toàn
- **GND phải giữ nguyên** trên DSO152 suốt quá trình
- Nếu thấy bất thường (khí nóng, khét, diode phát sáng) → **tắt ngay**

### 📏 Chuẩn Độ DSO152
- Nếu không chắc scale → bắt đầu từ scale **lớn nhất** rồi zoom xuống
- AC coupling dùng cho tín hiệu dao động (Bước 2, trước khi offset)
- DC coupling dùng khi cần thấy mức DC (Bước 1, 3, 4, 5)

### 🌡️ Biến Thiên Nhiệt Độ
- Sau khi chỉnh xong, bật glow plug hoặc tạo phát nhiệt gần mạch
- Quan sát 5–10 phút → xung RPM_OUT có bị drift không?
- Nếu drift → tăng RP3 thêm **1/8–1/4 vòng** margin tránh nhiệt độ cao

### 🧲 Magnet Và Sensor
- Khoảng cách cảm biến–magnet: **10–20 mm tối ưu** (phụ thuộc loại magnet)
- Nếu magnet quay không đều → quay tay chậm, không giật
- Loại magnet: Cần **N50 hoặc mạnh hơn** (KMZ10A nhạy)

### 🔧 Trimpot Chỉnh
- **RP1 (offset)**: Ảnh hưởng đến tâm sóng — vặn nhẹ, từng chút một
- **RP2 (gain)**: Ảnh hưởng đến biên độ — vặn từng trang để quan sát biến đổi
- **RP3 (threshold)**: Ảnh hưởng đến ngưỡng kích — vặn cẩn thận, dùng "mò biên" để tìm điểm sạch

### 📝 Ghi Chép
Sau khi chỉnh xong, ghi lại:
- Vị trí RP1, RP2, RP3 (vd: "RP1 quay 2/3 vòng từ trái", "RP2 quay tâm", "RP3 quay 1/4 từ phải")
- Tốc độ quay test: bao nhiêu vòng/giây → bao nhiêu RPM firmware báo
- Điều kiện glow plug: có bật không → xung có clean không
- Ngày chỉnh + họ tên người chỉnh

---

## Công Cụ Cần Chuẩn Bị

- **DSO152** với probe 1×10 hoặc 1×1 (phụ thuộc mức điện áp, 1× thích hợp cho bước 1–4, 1×10 cho bước 5 nếu cần giảm dung lượng)
- **Screwdriver nhỏ** (để vặn trimpot)
- **Bộ jumper / que điểm** (để kết nối probe)
- **Magnet** (loại N50 hoặc tương tương)
- **Máy tính / Laptop** chạy Serial Monitor hoặc terminal, kết nối USB ESP32

---

## Xử Sự Cố

| Vấn Đề | Triệu Chứng | Kiểm Tra | Giải Pháp |
|--------|-----------|----------|---------|
| Comparator không ra xung | GPIO33 mãi 0V | TP_RPM_OUT (U2) | Bước 5: Tăng RP2 gain, giảm RP3 threshold |
| Double-pulse / spike | RPM báo cao gấp đôi | TP_RPM_OUT (U2) | Bước 5: Tăng RP3 threshold |
| RPM drift theo nhiệt độ | Lúc lạnh sạch, lúc nóng spike | Bước 5 + glow bật | Tăng RP3 thêm margin |
| Xung quá yếu vào GPIO33 | Firmware không nhận RPM | GPIO33 + TP_RPM_OUT | Kiểm tra D1 clamp, R2 series; giảm RP3 |
| Sóng INA_OUT quá yếu | Bước 2 biên độ < 50mVpp | TP_INA_OUT (U3) | Magnet quá xa; xoay cảm biến 90° |

---

## Tài Liệu Tham Khảo

- **KMZ10A datasheet**: AMR sensor, output differential mV theo từ trường
- **INA826 datasheet**: Instrumentation amplifier, gain khả dụng 1–10000
- **LMV358 datasheet**: Op-amp rẻ, rail-to-rail, tần số gain-bandwidth ~1MHz
- **LMV393 datasheet**: Comparator, response time ~300ns
- **DSO152**: User Manual — tìm trên trang nhà sản xuất hoặc MFG Wiki
