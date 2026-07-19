# BringUp_Logs — Nhật ký bring-up phần cứng

Thư mục lưu các log/đo đạc thực tế trong quá trình đưa mạch vào vận hành
(hardware bring-up), dùng làm tham chiếu cho lần sau và cho việc debug.

| File | Nội dung |
|------|---------|
| `RPM_TEST_STARTER_log_20260719.txt` | Log Serial firmware `TEST_STARTER` quét PWM 1000→1220µs, đọc RPM/NOISE/STAB sau khi sửa lỗi D1 |
| `RPM_TEST_STARTER_log_20260719_run2.txt` | Run 2: quét rộng 1150→1300µs + coast-down — bằng chứng nhiễu EMI motor/ESC |
| `RPM_DSO152_measurements_20260719.txt` | Số đo DSO152 từng tầng mạch RPM (TP_5V, INA_OUT, LMV358_OUT, RPM_OUT, GPIO33) |

## Bài học chốt lại (bring-up RPM)

- **Sửa D1 thành công**: xung magnet đã tới GPIO33 (3.36V sạch); D1 (TVS) cũ sai/hỏng
  kẹp mức cao xuống ~2V < ngưỡng VIH ESP32. **Phải lắp lại TVS 3.3V đúng loại** trước
  khi chạy động cơ thật.
- **RPM ghim cứng ~1500 khi ESC chạy nhưng đọc đúng (giảm dần) khi ESC tắt** (run 2)
  → **nhiễu EMI từ motor/ESC** lấn át tín hiệu RPM, KHÔNG phải lỗi firmware/nam châm.
  Xử lý: **star ground**, bọc shield + tách xa dây RPM khỏi dây motor, lắp lại D1,
  thêm lọc tại GPIO33.
- **LMV358 (U1) bão hòa thành sóng vuông là bình thường** (tín hiệu mạnh) — đừng ép về sine.
- **RP1/RP2/RP3 là trimpot 3296 25 vòng** — phải vặn nhiều vòng mới thấy đổi; RP3 đổi
  duty/độ sạch xung chứ không đổi mức cao.

Xem thêm quy trình hiệu chỉnh trong `../COMMISSIONING_GUIDE.md` (Giai đoạn 2 — hiệu chỉnh RPM).
