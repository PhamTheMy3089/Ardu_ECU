# Hướng Dẫn Hiệu Chỉnh & Xác Nhận Cảm Biến RPM KMZ10A (DSO152 + TEST_STARTER)

**Firmware chính**: ECU_TestV1_EGT_DRY_START_PATCH
**Firmware test bench**: `TEST/TEST_STARTER/TEST_STARTER.ino`
**Cảm biến**: KMZ10A (AMR magnetoresistive)
**Oscilloscope**: DSO152 (1 kênh, BW ~200kHz)
**Ngày**: 2026-07-16

> Tài liệu này gộp **2 giai đoạn** thành 1 quy trình liền mạch:
> **Giai đoạn A** — chỉnh 3 trimpot bằng oscilloscope DSO152 (phần cứng analog).
> **Giai đoạn B** — dùng firmware bench `TEST_STARTER.ino` quét toàn dải PWM
> starter để xác nhận RPM/nhiễu/độ ổn định bằng số liệu firmware thực tế,
> trước khi chuyển sang firmware ECU chính.

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

# GIAI ĐOẠN A — Hiệu Chỉnh Analog Bằng Oscilloscope (5 Bước)

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

**Xác nhận nhanh tại GPIO33** (trước khi sang Giai đoạn B):
- Cắm cáp J_RPM_OUT1 → RPM_PIN_IN1 (kết nối về ECU board)
- Probe DSO152 trực tiếp vào chân **GPIO33** của NodeMCU-32S
- Kỳ vọng: xung **0–3.3V** (D1 TVS clamp giới hạn, không được vượt 3.3V)
- Nếu thấy xung vẫn 5V tại GPIO33: **D1 bị lỗi** → thay thế ngay trước khi
  cấp nguồn ESP32/upload firmware bên dưới

---

# GIAI ĐOẠN B — Quét Toàn Dải PWM Bằng Firmware TEST_STARTER

Sau khi 3 trimpot đã chỉnh xong bằng oscilloscope (chỉ xác nhận bằng tay
quay ở tốc độ thấp), giai đoạn này dùng firmware bench riêng
`TEST/TEST_STARTER/TEST_STARTER.ino` để **quét toàn dải PWM starter thật**
và để chính firmware tự động đánh giá nhiễu/ổn định bằng số liệu, thay vì
chỉ quan sát bằng mắt trên oscilloscope ở tốc độ tay quay chậm.

## 🎯 Mục Đích

- Khi khởi động: **starter KHÔNG quay** (PWM = 1000µs).
- Tăng/giảm PWM starter bằng phím **`+`** và **`-`**.
- ESP32 quan sát RPM tương ứng với từng mức PWM và tự đánh giá:
  - **Nhiễu (NOISE)**: `CLEAN` / `WARN` / `NOISY` / `NO_SIGNAL`
  - **Ổn định (STAB)**: `STABLE` / `WARN` / `UNSTABLE` / `SETTLING` / `OFF`

## 🔌 Đấu Nối (chỉ 2 chân, giống mạch RPM chính)

| Tín hiệu | GPIO ESP32 |
|----------|-----------|
| Starter ESC signal | **25** |
| RPM sensor (từ RPM_OUT qua J_RPM_OUT1) | **33** |

- GND chung giữa ESP32 ↔ ESC ↔ cảm biến.
- Nguồn ESC/BEC **tách riêng** với nguồn ESP32 — starter cần dòng lớn
  (xem phần nguồn/pin đã bàn riêng), không được cấp chung với 5V logic ESP32.

## ⚠️ An Toàn — Bắt Buộc Trước Khi Bật Starter

- **THÁO cánh/impeller** khỏi starter trước khi test — motor có thể quay nhanh.
- Cố định starter chắc chắn xuống bàn test.
- Đây là firmware **TEST**: không cooldown, không kiểm tra EGT, không ARM.
  **KHÔNG** dùng để chạy động cơ có nhiên liệu.
- Nguồn cấp cho starter phải đủ dòng (xem phần đánh giá pin/nguồn) — nguồn
  yếu (ví dụ adapter dòng thấp) sẽ làm starter khựng/giật giật giả, dễ
  nhầm là lỗi cảm biến RPM trong khi thực chất là thiếu dòng cấp.

## 🚀 Cách Upload

1. Mở `TEST/TEST_STARTER/TEST_STARTER.ino` trong Arduino IDE.
2. Board: **ESP32 Dev Module** (hoặc NodeMCU-32S).
3. Cần thư viện: **ESP32Servo**.
4. Upload → Mở Serial Monitor: **115200 baud**, line ending = **Newline**.

## ⌨️ Bảng Lệnh Đầy Đủ

| Lệnh | Chức năng |
|------|-----------|
| `+` | Tăng PWM starter 1 bước (mặc định 10µs) |
| `-` | Giảm PWM starter 1 bước |
| `0` hoặc `s` | Dừng starter (PWM về 1000µs) |
| `step <us>` | Đổi bước tăng/giảm (1..200) |
| `pwm <us>` | Đặt PWM trực tiếp (1000..2000) |
| `ppr <n>` | Số xung/vòng (1=nam châm, 2=quang học) |
| `filter <us>` | Bộ lọc glitch RPM (20..2000), mặc định 120 |
| `edge rising\|falling` | Cạnh kích RPM |
| `reset` | Xóa bộ đếm & lịch sử RPM |
| `status` | In trạng thái ngay |
| `help` | In lại menu |

> Mẹo: `+` và `-` xử lý ngay từng ký tự, có thể bấm liên tục để rà quét PWM.

## 📊 Đọc Dòng Trạng Thái

Ví dụ output (in 2 lần/giây):

```
PWM=1200us | RPM=8450 (win 8410) | raw=141 acc=141 rej=0 (0.0%) | jit=4.2% cv=1.1% | NOISE=CLEAN STAB=STABLE
```

| Trường | Ý nghĩa |
|--------|---------|
| `PWM` | Mức PWM starter hiện tại (µs). `(OFF)` khi = 1000µs |
| `RPM` | RPM theo chu kỳ tức thời (phản ứng nhanh) |
| `win` | RPM trung bình cửa sổ 100ms (chéo kiểm) |
| `raw` | Số cạnh thô cảm biến bắt được / cửa sổ |
| `acc` | Số xung được chấp nhận sau bộ lọc |
| `rej` | Số xung bị lọc (nhiễu hẹp) |
| `(%)` | Tỉ lệ xung bị lọc = `rej/raw` |
| `jit` | Jitter chu kỳ = `(max-min)/avg` — dao động thời gian |
| `cv` | Hệ số biến thiên RPM gần đây (stddev/mean) — độ ổn định |
| `NOISE` | Đánh giá nhiễu tổng hợp |
| `STAB` | Đánh giá độ ổn định tại PWM hiện tại |

## 🧭 Ngưỡng Đánh Giá Tự Động

**NOISE** (dựa trên rejectPct + jitter):

| Kết quả | Điều kiện | Ý nghĩa |
|---------|-----------|---------|
| `CLEAN` | reject ≤ 5% và jit ≤ 15% | Tín hiệu tốt |
| `WARN` | reject > 5% hoặc jit > 15% | Nhiễu nhẹ, theo dõi |
| `NOISY` | reject > 20% hoặc jit > 40% (hoặc raw>0 mà acc=0) | Nhiễu nặng — cần sửa |
| `NO_SIGNAL` | không có cạnh nào | Chưa nhận tín hiệu |

**STAB** (dựa trên CV% của RPM, chỉ tính sau khi PWM ổn định 1.5s):

| Kết quả | Điều kiện | Ý nghĩa |
|---------|-----------|---------|
| `STABLE` | CV ≤ 2% | RPM ổn định tốt |
| `WARN` | 2% < CV ≤ 5% | Dao động vừa |
| `UNSTABLE` | CV > 5% | RPM dao động mạnh |
| `SETTLING` | mới đổi PWM / chưa đủ mẫu | Đang chờ ổn định |
| `OFF` | PWM = 1000µs | Starter đang tắt |

## 📋 Quy Trình Quét Khuyến Nghị

1. **Test tĩnh (PWM=OFF)**: Quan sát 30s. Kỳ vọng `raw≈0`, `NOISE=NO_SIGNAL`.
   Nếu có `acc>0` khi starter chưa quay → có nhiễu nền, kiểm tra wiring/RP3
   trước khi quét tiếp (quay lại Giai đoạn A, Bước 5).
2. **Rà quét lên**: Bấm `+` từ từ (ví dụ mỗi lần +10µs từ 1100µs).
   Sau mỗi bước, chờ `STAB` chuyển từ `SETTLING` → xem `NOISE`/`STAB`.
3. **Ghi lại bảng** PWM → RPM → NOISE → STAB ở nhiều mức (dùng bảng ghi
   chú ở cuối tài liệu).
4. **Nếu `NOISY` ở PWM cao**: tăng filter (`filter 300`), thêm tụ 100nF tại
   GPIO33, tách dây tín hiệu RPM xa dây công suất ESC. Nếu vẫn không hết,
   quay lại Giai đoạn A và tăng thêm margin RP3.
5. **Kết luận GO/NO-GO**: Toàn dải làm việc nên đạt `NOISE=CLEAN` và
   `STAB=STABLE` trước khi chuyển sang firmware ECU chính
   (`ECU_TestV1_EGT_DRY_START_PATCH.ino`).

---

## Bảng Tóm Tắt Toàn Bộ Quy Trình (A + B)

```
GIAI ĐOẠN A — Oscilloscope DSO152
[A1] Kiểm tra nguồn: U16=5V?, U14=2.5V?
      ↓ PASS
[A2] INA_OUT (U3, AC coupling): thấy sóng sin khi quay magnet?
      ↓ PASS (biên độ >100mVpp)
[A3] Chỉnh RP1 (offset): LMV358_OUT (U1, DC) đối xứng quanh 2.5V
      ↓ Đỉnh+ và đỉnh− cách đều 2.5V
[A4] Chỉnh RP2 (gain): LMV358_OUT (U1) biên độ 1–2Vpp, không clipping
      ↓ OK
[A5] Chỉnh RP3 (threshold): RPM_OUT (U2) = xung vuông sạch 0/5V
     + Thêm margin 1/4 vòng + Test EMI với starter bật
      ↓ OK
[A-cuối] GPIO33 = 0–3.3V (D1 clamp đúng)?
      ↓ PASS

GIAI ĐOẠN B — Firmware TEST_STARTER (quét toàn dải PWM thật)
[B1] Test tĩnh PWM=OFF: raw≈0, NOISE=NO_SIGNAL?
      ↓ PASS
[B2] Rà quét PWM tăng dần bằng phím +, ghi bảng PWM→RPM→NOISE→STAB
      ↓ Toàn dải NOISE=CLEAN, STAB=STABLE
[B3] Test EMI: bật starter, kiểm tra spike trên RPM_OUT/GPIO33
      ↓ Không spike bất thường
      ↓ PASS → SẴN SÀNG chuyển sang firmware ECU chính, tiếp tục
              PRE_ENGINE_TEST_GUIDE.md / COMMISSIONING_GUIDE.md
```

---

## Ghi Chú Kết Quả Hiệu Chỉnh

### Giai đoạn A — Trimpot

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

Kiểm tra cuối Giai đoạn A:
  GPIO33 max = _____ V (phải ≤ 3.3V)  ☐ OK
```

### Giai đoạn B — Quét PWM bằng TEST_STARTER

Ghi lại nhiều mức PWM trong dải làm việc thực tế của starter:

| PWM (µs) | RPM | raw | acc | rej% | jit% | cv% | NOISE | STAB |
|----------|-----|-----|-----|------|------|-----|-------|------|
| 1000 (OFF) | | | | | | | | |
| 1100 | | | | | | | | |
| 1150 | | | | | | | | |
| 1200 | | | | | | | | |
| 1250 | | | | | | | | |
| ___ | | | | | | | | |

```
RPM khi quay tay 1 vòng/s (ppr=1) ≈ 60?  ☐ OK
EMI test lúc starter chạy: spike trên RPM_OUT/GPIO33?  ☐Có ☐Không  ☐ OK

Kết luận: ☐ PASS — Sẵn sàng test engine  ☐ FAIL — Xem ghi chú:
_________________________________________________
```

---

**Tài liệu liên quan**:
- `PRE_ENGINE_TEST_GUIDE.md` — Quy trình test tổng thể trước khi test engine
- `COMMISSIONING_GUIDE.md` — Quy trình từ test cơ bản tới chạy thật có throttle
- `TEST/TEST_STARTER/TEST_STARTER.ino` — Firmware test RPM + starter bench
  (đã gộp đầy đủ cách dùng vào Giai đoạn B ở trên)
- `CODE_REVIEW_FINDINGS.md` — Phân tích firmware ECU chính

**Phiên bản**: 2.0 — 2026-07-16 (gộp TEST_STARTER vào quy trình DSO152)
