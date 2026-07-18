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

## 📊 Điểm An Toàn Tổng Thể (lúc review ban đầu, trước khi fix)

| Hạng mục | Điểm |
|----------|------|
| Logic state machine | 8/10 |
| EGT safety guards | 7/10 *(vấn đề #1 Web UI)* |
| RPM noise handling | 8/10 *(vấn đề #3, #8)* |
| Fuel control safety | 9/10 |
| Serial/Web interface safety | 6/10 *(vấn đề #2, #5)* |
| **Tổng** | **7.5/10** |

**Cập nhật**: cả 8 vấn đề trên đã được fix (xem checklist bên dưới) — bảng
điểm này chỉ còn giá trị lịch sử, không phản ánh trạng thái code hiện tại.

---

## 🚀 Quyết Định: Có Upload ESP32 Được Không?

**YES — đã fix toàn bộ 8/8 vấn đề vòng 1** (trước đây có 4 ràng buộc vận
hành tạm thời cho #1/#2/#5/#6, nay không còn cần thiết vì các lỗi gốc đã
được fix trong code thay vì né bằng quy trình vận hành).

---

## 📋 Fix Priority

```
Fix ngay (trước lab test):
[x] #5 Non-blocking serial command buffer

Fix trước engine test thực sự:
[x] #1 EGT guard trong stage2Off()
[x] #2 Require clearAbort trước startidle
[x] #3 resetRpmStats() trước wet start spin-up

Fix sau (ít quan trọng hơn):
[x] #4 Reset gradientCps khi EGT invalid
[x] #6 SD slot overflow handling — không còn ghi header mới đè vào ECU999.CSV khi đầy 1000 slot
[x] #7 Single SPI read cho readCelsius + readError — đổi thứ tự đọc readError() trước
[x] #8 Dùng stddev thay max-min cho jitter — coefficient of variation, không bị 1 outlier làm sai
```

**Tất cả 8 lỗi vòng 1 đã fix (2026-07-16).** Compile sạch (`g++ -fsyntax-only
-Wall -Wextra`, không warning).

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

# 🔁 Review Vòng 3 (2026-07-16) — So sánh với Rev11/Rev12_TC10, 2 lỗi đã fix

So sánh cấu trúc + an toàn với 2 bản firmware tham khảo
(`REFERENCES/Firmware/Rev11`, `REFERENCES/Firmware/Rev12_TC10`). Kết luận
chung: firmware hiện tại **an toàn hơn** cả 2 bản tham khảo ở mọi interlock
cốt lõi (abort cứng, cooldown thật, ARM/START 2 bước, RPM noise
classification, Test Wizard checklist). Tuy nhiên tìm được 2 khoảng trống
so với Rev11/12, cả 2 đã được fix:

| # | Vị trí | Lỗi | Cách fix |
|---|--------|-----|---------|
| 1 | Không có tương đương RC-signal-loss failsafe của Rev11/12 | `throttlePct` là giá trị chốt qua lệnh Serial/Web rời rạc; nếu mất kết nối Web UI/Serial giữa lúc `MODE_IDLING`/`MODE_OPERATING`, động cơ chạy mãi ở ga cuối cùng không ai giám sát | Thêm **comm watchdog**: `lastOperatorLinkMs` cập nhật ở mọi lệnh (`handleCommand()`) và mỗi lần Web UI poll `/api` (tự động mỗi 700ms khi tab mở). `checkFailures()` tự abort `COMM_TIMEOUT` nếu quá `cfg.commTimeoutMs` (mặc định 8000ms, chỉnh qua `set commtimeout <3000..60000>`) không có tín hiệu nào trong lúc IDLING/OPERATING. Tắt qua `set commwatchdog off` yêu cầu `arm2` trước (debug only) |
| 2 | `abortAll()` → `enterCooldown()` | Log snapshot lúc abort bị ghi **sau khi** `enterCooldown()` đã zero hóa `fuelTargetUs`/`pumpUs`/`ignCmd`/`startUs` → SD/event log thể hiện giá trị đã an toàn hóa, không phải giá trị thật lúc lỗi xảy ra, làm giảm giá trị chẩn đoán sự cố | `abortAll()` gọi `addLog("ABORT_SNAPSHOT reason=...")` **trước** khi gọi `enterCooldown()`, chụp đúng RPM/EGT/fuel/outputs tại thời điểm lỗi |

**Verify**: compile sạch (`g++ -fsyntax-only -Wall -Wextra`, mock ESP32/Servo/
MAX31855/WiFi/WebServer/SD, không warning).

---

# 🔧 Cập nhật vai trò Valve1/Valve2 (2026-07-16) — theo manual EnJet E86/G3

Đối chiếu `REFERENCES/Documentation/Engine_Manual_NewerModel.pdf` (EnJet G3
Series manual, có bảng thông số E86) xác nhận động cơ dùng **2 valve dầu**
(không phải gas): **Start solenoid valve** (mạch dầu khởi động riêng) +
**Main oil valve** (mạch dầu chính, luôn mở khi bơm chạy — theo mô tả
"Oil Pump Test" tự động liên kết mở Main valve trong manual).

Trước đây `fuelValvesAuto()` mở/đóng cả 2 valve đồng thời, không phân biệt
vai trò. Đã cập nhật:

```cpp
void fuelValvesAuto(bool on) {
  valve2Cmd = on;                              // Main: mở bất cứ khi nào có lệnh nhiên liệu
  valve1Cmd = on && (ecuMode == MODE_STARTING); // Start: chỉ mở trong lúc MODE_STARTING
}
```

- **VALVE1 (Start)** chỉ mở trong `ST_INTRO_FUEL → ST_POST_IGNITION_HEAT →
  ST_ACCEL_TO_IDLE` (toàn bộ `MODE_STARTING`), tự động đóng ngay khi chuyển
  sang `MODE_IDLING`.
- **VALVE2 (Main)** giữ hành vi cũ — mở bất cứ khi nào `fuelValvesAuto(true)`
  được gọi, bất kể mode (khớp `pumptest`/Test Wizard pump-prime = "Oil Pump
  Test" của Enjet, chỉ mở Main).
- Checklist đổi tên hiển thị `VALVE_1`→`VALVE1_START`, `VALVE_2`→`VALVE2_MAIN`
  để rõ vai trò trên Serial/Web UI.
- Đã rà soát toàn bộ điểm gọi `valve1Cmd`/`valve2Cmd` (rest-guard, fueled
  check trong `checkFailures()`, log) — không có điểm nào phụ thuộc hành vi
  "2 valve luôn giống nhau" nên không có tác dụng phụ.

**Lưu ý còn để ngỏ**: giả định trên dựa vào tài liệu chính hãng, chưa xác
nhận vật lý E86 đời cũ của người dùng có đúng 2 mạch dầu tách biệt hay
không — nên đối chiếu lại đường ống thật trước khi test có nhiên liệu.

**Verify**: compile sạch (`g++ -fsyntax-only -Wall -Wextra`, không warning).

---

# 🔧 ESP32Servo → LEDC + Starter Kick + Web UI cho TEST_STARTER (2026-07-17)

## Bối cảnh

Test bench với nguồn 12V 2A: starter khựng/giật giật rồi dừng dù firmware
vẫn gửi PWM đều đặn. Ban đầu chẩn đoán là thiếu dòng cấp nguồn (đúng, xem
mục "Nguồn & Pin" trong `COMMISSIONING_GUIDE.md`), nhưng người dùng thử
nghiệm 1 sketch riêng dùng LEDC PWM trực tiếp (không qua thư viện
`ESP32Servo`) — **chạy êm với CÙNG một nguồn 12V 2A**. Điều này cho thấy
`ESP32Servo` là một nguyên nhân riêng biệt, độc lập với nguồn điện — xem
chi tiết kỹ thuật trong `CLAUDE.md` ở gốc repo.

## Thay đổi

| # | File | Thay đổi |
|---|------|---------|
| 1 | `ECU_TestV1_EGT_DRY_START_PATCH.ino` | Bỏ `ESP32Servo`, chuyển pump+starter ESC sang LEDC PWM trực tiếp (`escAttach()`/`escWriteUs()`, tương thích cả ESP32 Arduino core 2.x và 3.x qua `ESP_ARDUINO_VERSION_MAJOR`) |
| 2 | `ECU_TestV1_EGT_DRY_START_PATCH.ino` | Thêm **starter kick**: `cfg.starterKickUs`/`cfg.starterKickMs` — xung cao ngắn (mặc định 1300us/300ms) ngay đầu `ST_PURGE` trước khi hạ về `starterPurgeUs`, giúp starter có cơ cấu Bendix/clutch ăn khớp dứt khoát. Lệnh mới: `set starterkickus`, `set starterkickms` |
| 3 | `ECU_TestV1_EGT_DRY_START_PATCH.ino` | Web UI dashboard: thêm mục "Starter Kick" (chỉnh kickUs/kickMs) và "Starter Manual Test" (chạy `starttest <us> <ms>` từ trình duyệt) |
| 4 | `TEST/TEST_STARTER/TEST_STARTER.ino` | Cùng 3 thay đổi trên: LEDC thay ESP32Servo, thêm kick (`kickus`/`kickms`), và thêm **Web UI đầy đủ** (SoftAP `TEST_STARTER`/`test1234`, dashboard + toàn bộ lệnh qua nút/ô nhập) — cho phép test không cần Serial Monitor |

**Verify**: compile sạch cả 2 file (`g++ -fsyntax-only -Wall -Wextra`, không warning).

**Lưu ý**: `starterKickMs=0` tắt hoàn toàn tính năng kick nếu không cần.

---

# 🔧 Gỡ starter kick + tăng giá trị test (2026-07-18)

Theo yêu cầu: **bỏ hẳn tính năng starter kick** (không cần công đoạn này nữa)
và nâng giá trị test.

| # | File | Thay đổi |
|---|------|----------|
| 1 | `ECU_TestV1_EGT_DRY_START_PATCH.ino` | Gỡ `cfg.starterKickUs`/`cfg.starterKickMs`, helper `starterKickOrSteady()`, 2 lệnh `set starterkickus`/`set starterkickms`, mục Web UI "Starter Kick", và dòng in trong printConfig/printHelp. `ST_PURGE` (cả start thật lẫn dry-start) giờ dùng thẳng `starterPurgeUs` |
| 2 | `ECU_TestV1_EGT_DRY_START_PATCH.ino` | **starterSpinUs 1150 → 1200** (test wizard "Starter" + spin thật + default Web UI "Starter Manual Test"). **introFuelUs 1160 → 1210** (pump prime test + intro fuel thật). Nới trần bench `pumptest` 1175 → **1225µs** để đạt mức mới |
| 3 | `TEST/TEST_STARTER/TEST_STARTER.ino` | Gỡ toàn bộ kick (globals, `applyPwm`/`setPwm`, lệnh `kickus`/`kickms`, trường JSON, mục Web UI, docstring). Default PWM Web UI 1150 → 1200 |

**Lưu ý**: `introFuelUs`(1210) hiện > `idleFuelUs`(1175) — theo quyết định "đổi
cả config chung"; ảnh hưởng cả chuỗi khởi động thật, không chỉ lệnh test.

**Không hard-code — chỉnh runtime**: các mức PWM starter/fuel giờ đổi được lúc
chạy, không cần build lại firmware:
- Lệnh mới: `set purgeus <us>`, `set spinus <us>`, `set assistus <us>` (1000..1500)
  cho starterPurge/Spin/Assist. `set intro`/`set idleus`/`set maxus` đã có sẵn.
- Web UI: thêm panel **"Starter & Fuel PWM"** với ô nhập cho purge/spin/assist +
  intro/idle/max, mỗi ô có nút Set gọi lệnh tương ứng.
- `printConfig` in thêm dòng `starter purge/spin/assist us=...`.
- Giá trị trong Config struct chỉ còn là **mặc định lúc khởi động**, ghi đè
  được bất cứ lúc nào qua Serial hoặc trình duyệt.

**Verify**: compile sạch cả 2 file (`g++ -fsyntax-only -Wall -Wextra`, không
warning) + mô phỏng chèn auto-prototype kiểu Arduino cho firmware chính (OK).

---

**Người review**: Code Review Agent (automated)  
**Phiên bản firmware**: ECU_TestV1_EGT_DRY_START_PATCH  
**Lần cập nhật**: 2026-07-18 (gỡ starter kick, starterSpinUs=1200, introFuelUs=1210)
