# Hướng Dẫn Hiệu Chỉnh Mạch RPM KMZ10A Bằng Oscilloscope DSO152

**Firmware**: ECU_TestV1_EGT_DRY_START_PATCH  
**Cảm biến**: KMZ10A (AMR magnetoresistive)  
**Oscilloscope**: DSO152 (1 kênh, BW ~200kHz)  
**Ngày**: 2026-07-16

---

## ⚠️ Lưu Ý Quan Trọng Về KMZ10A

KMZ10A **không phải Hall sensor thông thường** — đây là cảm biến
**từ trở anisotropic (AMR)** dạng cầu Wheatstone. Output là tín hiệu
**analog vi sai (differential) cỡ mV**, cần qua toàn bộ chuỗi mạch
khuếch đại và so sánh trước khi trở thành xung digital cho ESP32.

---

## Sơ Đồ Signal Chain

```
KMZ10A (U17, header 4 chân)
  Pin 1: +V0  → SENSOR_P  [TP: U13]
  Pin 3: −V0  → SENSOR_N  [TP: U12]
  Pin 4: VCC  → +5V
  Pin 2: GND  → GND
        │
        │ R41/R42 (100 Ω series) + C19 (1 nF, lọc nhiễu vi sai)
        ▼
   INA826 U20 — Instrumentation Amplifier
   Gain = 1 + 100kΩ/R43 = 1 + 100k/2.7k ≈ 38×
   REF = GND, V+ pin6 = VREF (2.5V)
        │
        │ INA_OUT  [TP: U3]
        ▼
   C15 (10 µF) + R35 (1 MΩ) + R37 (4K7) — AC coupling + LPF
        │
        ▼
   LMV358 U18 Op-Amp 1 ← RP1 100K (OFFSET trim)
        │
        ▼
   LMV358 U18 Op-Amp 2 ← RP2 100K (GAIN trim)
        │
        │ LMV358_OUT  [TP: U1]
        ▼
   R32 (10K) + C2 (4.7 nF LPF)
        │
        ▼
   LMV393 U19 Comparator (Schmitt trigger, hysteresis qua R36 470K)
   IN− = VTH_RPM  [TP: U21]  ← RP3 10K (THRESHOLD trim)
        │
        │ RPM_OUT  [TP: U2]  ← ĐIỂM ĐO QUAN TRỌNG NHẤT
        ▼
   J_RPM_OUT1 → cáp ngoài → RPM_PIN_IN1
        │
        │ R10 (10K pull-up 3.3V) + R2 (1K series) + D1 (TVS 3.3V)
        ▼
   GPIO33 (ESP32 NodeMCU-32S)
```

---

## Bản Đồ Testpoint Trên Mạch

| Ký hiệu | Tên | Loại tín hiệu | Dùng cho |
|---------|-----|--------------|---------|
| **U15** | TP_GND | 0 V reference | Kẹp GND probe vào đây suốt quá trình đo |
| **U16** | TP_5V | DC +5V | Kiểm tra nguồn cấp mạch analog |
| **U14** | TP_VREF | DC ~2.5V | Kiểm tra mid-rail (tham chiếu cả mạch) |
| **U13** | TP_SENSOR_P | Analog mV | Output cầu + của KMZ10A |
| **U12** | TP_SENSOR_N | Analog mV | Output cầu − của KMZ10A |
| **U3** | TP_INA_OUT | Analog ×38, DC~2.5V | Output INA826 |
| **U1** | TP_LMV358_OUT | Analog trimmed | Output LMV358 (trước comparator) |
| **U21** | TP_VTH_RPM | DC 0–5V | Ngưỡng comparator (đo khi chỉnh RP3) |
| **U2** | **TP_RPM_OUT** | **Digital 0/5V** | **Xung RPM cuối — quan trọng nhất** |
| GPIO33 | Trực tiếp | Digital 0/3.3V | Tín hiệu vào ESP32 |

---

## 3 Trimpot Điều Chỉnh

| Trimpot | Giá trị | Vị trí | Chức năng |
|---------|---------|--------|----------|
| **RP1** | 100K | LMV358 tầng 1 | **OFFSET** — cân bằng DC của tín hiệu analog |
| **RP2** | 100K | LMV358 tầng 2 | **GAIN** — biên độ tín hiệu trước comparator |
| **RP3** | 10K | Comparator IN− | **THRESHOLD** — ngưỡng kích comparator |

---

## Cài Đặt DSO152 Cho Từng Bước

| Bước | Probe tại | Coupling | Volt/div | Time/div | Trigger |
|------|-----------|----------|----------|----------|---------|
| 1a | TP_5V (U16) | DC | 2 V | 10 ms | Auto |
| 1b | TP_VREF (U14) | DC | 1 V | 10 ms | Auto |
| 2 | TP_INA_OUT (U3) | **AC** | 500 mV | 2 ms | Auto |
| 3 | TP_LMV358_OUT (U1) | **DC** | 1 V | 2 ms | Auto |
| 4 | TP_LMV358_OUT (U1) | DC | 1 V | 2 ms | Auto |
| 5 | TP_RPM_OUT (U2) | DC | 2 V | 2 ms | **Rising** |
| Cuối | GPIO33 | DC | 2 V | 2 ms | Rising |

> **Lưu ý**: GND probe **luôn cắm vào U15 (TP_GND)** trong toàn bộ quá trình.

---

## Quy Trình Hiệu Chỉnh 5 Bước

---

### BƯỚC 1 — Kiểm Tra Nguồn Điện

Thực hiện trước mọi thứ khác, không cần quay magnet.

**1a. Kiểm tra +5V**
- Probe → **U16 (TP_5V)**
- Kỳ vọng: đường thẳng phẳng tại **+5.0V ± 0.2V**
- Nếu dao động mạnh hoặc sai: kiểm tra BEC/nguồn cấp trước khi tiếp tục

**1b. Kiểm tra VREF**
- Probe → **U14 (TP_VREF)**
- Kỳ vọng: DC ổn định tại **2.4–2.6V** (chia đôi 5V qua R40/R39 = 4K7/4K7)
- Nếu sai: kiểm tra hàn R40, R39, C23, C1

**→ PASS cả 2: tiếp tục Bước 2**

---

### BƯỚC 2 — Xem Tín Hiệu INA826 (U3)

Mục đích: xác nhận KMZ10A đang nhận từ trường và INA826 hoạt động.

**Thao tác**:
- Probe → **U3 (TP_INA_OUT)**
- Dùng tay quay magnet (nam châm trong spinner nut) qua trước đầu cảm biến
  với tốc độ **~1–3 vòng/giây**
- Cài DSO152: **AC coupling** để loại bỏ DC 2.5V, chỉ thấy AC signal

**Kỳ vọng**: dạng sóng hình sin hoặc quasi-sin, đều, **biên độ 200mV–1Vpp**

**Nếu không thấy tín hiệu gì**:
- Kiểm tra khoảng cách magnet → sensor (thường ≤3mm)
- Thử xoay KMZ10A 90° (AMR nhạy hướng từ trường)
- Kiểm tra nguồn 5V vào U17 chân 4

**Nếu biên độ quá nhỏ (<100mVpp)**:
- Magnet quá yếu hoặc khoảng cách quá xa
- Thử magnet mạnh hơn để xác nhận mạch hoạt động

**→ PASS: thấy sóng rõ ràng khi quay → tiếp tục Bước 3**

---

### BƯỚC 3 — Chỉnh RP1 (Offset) Tại U1

Mục đích: căn chỉnh tín hiệu analog nằm đối xứng quanh VREF (2.5V)
để comparator hoạt động đúng khi magnet quay cả chiều N và S.

**Thao tác**:
- Chuyển probe → **U1 (TP_LMV358_OUT)**
- Đổi DSO152 sang **DC coupling** (phải thấy cả DC offset)
- Quay magnet đều liên tục ~2 vòng/giây
- **Vặn RP1** từ từ

**Mục tiêu**: đỉnh dương và đỉnh âm của sóng **cách đều 2.5V**

```
Trước chỉnh (RP1 lệch):        Sau chỉnh (đúng):
    4V ┐ đỉnh                      3.5V ┐ đỉnh
       │                                │
    2.5V ─── VREF                  2.5V ─── VREF (đúng tâm)
       │                                │
    2V ┘ đáy                       1.5V ┘ đáy
```

**Kiểm tra**: ghi điện áp đỉnh+ và đỉnh−. Hiệu với 2.5V phải bằng nhau
(ví dụ: đỉnh+ = 3.8V, đỉnh− = 1.2V → cả hai cách 2.5V đúng 1.3V → OK)

---

### BƯỚC 4 — Chỉnh RP2 (Gain) Tại U1

Mục đích: điều chỉnh biên độ tín hiệu để comparator kích đủ sạch.

**Thao tác**: vẫn probe tại **U1 (TP_LMV358_OUT)**, quay magnet đều

**Vặn RP2** để đạt biên độ **1–2Vpp**:
- Quá nhỏ (<0.5Vpp): comparator không kích đủ → không ra xung RPM
- Vừa (1–2Vpp): tối ưu — comparator kích rõ ràng với margin tốt
- Quá lớn (>3Vpp hoặc clipping): sóng bị cắt phẳng đỉnh → distortion → double-pulse

**Nhận biết clipping**: đỉnh sóng bị "phẳng" thay vì tròn → giảm RP2

**Kết quả tốt**:
```
    3.5V ┐ đỉnh tròn (không phẳng)
         │
    2.5V ─── VREF
         │
    1.5V ┘ đáy tròn (không phẳng)
    Biên độ = 2Vpp
```

---

### BƯỚC 5 — Chỉnh RP3 (Threshold) Và Xác Nhận Xung RPM_OUT

**Đây là bước quan trọng nhất** — quyết định chất lượng xung RPM digital.

**Thao tác**:
- Chuyển probe → **U2 (TP_RPM_OUT)**
- Cài DSO152: DC coupling, 2V/div, trigger **Rising**, 2ms/div
- Quay magnet đều ~2 vòng/giây

**Mục tiêu**: xung vuông sạch 0/5V, **đúng 1 xung mỗi cực magnet đi qua**

**Cách chỉnh RP3**:

```
RP3 quá THẤP (ngưỡng thấp hơn VREF):
  ┌──┐  ┌─┐┌┐  ← double-pulse hoặc xung rác
  │  │  │ ││ │     → TĂNG RP3 (vặn theo chiều kim đồng hồ)

RP3 ĐÚNG (1 xung sạch/vòng):
  ┌──┐     ┌──┐
  │  │     │  │  ← đẹp, đều, cách đều nhau
  ┘  └─────┘  └──

RP3 quá CAO (ngưỡng quá cao):
  (không thấy xung nào dù magnet đang quay)
       → GIẢM RP3 (vặn ngược chiều kim đồng hồ)
```

**Lề an toàn (margin)**: sau khi có xung đẹp:
1. Tiếp tục tăng RP3 từ từ cho đến khi xung **vừa biến mất** → ghi vị trí này
2. Quay ngược lại **1/4 vòng** (về phía giảm) → đây là điểm làm việc tối ưu
3. Margin này bảo vệ khỏi drift theo nhiệt độ và dao động nguồn

**Kiểm tra nhiễu EMI**:
- Vẫn probe tại U2, bật starter ESC ở mức thấp (~20%)
- Xem RPM_OUT có xuất hiện spike lạ không
- Nếu có spike → tăng RP3 thêm 1/8 vòng, hoặc tăng `rpmfilter` trong firmware

**Đọc VTH_RPM**:
- Probe tạm sang **U21 (TP_VTH_RPM)** để đọc giá trị ngưỡng bằng số
- Ghi lại điện áp VTH_RPM tại điểm làm việc tối ưu (để tham chiếu sau)

---

## Kiểm Tra Cuối — GPIO33 Với Firmware TEST_STARTER

Sau khi 3 trimpot đã chỉnh xong:

**1. Xác nhận tại GPIO33**
- Cắm cáp J_RPM_OUT1 → RPM_PIN_IN1 (kết nối về ECU board)
- Probe DSO152 trực tiếp vào chân **GPIO33** của NodeMCU-32S
- Kỳ vọng: xung **0–3.3V** (D1 TVS clamp giới hạn, không được vượt 3.3V)
- Nếu thấy xung vẫn 5V tại GPIO33: D1 bị lỗi → thay thế ngay trước khi cấp nguồn ESP32

**2. Xác nhận với firmware TEST_STARTER**
- Upload `TEST/TEST_STARTER/TEST_STARTER.ino` vào ESP32
- Serial Monitor 115200 baud
- Gõ: `rpmdetail on`
- Quay magnet bằng tay (1 vòng/giây ≈ 60 RPM)

**Kỳ vọng output Serial**:
```
PWM=1000us(OFF) | RPM=58 (win 61) | raw=12 acc=12 rej=0 (0.0%) | jit=3.1% cv=-- | NOISE=CLEAN STAB=SETTLING
```

**Tiêu chí PASS**:

| Chỉ số | Tiêu chí |
|--------|---------|
| RPM hiển thị | ~60 khi quay 1 vòng/giây |
| `rejectPct` | < 5% |
| `NOISE` | `CLEAN` |
| RPM khi không quay | = 0 (không có false pulse) |

**Nếu `rejectPct` > 10% dù đã chỉnh RP3**:
- Tăng firmware filter: gõ `filter 200` rồi `filter 300` thử dần
- Hoặc thêm tụ 100nF từ GPIO33 xuống GND (gần ESP32)

---

## Bảng Tóm Tắt Quy Trình

```
[B1] Kiểm tra nguồn: U16=5V?, U14=2.5V?
      ↓ PASS
[B2] INA_OUT (U3, AC coupling): thấy sóng sin khi quay magnet?
      ↓ PASS (biên độ >100mVpp)
[B3] Chỉnh RP1 (offset): LMV358_OUT (U1, DC) đối xứng quanh 2.5V
      ↓ Đỉnh+ và đỉnh− cách đều 2.5V
[B4] Chỉnh RP2 (gain): LMV358_OUT (U1) biên độ 1–2Vpp, không clipping
      ↓ OK
[B5] Chỉnh RP3 (threshold): RPM_OUT (U2) = xung vuông sạch 0/5V
     + Thêm margin 1/4 vòng
     + Test EMI với starter bật
      ↓ OK
[Cuối] GPIO33 = 0–3.3V?, TEST_STARTER NOISE=CLEAN rejectPct<5%?
      ↓ PASS → SẴN SÀNG TEST ENGINE
```

---

## Ghi Chú Kết Quả Hiệu Chỉnh

```
Ngày hiệu chỉnh: _______________
Người thực hiện: _______________

Bước 1:
  TP_5V  = _____ V   ☐ OK
  TP_VREF = _____ V  ☐ OK

Bước 2:
  INA_OUT biên độ = _____ Vpp  ☐ OK

Bước 3 (RP1 Offset):
  LMV358_OUT: đỉnh+ = ___V  đỉnh− = ___V  tâm = ___V  ☐ OK

Bước 4 (RP2 Gain):
  Biên độ = _____ Vpp  clipping? ☐Có ☐Không  ☐ OK

Bước 5 (RP3 Threshold):
  VTH_RPM tại điểm mất xung = _____ V
  VTH_RPM điểm làm việc (−1/4 vòng) = _____ V
  EMI test (starter 20%): spike? ☐Có ☐Không  ☐ OK

Kiểm tra cuối:
  GPIO33 max = _____ V (phải ≤ 3.3V)  ☐ OK
  TEST_STARTER rejectPct = _____ %  ☐ OK (< 5%)
  RPM khi quay 1 vòng/s = _____ (phải ≈ 60)  ☐ OK

Kết luận: ☐ PASS — Sẵn sàng test engine  ☐ FAIL — Xem ghi chú:
_________________________________________________
```

---

**Tài liệu liên quan**:
- `PRE_ENGINE_TEST_GUIDE.md` — Quy trình test tổng thể trước khi test engine
- `TEST/TEST_STARTER/TEST_STARTER.ino` — Firmware test RPM bench
- `CODE_REVIEW_FINDINGS.md` — Phân tích firmware

**Phiên bản**: 1.0 — 2026-07-16
