# 📚 REFERENCES - Tham khảo từ Project Cũ

**Status**: Reference/Archive  
**Purpose**: Design reference and learning materials  
**Last Updated**: 2026-07-16

---

## 📂 Folder Structure

```
REFERENCES/
├── Documentation/          ← Hướng dẫn và tài liệu tham khảo
│   └── Manuals/
│       ├── Rev 9-10 ECU_Manual.md
│       └── ECU Rev11 Guide.pdf
│
├── Firmware/              ← Các phiên bản firmware cũ (tham khảo)
│   ├── Rev9/              ← Phiên bản cũ nhất (lưu trữ)
│   ├── Rev10/             ← Phiên bản cũ (lưu trữ)
│   ├── Rev11/             ← Phiên bản ổn định (tham khảo)
│   ├── Rev12_TC10/        ← Phiên bản test (tham khảo)
│   ├── TestV1_EGT_DRY_START/  ← Phiên bản phát triển cũ
│   └── README.md          ← So sánh phiên bản
│
└── Hardware/              ← Thiết kế phần cứng tham khảo
    ├── 3D Printable Files/
    │   ├── Bendix Sleeve.stl
    │   ├── Brushed motor mount.stl
    │   └── Brushless motor mount.stl
    ├── Components/        ← Hình ảnh linh kiện tham khảo
    │   ├── 5v-to-3-3v-logic-level-converter-500x500.jpg
    │   ├── CK1602 Power Supply.jpg
    │   ├── ESP32 Dev Kit.jpeg
    │   └── LR-7843.jpg
    ├── Schematics/        ← Các sơ đồ mạch cũ
    │   ├── ECU Rev9-10 Schematic.pdf
    │   └── Schematic_Ardu ECU_Rev 11.pdf
    └── README.md          ← Thông tin hardware tham khảo
```

---

## 🔍 Firmware Versions (For Reference)

| Version | Status | Purpose | Files |
|---------|--------|---------|-------|
| **Rev 9** | 📦 Archived | Very old design | .zip |
| **Rev 10** | 📦 Archived | Legacy reference | .zip |
| **Rev 11** | 📚 Reference | Stable design example | .ino + .h |
| **Rev 12 TC10** | 📚 Reference | Multi-sensor design | .ino + .h |
| **TestV1_EGT_DRY_START** | 📚 Reference | Older dev version | .ino |

**Note**: These are for reference only. Use the current project files in `/PROJECT_CURRENT/` for actual implementation.

---

## 🛠️ Hardware Reference Files

### 3D Printable Models
```
- Bendix Sleeve.stl          → Motor coupling component
- Brushed motor mount.stl    → Mount for brushed motor
- Brushless motor mount.stl  → Mount for brushless motor
```

### Component References
```
- Logic Level Converter   → 5V to 3.3V level shifting
- CK1602 Power Supply     → Power management reference
- ESP32 Dev Kit           → Microcontroller reference
- LR-7843                 → Additional component reference
```

### Circuit Schematics
```
- ECU Rev9-10 Schematic.pdf    → Older design reference
- Schematic_Ardu ECU_Rev 11.pdf → Reference design example
```

---

## 📖 How to Use This Reference Material

### ✅ DO:
- ✓ Use to understand design evolution
- ✓ Reference for architecture inspiration
- ✓ Check firmware features in different versions
- ✓ Study schematic designs for learning
- ✓ Examine 3D models for mechanical design

### ❌ DON'T:
- ✗ Don't use old firmware for new projects (use PROJECT_CURRENT)
- ✗ Don't assume old schematics are current
- ✗ Don't change firmware versions without understanding differences
- ✗ Don't copy code without verifying compatibility

---

## 🧭 Navigation Guide

**Looking for...**

| You want to... | Go to... |
|---|---|
| Current project files | `/PROJECT_CURRENT/` |
| Latest firmware | `/PROJECT_CURRENT/ECU_TestV1_EGT_DRY_START_PATCH.ino` |
| Stable firmware example | `/REFERENCES/Firmware/Rev11/` |
| Hardware design reference | `/REFERENCES/Hardware/Schematics/` |
| Old manuals/docs | `/REFERENCES/Documentation/Manuals/` |
| 3D print files | `/REFERENCES/Hardware/3D Printable Files/` |

---

## 📊 Version Comparison

### Features Across Versions

```
FEATURE              | Rev9 | Rev10 | Rev11 | Rev12 | TestV1
---------------------|------|-------|-------|-------|--------
Basic EGT control    |  ✓   |  ✓    |  ✓    |  ✓    |  ✓
RPM sensing          |  ✓   |  ✓    |  ✓    |  ✓    |  ✓
WiFi interface       |  ✗   |  ✓    |  ✓    |  ✓    |  ✓
SD card logging      |  ✗   |  ✗    |  ✓    |  ✓    |  ✓
Pressure sensor      |  ✗   |  ✗    |  ✗    |  ✓    |  ✗
Load cell support    |  ✗   |  ✗    |  ✗    |  ✓    |  ✗
Kero start mode      |  ✗   |  ✗    |  ✗    |  ✓    |  ✗
Dry-start mode       |  ✗   |  ✗    |  ✗    |  ✗    |  ✓
Cooldown airflow     |  ✗   |  ✗    |  ✗    |  ✗    |  ✓
RPM diagnostics      |  ✗   |  ✗    |  ✗    |  ✗    |  ✓
Web UI               |  ✗   |  ⚠️   |  ✓    |  ✓    |  ✓
```

---

## 📝 Manual References

- **Rev 9-10 ECU_Manual.md** → Describes features for Rev 9 and 10
- **ECU Rev11 Guide.pdf** → Detailed guide for Rev 11 firmware

**Note**: These are old and may not reflect current implementation.

---

## 🔗 Recommended Reading Order

For understanding the project evolution:

1. Start with `/PROJECT_CURRENT/README.md` (current project)
2. Review `/PROJECT_REVIEW.md` (comprehensive analysis)
3. Explore `/REFERENCES/Firmware/Rev11/` (stable reference design)
4. Check `/REFERENCES/Hardware/Schematics/` (design examples)
5. Study old `/REFERENCES/Documentation/` (historical context)

---

## ⚠️ Important Notes

- **These are reference files only** - Not for production use
- **Firmware versions may not compile** with current libraries
- **Hardware designs may be outdated** - Check current project
- **Use for learning and design inspiration only**
- **For actual implementation, use PROJECT_CURRENT files**

---

## 📚 Archive Information

| Item | Details |
|------|---------|
| **Archived Date** | 2026-07-16 |
| **Reason** | Reorganized to separate current project from references |
| **Status** | Read-only reference material |
| **Maintenance** | No updates planned |

---

**Last Updated**: 2026-07-16  
**Purpose**: Reference and Learning Material  
**Active Project**: See `/PROJECT_CURRENT/`
