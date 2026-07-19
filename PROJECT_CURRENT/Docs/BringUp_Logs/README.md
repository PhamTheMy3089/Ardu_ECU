# BringUp_Logs — Nhật ký bring-up phần cứng

Thư mục lưu các log/đo đạc thực tế trong quá trình đưa mạch vào vận hành
(hardware bring-up), dùng làm tham chiếu cho lần sau và cho việc debug.

| File | Nội dung |
|------|---------|
| `RPM_TEST_STARTER_log_20260719.txt` | Log Serial firmware `TEST_STARTER` quét PWM 1000→1220µs, đọc RPM/NOISE/STAB sau khi sửa lỗi D1 |
| `RPM_DSO152_measurements_20260719.txt` | Số đo DSO152 từng tầng mạch RPM (TP_5V, INA_OUT, LMV358_OUT, RPM_OUT, GPIO33) |

Xem thêm quy trình hiệu chỉnh trong `../COMMISSIONING_GUIDE.md` (Giai đoạn 2 — hiệu chỉnh RPM).
