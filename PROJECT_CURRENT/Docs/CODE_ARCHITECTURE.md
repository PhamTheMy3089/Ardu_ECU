# ECU TestV1 Firmware - Code Architecture & Understanding Guide

**Firmware**: `ECU_TestV1_EGT_DRY_START_PATCH.ino`  
**Size**: 1,919 lines  
**Platform**: ESP32 (NodeMCU-32S)  
**Language**: Arduino C++

---

## 🏗️ Overall Architecture

The firmware is organized around a **State Machine** model with the following key concepts:

```
┌─────────────────────────────────────────────┐
│         Main Loop (240 Hz)                  │
├─────────────────────────────────────────────┤
│ 1. Read EGT & RPM sensors                   │
│ 2. Update state machine                     │
│ 3. Calculate fuel target                    │
│ 4. Update outputs (ESC, ignition, valves)   │
│ 5. Handle button inputs                     │
│ 6. Update Web UI                            │
│ 7. Log to SD card                           │
│ 8. Update status LED                        │
└─────────────────────────────────────────────┘
```

---

## 🎯 Core State Machine

### **ECU Modes** (Lines 169)
```cpp
enum EcuMode {
  MODE_WAITING,    // Idle, waiting for user command
  MODE_STARTING,   // Starting sequence in progress
  MODE_IDLING,     // Idle stable, waiting for throttle
  MODE_OPERATING,  // Active operation
  MODE_COOLDOWN,   // Post-stop cooling with starter running
  MODE_ABORTED     // Error state
}
```

### **Start Stages** (Lines 170)
```cpp
enum StartStage {
  ST_NONE,                 // Not starting
  ST_PURGE,                // Purge engine (starter only)
  ST_SPINUP_PREHEAT,       // Spin up + preheat
  ST_INTRO_FUEL,           // First fuel injection
  ST_POST_IGNITION_HEAT,   // Post-ignition heating
  ST_ACCEL_TO_IDLE         // Accelerate to idle RPM
}
```

### **Mode Transitions**
```
WAITING
  ↓ (user presses button 3s)
STARTING (ST_PURGE → ST_SPINUP → ST_INTRO_FUEL → ST_POST_IGN_HEAT → ST_ACCEL_TO_IDLE)
  ↓ (idle RPM reached)
IDLING
  ↓ (throttle applied)
OPERATING
  ↓ (soft stop / error)
COOLDOWN (starter runs, fuel OFF) / ABORTED
  ↓ (cooldown complete or manual clear)
WAITING
```

---

## 📊 Data Structures

### **1. Configuration Structure** (Lines 102-167)
```cpp
struct Config {
  // Safety & Start Requirements
  bool autoStartEnabled;              // Auto-start on power-up
  bool requireEgtForStart;            // Require EGT valid for start
  bool allowDryStartWhenEgtFault;     // Allow dry-start if EGT sensor open
  bool requireRpmForStart;            // Require RPM increase for fuel
  
  // Temperature Control
  int ignitionThresholdC;             // Min temp for ignition
  int maxEgtC;                        // Max safe EGT
  int startTargetEgtC;                // Target during start
  int maxTempGradientCps;             // Max temp rise rate
  
  // RPM Control
  int maxRpm;                         // Max safe RPM
  int idleRpm;                        // Target idle RPM
  int rpmTolerance;                   // RPM deadband
  
  // Timing Parameters
  uint32_t purgeTimeMs;               // Purge duration
  uint32_t preheatMs;                 // Preheat duration
  uint32_t accelToIdleTimeoutMs;      // Max time to reach idle
  
  // Cooldown Parameters
  uint32_t cooldownMinMs;             // Min cooldown time
  int cooldownTargetC;                // Target cooldown temp
  
  // Fuel Control
  int introFuelUs;    // ~50 ml/min
  int idleFuelUs;     // ~80 ml/min
  int maxFuelUs;      // ~280 ml/min
};
```

### **2. EGT State** (Lines 217-222)
```cpp
struct EgtState {
  bool ok;                    // Valid reading
  float c;                    // Current temperature (°C)
  float prevC;                // Previous temperature
  float gradientCps;          // Rate of change (°C/sec)
  uint8_t fault;              // Fault code from MAX31855
  uint32_t lastReadMs;        // Last read time
  uint32_t lastGoodMs;        // Last valid reading time
};
```

### **3. RPM State** (Lines 237-261)
```cpp
struct RpmState {
  float rpm;                  // Current RPM value
  float avgIntervalUs;        // Average pulse interval (microseconds)
  float jitterPct;            // Pulse timing variation (%)
  float rejectPct;            // Rejected pulses (%)
  RpmNoiseLevel noise;        // Signal quality classification
  bool signalRecent;          // Signal received recently
  bool restGuardActive;       // REST mode noise guard enabled
  // ... (debug/diagnostic fields)
};
```

---

## 🔌 Pin Configuration (Lines 44-58)

| Pin | Function | Direction | Type |
|-----|----------|-----------|------|
| GPIO 18 | EGT CLK | OUT | SPI |
| GPIO 5 | EGT CS | OUT | SPI |
| GPIO 19 | EGT DO | IN | SPI |
| GPIO 33 | RPM Sensor | IN | Digital (ISR) |
| GPIO 26 | Pump ESC | OUT | PWM (1000-2000 µs) |
| GPIO 25 | Starter ESC | OUT | PWM (1000-2000 µs) |
| GPIO 17 | Valve 1 | OUT | Digital |
| GPIO 16 | Valve 2 | OUT | Digital |
| GPIO 32 | Ignition/Glow | OUT | Digital |
| GPIO 22 | User Button | IN | Digital (active-low) |
| GPIO 2 | Status LED | OUT | Digital (active-low) |
| **SPI (SD Card)** | | | |
| GPIO 13 | SD CS | OUT | SPI |
| GPIO 14 | SD SCK | OUT | SPI |
| GPIO 23 | SD MOSI | OUT | SPI |
| GPIO 27 | SD MISO | IN | SPI |

---

## 🔋 Key Components & Systems

### **1. EGT (Exhaust Gas Temperature) System**
**File Location**: Lines 217-222, 394-401

**Components**:
- `Adafruit_MAX31855` thermocouple reader
- K-type thermocouple
- SPI communication

**Key Functions**:
```cpp
egtFaultString(uint8_t f)          // Decode MAX31855 fault codes
```

**Reading Cycle**:
- Polling period: 120ms (EGT_READ_PERIOD_MS)
- Fault detection: SPI/wiring, open circuit, shorts
- Gradient monitoring: Detect sharp temperature rises
- Lookahead: 3-second prediction to prevent overshoot

**Safety Features**:
- Dry-start mode if EGT is OPEN (sensor fault)
- Temperature gradient limits (200°C/sec default)
- Maximum EGT limit (680°C default)

### **2. RPM Sensing System**
**File Location**: Lines 224-261, 375-391

**Components**:
- ISR (Interrupt Service Routine) on GPIO 33
- Hardware interrupt for precise pulse timing
- Real-time noise filtering

**RPM Calculation**:
```cpp
1. Measure pulse interval (microseconds)
2. Calculate RPM: RPM = (60,000,000 µs/min) / (pulse_interval µs)
3. Apply noise filter (120 µs minimum pulse width)
4. Classify signal quality (CLEAN, WARN, NOISY, REST_NOISE)
```

**Noise Classification**:
```cpp
RPM_CLEAN       → <5% rejected, <15% jitter → Safe to use
RPM_WARN        → 5-20% rejected, 15-30% jitter → Caution
RPM_NOISY       → >20% rejected, >30% jitter → Block start
RPM_REST_NOISE  → Isolated pulses at rest → Block start
RPM_NO_SIGNAL   → No pulses received → Cannot start
```

**REST Guard Feature**:
When ECU is WAITING and all outputs OFF, any isolated pulses are rejected. This prevents 1-3 false pulses from being misinterpreted as engine RPM.

### **3. Fuel Control System**
**File Location**: Lines 403-421, 152-163

**Hybrid Fuel Control Model**:
```
Target RPM
    ↓
Fuel Target (fuelTargetUs) ← Compare RPM to idle target
    ↓
Actual Pump Output (pumpUs) ← Ramp slowly (1 µs per step)
```

**Pump Calibration Table** (Lines 96-100):
```cpp
// Interpolates between these points
1000 µs  → 0.0 ml/min   (idle safe)
1160 µs  → 50.0 ml/min  (intro fuel)
1175 µs  → 80.0 ml/min  (idle)
1250 µs  → 265.0 ml/min
1260 µs  → 280.0 ml/min (max)
```

**Key Functions**:
```cpp
flowFromUs(int us)      // Convert microseconds to flow rate
usFromFlow(float ml)    // Convert flow rate to microseconds
```

**Control Logic**:
- Target fuel is adjusted based on RPM error
- Actual pump output ramps toward target at 1 µs/step
- Low-RPM ramps are slower (smoother control)
- Cut step is faster for emergencies (5 µs vs 1 µs)

### **4. Button/User Input System**
**File Location**: Lines 329-334

**Button Logic**:
```
WAITING Mode:
  Short press → Print status
  Hold 2s     → ARM (stage2Armed = true for 10s)
  
ARMED State:
  Hold 3s     → START sequence

STARTING/IDLING/OPERATING:
  Any press   → SOFT STOP (graceful shutdown)

ABORTED State:
  Hold 2s     → Clear error (if EGT safe)
```

**Debounce**: 35ms (BTN_DEBOUNCE_MS)

### **5. Output Control** (Lines 423-434)

**applyOutputs()**:
- Constrains ESC values to 1000-2000 µs
- Writes PWM to pump & starter ESCs
- Sets digital outputs (ignition, valves)

**Safety**:
- All outputs initialized to safe state (1000 µs for ESC, OFF for digital)
- `forceSafeOutputs()` cuts all power immediately

---

## 🚀 Start Sequence Details

### **Complete Start Flow** (Lines ~800+)

```
1. PURGE (3s default)
   └─ Starter runs at purgeUs (1100 µs)
   └─ No fuel, no ignition

2. SPINUP_PREHEAT
   └─ Increase starter to spinUs (1150 µs)
   └─ Wait for preheatMs (2.5s)
   └─ Monitor RPM rise

3. INTRO_FUEL
   └─ Ignite glow plug
   └─ Introduce fuel at introFuelUs (~50 ml/min)
   └─ Wait for flame confirmation (RPM > fuelConfirmRpm)

4. POST_IGNITION_HEAT
   └─ Maintain fuel flow
   └─ Monitor temperature rise
   └─ Increase starter to assistUs (1200 µs)

5. ACCEL_TO_IDLE
   └─ Gradually increase fuel
   └─ Target idleRpm (42,000 RPM default)
   └─ Starter releases at starterReleaseRpm
   └─ Timeout after accelToIdleTimeoutMs (20s)
   └─ Move to IDLING when idle RPM stable
```

### **Dry-Start Mode** (EGT Sensor Open)
When thermocouple is disconnected:
```
1. Detect EGT fault (OPEN)
2. Check allowDryStartWhenEgtFault flag
3. Convert to DRY START:
   └─ Starter runs normally
   └─ PUMP stays OFF
   └─ VALVES stay OFF
   └─ IGNITION stays OFF
   └─ Only tests starter & RPM sensor
4. Cannot transition to fuel-on stage
   └─ Real fuel start still requires valid EGT
```

---

## 🌐 Web UI System

**File Location**: Lines 265-273, 200-201

**Configuration**:
- SSID: `ECU_TestV1`
- Password: `admin1234`
- IP: `192.168.4.1`
- Port: 80

**Features**:
- Dashboard with real-time status
- Control buttons (ARM, START, STOP)
- Parameter adjustment
- Test Wizard (runs checklist tests)
- Event log viewer
- Configuration save/restore

**Key Functions**:
```cpp
setupWebServer()         // Define all routes
startWebServer()         // Start SoftAP & listen
stopWebServer()          // Shutdown
htmlPage()               // Generate HTML dashboard
webStatusJson()          // JSON status data
```

---

## 💾 SD Card Logging

**File Location**: Lines 275-280, 204-207

**CSV Logging Format**:
```
Time_ms, Mode, RPM, EGT_C, Pump_us, Start_us, Ign, V1, V2
1000,    PURGE, 0,  25,   1100,    1100,    0,  0,  0
1500,    PURGE, 150, 26,  1100,    1100,    0,  0,  0
...
```

**Log Rate**: 2 lines/sec (500ms) during active operation

**File Naming**: `/ECU000.CSV`, `/ECU001.CSV`, etc. (auto-increment)

**Functions**:
```cpp
initSdLogging()          // Mount SD, create log file
updateSdTelemetry()      // Write data periodically
sdLogEvent(msg)          // Log text event
```

---

## ✅ Test Wizard/Checklist System

**File Location**: Lines 287-326

**9 Tests**:
1. **EGT** - Verify sensor reads temperature
2. **RPM_NOISE** - Check signal quality
3. **IGN_PULSE** - Ignition output works
4. **STARTER** - Starter spins
5. **STARTER_IGN_EMI** - No EMI interference
6. **VALVE_1** - Gas valve 1 operates
7. **VALVE_2** - Gas valve 2 operates
8. **PUMP_PRIME** - Fuel pump primes
9. **KILL_SWITCH** - Emergency stop works

**Results**:
```cpp
enum TestResult {
  TEST_NOT_RUN,    // Never run
  TEST_RUNNING,    // Currently executing
  TEST_PASS,       // Successful
  TEST_FAIL        // Failed
};
```

**Integration**:
- Must pass checklist before normal start (configurable)
- Can be run from Web UI
- Results stored in event log
- Helps diagnose hardware issues

---

## 🔌 Status LED Patterns

**GPIO 2** (active-low):

| Pattern | Meaning |
|---------|---------|
| Very fast blink | Component test running |
| Fast blink | Abort/error detected |
| Quick blink (2 Hz) | Armed, waiting for start |
| Solid ON | Starting/operating |
| Medium blink | Idle stable |
| Faster blink | Cooldown in progress |
| Slow heartbeat | Waiting for input |

---

## 🎮 Serial Commands

Type commands in Serial Monitor:

```cpp
rpmdetail              // Toggle detailed RPM output
rpmdetail on|off       // Enable/disable
set rpmfilter <us>     // Change pulse width filter (e.g., 150)
set rpmedge rising|falling  // Select edge trigger
rpmreset               // Reset RPM ISR counters
?                      // Print checklist
test <name>            // Run specific test
set <param> <value>    // Change config parameter
```

---

## 📈 Real-Time Calculations

### **EGT Gradient** (Temperature rise rate)
```cpp
gradientCps = (current_temp - prev_temp) / time_elapsed_seconds
```
- Monitored every 120ms
- Lookahead 3 seconds: if gradient continues, will temp exceed max?
- Fuel is cut if overshoot predicted

### **RPM Jitter** (Pulse timing variation)
```cpp
jitterPct = (maxInterval - minInterval) / avgInterval * 100%
```
- Clean: <15%
- Warn: 15-30%
- Noisy: >30%

### **RPM Error** (Difference from target)
```cpp
error = targetRpm - currentRpm
fuel_adjustment = error * K_proportional
```

---

## 🔒 Safety Features

### **1. Multi-Level Abort**
- EGT fault → Abort if `abortOnEgtFault`
- RPM fault → Abort if `abortOnRpmFault`
- Temperature limit → Abort if temp > maxEgtC
- RPM limit → Cut fuel if RPM > maxRpm
- Timeout → Abort if stage takes too long

### **2. Gradient Monitoring**
- Real-time temperature rise rate checked
- Lookahead 3 seconds
- Cuts fuel if overshoot predicted

### **3. RPM Confidence**
- Pulse noise classified in real-time
- Noisy signals block start
- Rest guard prevents false starts

### **4. Safe Defaults**
- All outputs OFF on boot
- ESC at safe (1000 µs) position
- No auto-start unless explicitly enabled

### **5. Cooldown Mode**
- Post-stop cooldown with starter running
- Fuel/valves/ignition stay OFF
- Allows engine to cool naturally
- Configurable duration (5-45 seconds)

---

## 📊 Performance Metrics

| Metric | Value | Notes |
|--------|-------|-------|
| Main Loop Rate | ~240 Hz | Based on status print interval |
| EGT Sample Rate | ~8 Hz | 120ms polling |
| RPM Sample Rate | 10 Hz | 100ms window |
| SD Log Rate | 2 Hz | 500ms interval |
| Button Debounce | 35 ms | Stable press detection |
| Web Update Rate | ~1-2 Hz | Browser refresh rate |

---

## 🔄 Function Call Hierarchy

### **Main Control Loop**
```cpp
loop()
├─ Read EGT (every 120ms)
├─ Sample RPM (every 100ms)
├─ updateButtonState()
├─ updateFuel() [mode-specific]
├─ updateStarting() [if MODE_STARTING]
├─ updateIdling()
├─ updateOperating()
├─ updateCooldown()
├─ applyOutputs()
├─ updateStatusLed()
├─ printStatus() [every 250ms]
├─ handleSerial()
├─ server.handleClient() [if web enabled]
└─ updateSdTelemetry() [every 500ms]
```

---

## 🎓 Learning Path

**Suggested order to understand the code**:

1. **Start here**: Enums & structs (lines 169-261)
   - Understand modes, stages, data structures

2. **Pin layout**: Globals & initialization (lines 211-280)
   - See what's connected where

3. **Fuel control**: Pump calibration (lines 403-421)
   - How fuel flow maps to microseconds

4. **RPM**: Noise classification (lines 375-386)
   - Signal quality detection

5. **Main loop**: The `loop()` function (~line 1800+)
   - How everything ties together

6. **State machines**: Mode/stage transitions
   - Flow between states

7. **Web UI**: HTML generation
   - How dashboard works

8. **Serial commands**: Command handler
   - Debugging interface

---

## 🐛 Debugging Tips

### **Check RPM Signal Quality**
```
Serial: rpmdetail
Look for: rejectPct <5%, jitterPct <15%
If high: Check magnet position, hall sensor wiring, EMI
```

### **Monitor EGT Reading**
```
Expected range: 25-700°C
If reading NAN: Check thermocouple connection, SPI wiring
If fault code: See egtFaultString() for interpretation
```

### **Verify Fuel Flow**
```
Use pump test from checklist
Monitor SD log for pump_us values
Compare against kPumpMap table
```

### **Test Starter Torque**
```
Use starter test from checklist
Increase starterPurgeUs/spinUs/assistUs if weak
```

---

## 📚 Key Files & Lines

| Section | Lines |
|---------|-------|
| Configuration struct | 102-167 |
| State enums | 169-170 |
| EGT handling | 217-222, 394-401 |
| RPM diagnostics | 224-261, 375-391 |
| Fuel calibration | 96-100, 403-421 |
| Button logic | 329-334 |
| Output control | 423-434 |
| Mode/stage functions | 435-449 |
| Web UI | 265-273 |
| SD logging | 275-280 |
| Test checklist | 287-326 |

---

## 🚀 Next Steps

Now that you understand the architecture:

1. **Review the main loop** - See how all parts work together
2. **Trace a start sequence** - Follow the state transitions
3. **Examine error handling** - How aborts work
4. **Study specific subsystems**:
   - RPM ISR (lines ~1500+)
   - Web server routes (lines ~1600+)
   - Fuel controller logic (lines ~1300+)

---

**Document Version**: 1.0  
**Last Updated**: 2026-07-16  
**Firmware Version**: TestV1_EGT_DRY_START_PATCH
