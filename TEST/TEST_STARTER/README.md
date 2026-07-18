# TEST_STARTER — Test RPM theo PWM Starter

Firmware test độc lập để kiểm tra **cảm biến RPM + starter** trước khi chạy động cơ thật.  
**Không cần ARM, không có bảo vệ** — chỉ tập trung vào đo nhiễu và độ ổn định của tín hiệu RPM theo từng mức PWM.

---

## 🎯 Mục Đích

- Khi khởi động: **starter KHÔNG quay** (PWM = 1000µs).
- Tăng/giảm PWM starter bằng phím **`+`** và **`-`**.
- ESP32 quan sát RPM tương ứng với từng mức PWM và tự đánh giá:
  - **Nhiễu (NOISE)**: `CLEAN` / `WARN` / `NOISY` / `NO_SIGNAL`
  - **Ổn định (STAB)**: `STABLE` / `WARN` / `UNSTABLE` / `SETTLING` / `OFF`

---

## 🔌 Đấu Nối (chỉ 2 chân)

| Tín hiệu | GPIO ESP32 |
|----------|-----------|
| Starter ESC signal | **25** |
| RPM sensor (Hall) | **33** |

- GND chung giữa ESP32 ↔ ESC ↔ cảm biến.
- Nguồn ESC/BEC **tách riêng** với nguồn ESP32.
- Cảm biến RPM cần pull-up (thường có sẵn trên module Hall).

---

## 🌐 Test Qua Web UI (không cần Serial Monitor)

Firmware tự bật SoftAP ngay khi khởi động — dùng song song hoặc thay thế
Serial Monitor:

```
WiFi: TEST_STARTER  |  Pass: test1234
URL:  http://192.168.4.1
```

Dashboard hiện đầy đủ PWM/RPM/NOISE/STAB và có nút/ô nhập cho toàn bộ
lệnh (`+`/`-`/STOP, đặt PWM, step, filter, ppr, edge, reset) —
tiện khi test bằng điện thoại đứng cạnh bàn test thay vì cắm laptop.

---

## ⚠️ An Toàn

- **THÁO cánh/impeller** khỏi starter trước khi test — motor có thể quay nhanh.
- Cố định starter chắc chắn xuống bàn test.
- Đây là firmware **TEST**: không cooldown, không kiểm tra EGT, không ARM.
  **KHÔNG** dùng để chạy động cơ có nhiên liệu.

---

## 🚀 Cách Upload

1. Mở `TEST_STARTER.ino` trong Arduino IDE.
2. Board: **ESP32 Dev Module** (hoặc NodeMCU-32S).
3. Không cần thư viện ngoài — dùng LEDC PWM có sẵn trong ESP32 Arduino core.
4. Upload → Mở Serial Monitor: **115200 baud**, line ending = **Newline**.

---

## ⌨️ Lệnh

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

> **PWM engine**: firmware này dùng LEDC trực tiếp (`ledcAttach`/`ledcWrite`),
> không dùng thư viện `ESP32Servo` — xem `CLAUDE.md` ở gốc repo để biết lý do
> (ESP32Servo từng gây khựng starter độc lập với nguồn điện).

---

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

---

## 🧭 Ngưỡng Đánh Giá

### NOISE (dựa trên rejectPct + jitter)
| Kết quả | Điều kiện | Ý nghĩa |
|---------|-----------|---------|
| `CLEAN` | reject ≤ 5% và jit ≤ 15% | Tín hiệu tốt |
| `WARN` | reject > 5% hoặc jit > 15% | Nhiễu nhẹ, theo dõi |
| `NOISY` | reject > 20% hoặc jit > 40% (hoặc raw>0 mà acc=0) | Nhiễu nặng — cần sửa |
| `NO_SIGNAL` | không có cạnh nào | Chưa nhận tín hiệu |

### STAB (dựa trên CV% của RPM, chỉ tính sau khi PWM ổn định 1.5s)
| Kết quả | Điều kiện | Ý nghĩa |
|---------|-----------|---------|
| `STABLE` | CV ≤ 2% | RPM ổn định tốt |
| `WARN` | 2% < CV ≤ 5% | Dao động vừa |
| `UNSTABLE` | CV > 5% | RPM dao động mạnh |
| `SETTLING` | mới đổi PWM / chưa đủ mẫu | Đang chờ ổn định |
| `OFF` | PWM = 1000µs | Starter đang tắt |

---

## 📋 Quy Trình Test Gợi Ý

1. **Test tĩnh (PWM=OFF)**: Quan sát 30s. Kỳ vọng `raw≈0`, `NOISE=NO_SIGNAL`.
   Nếu có `acc>0` khi starter chưa quay → có nhiễu nền, kiểm tra wiring.
2. **Rà quét lên**: Bấm `+` từ từ (ví dụ mỗi lần +10µs từ 1100µs).
   Sau mỗi bước, chờ `STAB` chuyển từ `SETTLING` → xem `NOISE`/`STAB`.
3. **Ghi lại bảng** PWM → RPM → NOISE → STAB ở nhiều mức.
4. **Nếu `NOISY` ở PWM cao**: tăng filter (`filter 300`), thêm tụ 100nF tại GPIO33,
   tách dây tín hiệu RPM xa dây công suất ESC.
5. **Kết luận GO/NO-GO**: Toàn dải làm việc nên đạt `NOISE=CLEAN` và `STAB=STABLE`
   trước khi chuyển sang firmware ECU chính.

---

**Firmware**: TEST_STARTER.ino — bench test only  
**Cập nhật**: 2026-07-18
