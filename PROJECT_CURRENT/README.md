# 🚀 PROJECT_CURRENT - Current Active Project

**Status**: Active Development  
**Last Updated**: 2026-07-16  
**Files**: 3 main files

---

## 📂 Contents

### 1. **ECU_TestV1_EGT_DRY_START_PATCH.ino**
```
File Size: 84.5 KB
Lines: ~1,900
Type: Arduino/ESP32 Firmware (Main)
```

**Features**:
- EGT-open dry-start mode (safety feature)
- ACCEL_TO_IDLE timeout (prevents fuel runaway)
- Real cooldown airflow management
- RPM noise diagnostics and guard
- Hybrid fuel control with calibration table
- Web UI with ESP32 SoftAP (ECU_TestV1 / admin1234)
- CSV telemetry SD logging
- Enhanced button logic (2-step ARM→START)

**Pin Configuration**:
- MAX31855 (EGT): CLK=18, CS=5, DO=19
- RPM Sensor: GPIO 33
- ESC Pump: GPIO 26
- ESC Starter: GPIO 25
- Valve 1: GPIO 17
- Valve 2: GPIO 16
- Ignition/Glow: GPIO 32
- User Button: GPIO 22 (active-low)
- Status LED: GPIO 2 (onboard)
- SD Card: CS=13, SCK=14, MOSI=23, MISO=27

**Web Interface**:
- URL: http://192.168.4.1
- SSID: ECU_TestV1
- Password: admin1234
- Features: Dashboard, Controls, Test Wizard, Event Log

---

### 2. **RPM_Sensor_20260709.net**
```
File Size: 5.5 KB
Type: PADS PCB Netlist Format
Purpose: RPM Sensor Circuit Reference
```

**Contains**:
- RPM sensor schematic definition
- Component connections
- PCB layout reference

---

### 3. **SCH_MinijetengineECU_20260709.json**
```
File Size: 139 KB
Type: JSON Schema
Purpose: ECU Design Reference
```

**Contains**:
- Complete ECU schematic in JSON format
- Component definitions
- Pinout specifications
- Circuit connections
- Design parameters

---

## 🔧 How to Use

### Uploading Firmware
1. Open `ECU_TestV1_EGT_DRY_START_PATCH.ino` in Arduino IDE
2. Select Board: **ESP32 DevKit V1** (or NodeMCU-32S)
3. Install required libraries:
   - SPI
   - SD
   - ESP32Servo
   - Adafruit_MAX31855
   - WiFi
   - WebServer

4. Upload to ESP32

### Accessing Web UI
1. Power on ESP32
2. Connect to WiFi: **ECU_TestV1**
3. Password: **admin1234**
4. Open browser: **http://192.168.4.1**

### Using Reference Schemas
- **RPM_Sensor_20260709.net**: Import into PADS PCB or reference for circuit design
- **SCH_MinijetengineECU_20260709.json**: Open with JSON viewer for schematic reference

---

## 📋 Key Parameters

### Safety Limits
```
MAX_RPM: Configurable via Web UI
MAX_TEMP_C: Configurable via Web UI
ACCEL_TO_IDLE_TIMEOUT: 20s (default)
```

### Control Ranges
```
ESC: 1000-2000 microseconds
Pump: Configurable (1000-1270 us typical)
Valves: ON/OFF (GPIO control)
Ignition: ON/OFF (GPIO control)
```

### Sensor Sampling
```
EGT: ~8 Hz (120ms period)
RPM: 10 Hz (100ms period)
SD Logging: 2 lines/sec (500ms period)
```

---

## ⚠️ Important Warnings

- **High Temperature Hazard**: Keep components away from engine exhaust
- **ESP32 Voltage**: 3.3V only - Use logic level converter for 5V devices
- **K-Type Thermocouple Only**: Do not use other types
- **Power Supply**: Use proper PSU rated for your application
- **Mechanical Safety**: Jet engine produces high thrust - secure all equipment

---

## 🚀 Next Steps

1. **Upload Firmware** to ESP32
2. **Access Web UI** and configure parameters
3. **Reference Schemas** for hardware integration
4. **Test on Actual Engine** following safety procedures

---

## 📞 Support

For technical questions or issues:
1. Check the Web UI dashboard for status
2. Review error codes in event log
3. Consult REFERENCES folder for design documentation
4. Refer to PROJECT_REVIEW.md for architecture overview

---

**Project Status**: 🟢 Active Development  
**Stability**: 🟠 Test Version (Use with caution)  
**Last Updated**: 2026-07-16
