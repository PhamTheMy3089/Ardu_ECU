# Firmware Directory

Thư mục chứa mã nguồn firmware cho các phiên bản khác nhau của ECU.

## 📋 Firmware Versions

### **Rev9** - Lưu trữ
- Phiên bản cũ nhất
- Được lưu trữ dưới dạng ZIP
- Không nên sử dụng cho ứng dụng mới

### **Rev10** - Lưu trữ
- Cải tiến từ Rev9
- Được lưu trữ dưới dạng ZIP
- Chỉ dùng cho tham khảo

### **Rev11** ⭐ Ổn định hiện tại
- **Status**: Phiên bản ổn định chính
- **Hardware**: Major redesign
- **Features**:
  - SD Card logging
  - Enhanced web interface
  - Temperature gradient monitoring
  - Fuel solenoid capability
  - WiFi SSID/Password customization
  - Magnetic/Optical RPM sensor support

**Files**:
- `ECU_Rev11.ino` - Main firmware
- `page1.h` to `page8.h` - Configuration pages

### **Rev12_TC10** 🧪 Test Version
- **Status**: Test/Development
- **Features**:
  - More sensors support
  - Pressure sensor reading
  - Load cell integration
  - Kero Start capability
  - New starting methods (open loop, pressure-based)
  - Configuration file save/load

**Files**:
- `ECU_Rev12_TC10.ino` - Test firmware
- `page1.h` to `page9.h` - Extended configuration (9 pages)

## 🔧 How to Use

1. **For Production**: Use `Rev11` - proven stable version
2. **For Testing**: Use `Rev12_TC10`
3. **For Reference**: Check older versions in archives
4. **For the active project**: see `/PROJECT_CURRENT/Firmware/ECU_TestV1_EGT_DRY_START_PATCH/` (not a reference version — this is the live firmware)

## 📝 Uploading Firmware

1. Open the `.ino` file in Arduino IDE
2. Ensure all dependencies are installed
3. Select ESP32 DevKit V1 as board
4. Select appropriate COM port
5. Upload to ESP32

## 📚 Dependencies

All firmware versions use common libraries:
- ESP32 core libraries
- U8g2lib (display)
- MAX31855 (thermocouple)
- ArduinoJson
- AsyncWebServer
- And others (see main README)

---

For more information, see [README.md](../README.md)
