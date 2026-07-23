# Bridge GSU — UI điều khiển ECU (Dây hoặc WiFi)

Firmware ESP32 **không còn phục vụ trang web riêng** nữa — nó chỉ còn API JSON
nhẹ (`/api`, `/cmd`) qua WiFi và lệnh `apijson` qua Serial. Toàn bộ **giao diện
điều khiển nằm ở công cụ này** (`page.html`), chạy trên PC, và nói chuyện với
ECU theo **một trong hai kiểu kết nối**:

| Kiểu | Đường truyền | Terminal thấy được gì |
|------|--------------|------------------------|
| **Dây** | Serial/USB-TTL vào `GSU_DEBUG_UART1` | TẤT CẢ dòng Serial thô: log khởi động, **log brownout/reset**, phản hồi lệnh |
| **WiFi** | SoftAP của ECU (`/api`,`/cmd` qua HTTP) | Chỉ Event Log của ECU (WiFi không sống sót qua lúc reset nên không bắt được log đó) |

Mở trình duyệt vào `http://127.0.0.1:8080/` sau khi chạy — giao diện giống nhau
ở cả 2 kiểu, có thêm tab **🖥 Terminal** để đọc dữ liệu ECU và gõ lệnh trực tiếp.

## Cách 1 — Windows, chạy trực tiếp, không cần cài gì (khuyên dùng)

Dùng `serial_web_bridge.exe` (build sẵn từ Go, chỉ dùng thư viện chuẩn, không
phụ thuộc Python/pip). Double-click để chạy — cửa sổ console sẽ hỏi:
1. Kiểu kết nối: `wire` (dây) hay `wifi`.
2. Nếu `wire`: cổng COM (vd `COM5`). Nếu `wifi`: IP của ECU (mặc định `192.168.4.1`).

Rồi mở `http://127.0.0.1:8080/`.

Chạy nhanh không cần nhập tay:
```
serial_web_bridge.exe wire COM5
serial_web_bridge.exe wifi 192.168.4.1
serial_web_bridge.exe wire COM5 115200 8080     (thêm baud, http-port)
```

Build lại từ source (khi bạn sửa page.html/thêm tính năng), trên máy có cài Go:
```
GOOS=windows GOARCH=amd64 go build -o serial_web_bridge.exe .
```
`page.html` được nhúng thẳng vào .exe lúc build (`//go:embed`), nên sau khi build
chỉ cần 1 file .exe, không cần kèm page.html.

## Cách 2 — Python (Linux/macOS, hoặc muốn sửa nhanh)

```
pip install pyserial          # chỉ cần cho chế độ Dây
python serial_web_bridge.py               # hỏi kiểu kết nối tương tác
python serial_web_bridge.py wire /dev/ttyUSB0
python serial_web_bridge.py wifi 192.168.4.1
```
(Bản Python đọc `page.html` cạnh nó, nên giữ 2 file cùng thư mục.)

## Đấu dây (chế độ Dây)

`GSU_DEBUG_UART1`: chỉ 3 dây — **GND + TX↔RX + RX↔TX** (chéo), **không nối VCC,
không nối DTR**. Nếu module USB-TTL ra mức 5V trên TX thì thêm cầu chia áp
1kΩ/2kΩ trước chân RX của ESP32 (ESP32 không chịu 5V). Không cắm USB on-board
của ESP32 cùng lúc (chung UART0).

## Endpoint nội bộ (bridge phục vụ)

- `GET /` — trang UI (`page.html`).
- `GET /api` — trạng thái JSON (dây: từ `apijson`; wifi: proxy từ ECU).
- `GET /cmd?c=<lệnh>` — gửi lệnh tới ECU.
- `GET /term?since=<n>` — lấy các dòng terminal mới từ chỉ số `n` (dùng cho tab Terminal).

## Yêu cầu firmware

- Lệnh Serial `apijson` (in ra JSON giống `/api`) — có sẵn trong
  `ECU_TestV1_EGT_DRY_START_PATCH.ino`, dùng cho chế độ Dây.
- SoftAP + `/api` + `/cmd` — có sẵn, dùng cho chế độ WiFi.

> Lưu ý: phần gọi cổng COM của Windows (kernel32) chưa chạy thử trên máy Windows
> thật từ môi trường build; logic HTTP/JSON/terminal đã test qua cổng serial ảo.
> Nếu mở cổng COM báo lỗi, kiểm tra cổng có bị chương trình khác giữ không.
