# Serial ↔ Web Bridge

Chạy trang điều khiển **y hệt trang Web của ECU** (cùng file `page.html`,
tách trực tiếp từ `htmlPage()` trong firmware) nhưng đi qua **cổng Serial**
(USB-TTL nối vào `GSU_DEBUG_UART1`) thay vì WiFi — dùng khi WiFi/SoftAP của
ECU không phản hồi (ví dụ nghi ngờ brownout khi starter chạy PWM cao), vẫn
điều khiển/theo dõi được vì không phụ thuộc WiFi.

## Đấu dây

Xem `GSU_DEBUG_UART1` trong `PROJECT_CURRENT/Docs/COMMISSIONING_GUIDE.md` /
lịch sử trao đổi trong session — chỉ 3 dây: **GND + TXD↔RX + RXD↔TX** (chéo),
**không nối VCC, không nối DTR**. Nếu module USB-TTL ra mức 5V trên TXD, cần
cầu chia áp 1kΩ/2kΩ trước khi vào chân RX của ESP32 (ESP32 không chịu 5V).

## Cách 1 — Windows, chạy trực tiếp, không cần cài gì (khuyên dùng)

Dùng `serial_web_bridge.exe` (build sẵn từ `main.go` + `serial_windows.go`,
không phụ thuộc thư viện ngoài, không cần Python/pip). Double-click để chạy —
cửa sổ console sẽ hỏi tên cổng COM (vd `COM5`), baud (Enter = mặc định 115200),
port HTTP (Enter = mặc định 8080), rồi tự mở sẵn sàng — mở trình duyệt vào
`http://127.0.0.1:8080/`.

Cũng có thể chạy kèm tham số để khỏi phải nhập tay:
```
serial_web_bridge.exe COM5 115200 8080
```

Muốn tự build lại từ source (nếu bạn sửa `page.html`/thêm tính năng):
```
GOOS=windows GOARCH=amd64 go build -o serial_web_bridge.exe .
```
(chạy lệnh này trên máy có cài Go — bản .exe kết quả thì không cần Go nữa).

## Cách 2 — Python (Linux/macOS, hoặc muốn sửa code nhanh)

```
pip install pyserial
python serial_web_bridge.py <cổng-serial> [--baud 115200] [--http-port 8080]
```

Ví dụ:
```
python serial_web_bridge.py /dev/ttyUSB0
python serial_web_bridge.py COM5
```

Cả 2 cách đều mở `http://127.0.0.1:<port>/` — giao diện/nút bấm y hệt trang
`http://192.168.4.1/` của ECU.

## Yêu cầu firmware

Cần lệnh Serial `apijson` (đã có sẵn trong
`ECU_TestV1_EGT_DRY_START_PATCH.ino`) — in ra đúng chuỗi JSON mà route
`/api` của web server trả về, dùng riêng cho bridge này (không tính là "lệnh
điều khiển" nên không ảnh hưởng comm-watchdog/nút bấm như một lệnh thật).

## Giới hạn

- Đây là công cụ dự phòng khi WiFi có sự cố, không thay thế theo dõi qua
  Serial Monitor thường (không hiện các dòng `Serial.println` dạng log/chữ,
  chỉ hiện đúng những gì trang Web hiện vốn hiển thị).
- Không chạy đồng thời 2 bridge/2 Serial Monitor trên cùng 1 cổng COM.
