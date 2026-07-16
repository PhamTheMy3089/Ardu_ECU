# Hướng Dẫn Commissioning ECU — Từ Test Cơ Bản Đến Chạy Thật Có Tăng Throttle

**Firmware**: `ECU_TestV1_EGT_DRY_START_PATCH.ino`
**Đối tượng**: Vận hành lần đầu trên phần cứng thật, đi từng bước an toàn.

> Nguyên tắc xuyên suốt: **không nhảy cóc bước**. Mỗi giai đoạn dưới phải
> PASS hết rồi mới sang giai đoạn kế tiếp. Nếu bất kỳ bước nào FAIL — dừng,
> tìm nguyên nhân, không ép qua bằng debug override.

---

## Giai đoạn 0 — Chuẩn bị trước khi cấp điện

- [ ] Kill switch vật lý (NC) đã lắp và test cơ học (không dựa vào firmware)
- [ ] Bình lửa/CO2 trong tầm với
- [ ] Bàn test cố định động cơ chắc chắn, không ai đứng ngay trục quay
- [ ] Chưa gắn ống dẫn nhiên liệu vào engine thật (test bench trước)
- [ ] Thẻ SD đã format FAT32, gắn vào ECU
- [ ] Nạp firmware mới nhất (đã fix 9 lỗi vòng 2) qua Arduino IDE

---

## Giai đoạn 1 — Kiểm tra tĩnh (không cấp nhiên liệu, không quay starter)

Kết nối Serial Monitor 115200 baud hoặc mở Web UI `192.168.4.1`.

| Bước | Lệnh | Kỳ vọng |
|------|------|---------|
| 1.1 | `status` | ECU in ra MODE=WAITING, không lỗi |
| 1.2 | `showcfg` | Xem toàn bộ config mặc định, ghi lại để đối chiếu sau |
| 1.3 | `test egt` | EGT=OK và nhiệt độ phòng (~20-30°C). Nếu FAULT → kiểm tra dây MAX31855 trước khi đi tiếp |
| 1.4 | `test rpm_noise` | RNOISE=CLEAN hoặc NO_SIGNAL (không quay gì cả → NO_SIGNAL là bình thường) |
| 1.5 | `sdstatus` rồi `sdtest` | SD=OK, file ECU0xx.CSV được tạo, event ghi được |

**PASS điều kiện**: cả 5 mục trên không có lỗi. Đây là bước bắt buộc trước
khi chạm vào bất kỳ thứ gì có chuyển động.

---

## Giai đoạn 2 — DSO152 chỉnh cảm biến RPM (KMZ10A)

Theo `DSO152_RPM_CALIBRATION_GUIDE.md` đầy đủ 5 bước (RP1 offset, RP2 gain,
RP3 threshold). **Không bỏ qua giai đoạn này** — RPM sai sẽ làm mọi guard
an toàn phía sau (OVERSPEED, RPM_SIGNAL_LOST, FLAMEOUT) hoạt động sai.

Xác nhận cuối:
```
rpmdetail on
```
Quay tay đều ~1 vòng/giây → Serial phải hiện RPM ≈ 60 (với ppr=1),
`NOISE=CLEAN`, `rej%` thấp (<5%).

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
test starter_ign  -> starter + glow cùng lúc, kiểm tra EMI có làm nhiễu RPM không (đây chính là finding #3 vòng 1 đã fix — resetRpmStats() trước bước này)
arm2
test valve1
arm2
test valve2
```

Với mỗi test, dùng `checklist` để xem kết quả:
```
checklist
```
Tất cả phải chuyển từ `NOT_RUN` → `PASS`. Nếu `FAIL` — sửa phần cứng trước
khi tiếp tục (đừng chỉnh firmware để né test).

---

## Giai đoạn 4 — Test bơm nhiên liệu riêng (KHÔNG gắn ống vào engine)

⚠️ Tháo ống dẫn nhiên liệu ra khỏi engine, xả vào cốc hứng riêng.

```
arm2
pumptest 1100 1500   -> bơm chạy 1500ms ở 1100us, đo ml thực tế đối chiếu bảng calib
```

So khớp với **pump calibration table** trong firmware. Lặp lại 2-3 mức us
khác nhau để xác nhận độ tuyến tính. Đây là bước cuối để pass `TEST_PUMP`
trong checklist:
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

Cách kích hoạt dry-start: rút giắc/ để hở mạch cảm biến EGT tạm thời (hoặc
dùng `set egtstart dry` để cho phép), lúc đó `startidle` sẽ tự động thành
dry-run (không nhiên liệu/van/mồi lửa vì `dryStartActive=true`).

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

---

## Giai đoạn 6 — Wet-start (khởi động thật, có nhiên liệu + lửa)

**Chỉ làm khi**: Giai đoạn 1-5 đều PASS, EGT gắn đúng và đọc chính xác,
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
RPM_SIGNAL_LOST, OVERSPEED...) — **đọc kỹ lý do trên Serial trước khi
`clearabort`**. Không type `clearabort` theo phản xạ.

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

**Giảm ga về idle**:
```
set throttle 0
```
→ tự động `MODE_OPERATING → MODE_IDLING`.

**Dừng máy an toàn (có làm mát)**:
```
stop
```
→ vào `MODE_COOLDOWN`, starter quay làm mát, tắt nhiên liệu/van, chờ
`cooldownTargetC` rồi mới tắt hẳn.

**Dừng khẩn cấp**: nhấn kill switch vật lý (NC) — bypass hoàn toàn
firmware, ngắt điện trực tiếp.

---

## Bảng Tóm Tắt Thứ Tự Toàn Bộ

```
0. Chuẩn bị an toàn (kill switch, bình lửa, cố định engine)
1. Test tĩnh: EGT, RPM_NOISE, SD                          → không quay gì
2. DSO152 chỉnh RP1/RP2/RP3                               → RPM sensor chính xác
3. Test Wizard từng bộ phận (ign/starter/starter_ign/valve1/valve2)
4. Test bơm riêng (pumptest, không gắn engine) + confirmkill
5. Dry-start (quay thật, không nhiên liệu/lửa)
6. Wet-start (đánh lửa thật) → IDLING ổn định
7. Tăng throttle từ 10% → 100% từng nấc, theo dõi EGT/RPM
```

---

## Lưu Ý An Toàn Xuyên Suốt

- **Không** dùng `set checklist off` khi test thật — chỉ dùng khi debug bench.
- **Không** dùng `clearabort force` trừ khi chắc chắn máy đã nguội và cảm
  biến EGT hỏng vĩnh viễn — lệnh này bỏ qua xác nhận nhiệt độ qua sensor.
- Sau **mọi ABORT**, đọc lý do (`lastAbortReason`) trước khi re-arm, đừng
  `clearabort` phản xạ.
- Không gõ lệnh Serial chậm/ngắt quãng khi engine đang chạy thật.
- Không để SD đầy 1000 file log.
- Throttle luôn tăng/giảm từng nấc nhỏ, không set thẳng 100% từ IDLING.

---

**Tài liệu liên quan**:
- `DSO152_RPM_CALIBRATION_GUIDE.md` — chi tiết giai đoạn 2
- `CODE_REVIEW_FINDINGS.md` — các lỗi đã fix, ảnh hưởng đến độ tin cậy các bước trên
