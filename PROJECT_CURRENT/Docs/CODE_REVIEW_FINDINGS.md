# Kết Quả Code Review - ECU_TestV1_EGT_DRY_START_PATCH.ino

**Ngày review**: 2026-07-16  
**File**: `ECU_TestV1_EGT_DRY_START_PATCH.ino` (1,919 dòng)  
**Trạng thái**: Sẵn sàng upload ESP32 — với lưu ý các vấn đề bên dưới

---

## 🔴 HIGH — Lỗi Logic An Toàn Nghiêm Trọng

### 1. `stage2Off()` không kiểm tra nhiệt độ EGT — Dòng 485
**Vấn đề**: Lệnh `off` qua Serial và nút "SAFE OFF" trên Web UI đều gọi `stage2Off()` → `enterWaitingSafe()` mà **không kiểm tra EGT còn nóng không**.  
**Nguy hiểm**: Sau khi bị ABORT do OVER_TEMP (EGT 650°C), operator có thể nhấn "SAFE OFF" → vào WAITING → re-arm ngay lập tức trong khi động cơ vẫn rất nóng.  
**Đường an toàn hiện tại**: Button vật lý kiểm tra `egtSafeForAbortClear()` (dòng 589) — nhưng Web UI thì không.  
**Khuyến nghị**: Thêm kiểm tra EGT vào `stage2Off()` trước khi gọi `enterWaitingSafe()`.

```cpp
// FIX gợi ý tại dòng ~485:
void stage2Off() {
  if (!egtSafeForAbortClear()) {
    Serial.println("STAGE2 OFF blocked: EGT still hot");
    return; // Không cho phép tắt khi EGT còn nóng
  }
  enterWaitingSafe();
}
```

---

### 2. Restart từ ABORTED không yêu cầu xác nhận lỗi — Dòng 1395
**Vấn đề**: `canStartAutoIdle()` cho phép start khi `ecuMode == MODE_ABORTED`. Chỉ kiểm tra EGT an toàn (dòng 1402), không yêu cầu operator xác nhận nguyên nhân abort (OVERSPEED, RPM_SIGNAL_LOST, v.v.).  
**Nguy hiểm**: Operator có thể gõ `startidle` ngay sau khi bị OVERSPEED abort — restart ngay mà không hiểu tại sao máy dừng.  
**Khuyến nghị**: Yêu cầu `clearAbort` command riêng trước `startidle`, hoặc tối thiểu log cảnh báo rõ ràng.

---

## 🟡 MEDIUM — Vấn Đề Ảnh Hưởng Đến Độ Tin Cậy

### 3. Pulse đầu tiên sau `resetRpmStats()` bỏ qua bộ lọc glitch — Dòng 740–745
**Vấn đề**: Trong ISR, khi `isrLastRawEdgeUs == 0` (sau `resetRpmStats()`), pulse đầu tiên được chấp nhận vô điều kiện, bất kể độ rộng.  
**Ảnh hưởng**: EMI spike từ igniter ở đầu giai đoạn `ST_SPINUP_PREHEAT` (wet start) có thể được đếm là RPM hợp lệ, che khuất thực tế starter không quay đủ.  
**Khuyến nghị**: Gọi `resetRpmStats()` trước cả wet start spin-up (tương tự dry start ở dòng 1457), không chỉ dry start.

### 4. `egt.gradientCps` bị stale khi EGT sensor lỗi — Dòng 882–886
**Vấn đề**: Khi `egt.ok = false`, biến `gradientCps` không được reset về 0. Các hàm control đọc `egt.ok` trước (an toàn), nhưng Web UI `/api` và `printStatus()` hiển thị gradient cũ mà không đánh dấu là invalid.  
**Ảnh hưởng**: Operator thấy gradient đang hiển thị "30 °C/s" nhưng thực ra EGT sensor đã bị lỗi từ lâu — gây nhầm lẫn khi debug.  
**Khuyến nghị**: Reset `egt.gradientCps = 0` khi `egt.ok = false` trong `updateEgt()`.

### 5. `Serial.readStringUntil('\n')` có thể block loop tới 1 giây — Dòng 1897
**Vấn đề**: Hàm `readStringUntil` chờ đến 1000ms nếu không có newline (gõ chậm, kết nối bị ngắt quãng).  
**Ảnh hưởng**: Trong 1 giây đó, `updateEgt()`, `updateRpm()`, `checkFailures()`, và `applyOutputs()` đều không chạy — **mất phản hồi an toàn trong 1 giây**.  
**Khuyến nghị**: Thay bằng non-blocking serial command buffer, đọc từng ký tự với `Serial.read()` và chỉ xử lý khi có `\n`.

---

## 🟢 LOW — Cải Tiến Nhỏ

### 6. SD file slot bị đầy → ghi đè file cũ — Dòng 995–998
Nếu đã có đủ 1000 files (ECU000.CSV đến ECU999.CSV), vòng lặp tìm tên file kết thúc với `i=999` vẫn trỏ vào file có sẵn. Header mới sẽ được append vào cuối file đó, làm hỏng log cũ.

### 7. Double SPI read → fault code có thể sai — Dòng 880–881
`thermo.readCelsius()` và `thermo.readError()` là 2 transaction SPI riêng biệt. Nếu lỗi thoáng qua giữa 2 lần đọc, `egt.fault = 0` trong khi `egt.ok = false` → hiển thị "SPI/WIRING" thay vì tên lỗi thực.

### 8. Jitter metric bị phóng đại bởi một outlier — Dòng 846–847
`jitterPct = (maxDt - minDt) / avgInterval * 100`. Một khoảng dài bất thường duy nhất trong cửa sổ 100ms làm jitter tăng cao sai, có thể báo `RPM_WARN` / `RPM_NOISY` khi tín hiệu thực ra sạch — có thể block start qua `rpmNoiseBlocksStart()`.

---

## ✅ Điểm Tốt — Giữ Nguyên

| Tính năng | Đánh giá |
|-----------|----------|
| REST RPM guard | Xuất sắc — loại bỏ false pulse khi mọi output OFF |
| Dry-start khi EGT OPEN | Đúng và an toàn — chạy starter, tắt nhiên liệu/van |
| `egtAllowsFuelIncrease()` lookahead 3s | Logic rõ ràng, đúng |
| ACCEL_TO_IDLE timeout 20s | Cần thiết, được implement tốt |
| Cooldown với starter chạy | Đúng — làm mát sau stop |
| Glow preheat trước khi có nhiên liệu | Đúng (SPINUP_PREHEAT → ignCmd=true trước fuel) |
| Pump calibration table | Khớp chính xác với đo thực tế |
| ISR `IRAM_ATTR` | Đúng — ISR trong IRAM |
| Button 2-step ARM→START | An toàn |

---

## 📊 Điểm An Toàn Tổng Thể

| Hạng mục | Điểm |
|----------|------|
| Logic state machine | 8/10 |
| EGT safety guards | 7/10 *(vấn đề #1 Web UI)* |
| RPM noise handling | 8/10 *(vấn đề #3, #8)* |
| Fuel control safety | 9/10 |
| Serial/Web interface safety | 6/10 *(vấn đề #2, #5)* |
| **Tổng** | **7.5/10** |

---

## 🚀 Quyết Định: Có Upload ESP32 Được Không?

**YES — Có thể upload, nhưng vận hành với các ràng buộc sau**:

| # | Ràng buộc vận hành | Lý do |
|---|-------------------|-------|
| 1 | **Không dùng Web "SAFE OFF" khi EGT > 100°C** | Vấn đề #1: không có EGT guard |
| 2 | **Sau mỗi ABORT, đọc kỹ lý do abort trên Serial/Web trước khi re-arm** | Vấn đề #2: restart không yêu cầu xác nhận |
| 3 | **Không gõ command Serial chậm/ngắt quãng khi engine đang chạy** | Vấn đề #5: block loop 1s |
| 4 | **Không để SD đầy 1000 files** | Vấn đề #6: ghi đè log |

**Các vấn đề HIGH #1 và #2 nên được fix trước khi test engine thực sự (không cấp bách cho lab test).**

---

## 📋 Fix Priority

```
Fix ngay (trước lab test):
[ ] #5 Non-blocking serial command buffer

Fix trước engine test thực sự:
[ ] #1 EGT guard trong stage2Off()
[ ] #2 Require clearAbort trước startidle
[ ] #3 resetRpmStats() trước wet start spin-up

Fix sau (ít quan trọng hơn):
[ ] #4 Reset gradientCps khi EGT invalid  
[ ] #6 SD slot overflow handling
[ ] #7 Single SPI read cho readCelsius + readError
[ ] #8 Dùng median thay max-min cho jitter
```

---

# 🔁 Review Vòng 2 (2026-07-16) — 9 lỗi mới, đã fix toàn bộ

Sau khi fix 8 lỗi vòng 1, chạy thêm một vòng review sâu (8 finder angles +
verify) và tìm được **9 lỗi mới**. Tất cả đã được fix và firmware đã
compile sạch (`g++ -fsyntax-only -Wall -Wextra`, không warning).

## 🔴 CRITICAL — An toàn nhiên liệu

| # | Vị trí | Lỗi | Cách fix |
|---|--------|-----|---------|
| 1 | `pumptest` / `test pump` | Mở van + bơm nhiên liệu khi ở MODE_ABORTED mà không kiểm tra EGT → có thể phun nhiên liệu vào buồng đốt còn nóng sau abort OVER_TEMP | Thêm `fuelCommandBlockedByHotEgt()`: chặn khi EGT đọc được và > `cooldownTargetC` |
| 2 | `checkFailures()` | Guard RPM_SIGNAL_LOST bỏ sót `ST_INTRO_FUEL`/`ST_POST_IGNITION_HEAT` → mất tín hiệu RPM khi van nhiên liệu đang mở không bị abort trong tối đa 6s | Guard mới bao trùm cả `MODE_STARTING` (mọi stage có nhiên liệu) |

## 🟠 HIGH — Độ tin cậy RPM & state machine

| # | Vị trí | Lỗi | Cách fix |
|---|--------|-----|---------|
| 3 | `rpmISR()` | Glitch bị reject vẫn cho glitch kế tiếp lọt qua filter → phantom RPM → có thể kích OVERSPEED giả | Thêm **adaptive mask**: reject cạnh đến sớm hơn ½ chu kỳ hợp lệ gần nhất (turbine không thể tăng đôi tốc độ trong 1 vòng) |
| 4 | `updateRpm()` | `nowUs = micros()` lấy trước `noInterrupts()` → ISR chen giữa gây wraparound uint32 → `signalRecent=false` giả → abort RPM_SIGNAL_LOST oan | Dời `micros()` ra **sau** critical section (đảm bảo `nowUs >= lastPulseUs`) |
| 5 | `egtSafeForAbortClear()` | Cảm biến EGT hỏng vĩnh viễn (`egt.ok=false`) chặn TẤT CẢ đường thoát MODE_ABORTED → kẹt cứng, phải cúp nguồn | Thêm `egtAllowsDeliberateAbortClear()` + lệnh `clearabort force` (chỉ dry-start sau đó) |
| 6 | `beginAutoIdle()` | Không xóa `activeTest`/`activeTestEndMs` → timer test đang chạy bắn giữa chu trình start thật, dừng starter | Xóa `activeTest = TEST_NONE; activeTestEndMs = 0;` khi bắt đầu start |
| 7 | `ST_INTRO_FUEL` dòng 1542 | Check NO_RPM_RISE (`fuelConfirmTimeoutMs`=10s) là **dead code** vì NO_IGNITION (6s) luôn abort trước | Chuyển check sang neo theo `ignitionDetectedMs`, chạy trong POST_IGN + ACCEL_TO_IDLE (`checkPostIgnitionRpmRise()`) |

## 🟡 MEDIUM — Hiệu năng / độ bền

| # | Vị trí | Lỗi | Cách fix |
|---|--------|-----|---------|
| 8 | `sdAppendLine()` | SD open/close đồng bộ mỗi event → block main loop 10–100ms trên thẻ chậm, trễ `checkFailures()` | Hàng đợi SD event: snapshot lúc phát sinh, ghi (blocking) trong loop **sau** safety check |
| 9 | `webStatusJson()` | ~25 lần `String +=` không `reserve()` → phân mảnh heap mỗi 700ms poll → nguy cơ Guru Meditation | Thêm `s.reserve(896)` |

## ✅ Kết luận vòng 2

- Firmware **compile sạch** với mock ESP32/Servo/MAX31855/WiFi/WebServer/SD.
- Đã trace toàn bộ luồng: `WAITING → PURGE → SPINUP_PREHEAT → INTRO_FUEL
  (đánh lửa) → POST_IGNITION_HEAT → ACCEL_TO_IDLE → IDLING`. Các fix
  **không phá vỡ** luồng chạy tới đánh lửa và idle.
- Các interlock để chạy wet start thật: `arm2` → `autostart on` → chạy đủ
  Test Wizard + `confirmkill` → `startidle`.

**Còn lại (tùy chọn, không chặn test)**: dry-start hiện bỏ qua toàn bộ
checklist kể cả `TEST_KILL` — cân nhắc vẫn yêu cầu xác nhận kill switch cho
dry-start vì starter vẫn quay trục thật (rủi ro thấp: dry-start không có
nhiên liệu/lửa).

---

**Người review**: Code Review Agent (automated)  
**Phiên bản firmware**: ECU_TestV1_EGT_DRY_START_PATCH  
**Lần cập nhật**: 2026-07-16 (vòng 2)
