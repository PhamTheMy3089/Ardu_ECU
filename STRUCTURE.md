# Project Structure Guide

Hướng dẫn cấu trúc thư mục dự án Ardu_ECU

## 📁 Directory Organization

```
Ardu_ECU/
│
├── 📄 README.md                    # Main project documentation
├── 📄 STRUCTURE.md                 # This file - folder structure guide
│
├── 📂 Documentation/               # Project documentation and guides
│   ├── Manuals/
│   │   ├── Rev 9-10 ECU_Manual.md
│   │   └── ECU Rev11 Guide.pdf
│   └── (More documentation can be added here)
│
├── 📂 Hardware/                    # All hardware-related files
│   ├── Schematics/               # Circuit diagrams and schemas
│   │   ├── ECU Rev9-10 Schematic.pdf
│   │   ├── Schematic_Ardu ECU_Rev 11.pdf
│   │   ├── RPM_Sensor_20260709.net      # Reference schematic
│   │   └── SCH_MinijetengineECU_20260709.json  # Reference schema
│   │
│   ├── Components/               # Component datasheets and images
│   │   ├── 5v-to-3-3v-logic-level-converter-500x500.jpg
│   │   ├── CK1602 Power Supply.jpg
│   │   ├── ESP32 Dev Kit.jpeg
│   │   └── LR-7843.jpg
│   │
│   └── 3D Printable Files/       # STL files for 3D printing
│       ├── Bendix Sleeve.stl
│       ├── Brushed motor mount.stl
│       └── Brushless motor mount.stl
│
├── 📂 Firmware/                    # Firmware for different ECU revisions
│   ├── Rev9/                      # ECU Revision 9 (archived)
│   │   └── ECU_Rev9.zip
│   │
│   ├── Rev10/                     # ECU Revision 10 (archived)
│   │   └── ECU_Rev10.zip
│   │
│   ├── Rev11/                     # ECU Revision 11 (Current Stable)
│   │   ├── ECU_Rev11.ino          # Main firmware file
│   │   ├── page1.h through page8.h # Configuration pages
│   │   └── (Additional support files)
│   │
│   ├── Rev12_TC10/               # ECU Revision 12 (Test Version)
│   │   ├── ECU_Rev12_TC10.ino     # Test firmware
│   │   ├── page1.h through page9.h # Extended configuration pages
│   │   └── (Additional support files)
│   │
│   └── TestV1_EGT_DRY_START/      # New Test Version (Current Development)
│       ├── ECU_TestV1_EGT_DRY_START_PATCH.ino
│       └── (Related firmware files)
│
└── 📂 .git/                        # Git repository metadata (hidden)
```

## 📊 Folder Descriptions

### 📄 Documentation/
Tất cả các tài liệu hướng dẫn và sách hướng dẫn cho dự án.
- **Manuals/**: Tài liệu chi tiết cho từng revision

### 📂 Hardware/
Tất cả các tệp liên quan đến phần cứng.
- **Schematics/**: Schema mạch điện và bản vẽ thiết kế
- **Components/**: Thông số kỹ thuật và hình ảnh linh kiện
- **3D Printable Files/**: Các file STL để in 3D

### 📂 Firmware/
Mã nguồn firmware cho các phiên bản khác nhau.
- **Rev9, Rev10**: Phiên bản cũ (lưu trữ)
- **Rev11**: Phiên bản ổn định hiện tại
- **Rev12_TC10**: Phiên bản thử nghiệm mới
- **TestV1_EGT_DRY_START**: Phiên bản phát triển hiện tại

## 🔄 Recent Changes (2026-07-16)

✅ **Reorganized project structure**:
- Created `Documentation/` folder for guides and manuals
- Created `Hardware/` folder with organized subfolders
  - `Schematics/` - All circuit diagrams in one place
  - `Components/` - Component references and images
  - `3D Printable Files/` - 3D models
- Created `Firmware/` folder with revision-based organization
  - Each revision in its own subfolder (Rev9, Rev10, Rev11, Rev12_TC10, TestV1_EGT_DRY_START)
- Removed duplicate files and eliminated confusing folder nesting
- Added reference files (RPM_Sensor, ECU schema) to Hardware/Schematics/

### Changes Made:
- ✅ Moved firmware files from duplicate locations to centralized `Firmware/` folder
- ✅ Organized schematics and hardware documentation
- ✅ Consolidated component references
- ✅ Added new test version files to proper location

## 🚀 Next Steps

1. **Review all files** to ensure they are in correct locations
2. **Update documentation** with new folder structure
3. **Standardize naming** conventions if needed
4. **Add README files** to each major folder for clarity

## 📝 Version Info

- **Project**: Ardu_ECU - Arduino based ECU for model Jet Engines
- **Current Stable Release**: Rev 11
- **Current Test Version**: Rev 12 TC10
- **Latest Development**: TestV1_EGT_DRY_START
- **Last Updated**: 2026-07-16

---

For more details, see [README.md](README.md)
