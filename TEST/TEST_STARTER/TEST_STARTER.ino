/*
  ============================================================================
  TEST_STARTER - RPM vs STARTER PWM Noise/Stability Bench Test
  ============================================================================
  Mục đích:
    - Test riêng cảm biến RPM và starter, KHÔNG cần ARM, KHÔNG có bảo vệ.
    - Khi khởi động: starter KHÔNG quay (PWM = 1000us safe).
    - Tăng/giảm PWM starter bằng phím '+' và '-' trên Serial Monitor.
    - ESP32 liên tục đo RPM theo từng mức PWM và đánh giá:
        * Nhiễu (noise)      : dựa trên tỉ lệ xung bị lọc + jitter chu kỳ
        * Ổn định (stability): dựa trên độ dao động RPM (CV%) khi PWM giữ nguyên
    - In kết luận: CLEAN/WARN/NOISY  +  STABLE/UNSTABLE/SETTLING/NO_SIGNAL

  Phần cứng (giống ECU chính, chỉ dùng 2 chân):
    - Starter ESC signal : GPIO 25
    - RPM sensor (Hall)  : GPIO 33
    - GND chung ESP32 <-> ESC <-> cảm biến.
    - Nguồn ESC/BEC tách riêng với nguồn ESP32.

  ⚠️ AN TOÀN BENCH TEST:
    - THÁO cánh/impeller khỏi starter trước khi test (motor có thể quay nhanh).
    - Cố định starter chắc chắn.
    - Đây là firmware TEST: không có cooldown, không có kiểm tra EGT, không ARM.
      KHÔNG dùng cho chạy động cơ thật có nhiên liệu.

  Serial Monitor: 115200 baud, line ending = Newline (hoặc No line ending
                  cũng được vì '+'/'-' xử lý ngay từng ký tự).

  Web UI (tùy chọn, không cần Serial Monitor): SoftAP SSID "TEST_STARTER",
  password "test1234", http://192.168.4.1 — cùng bộ lệnh, điều khiển bằng
  nút bấm/ô nhập trên trình duyệt điện thoại/laptop.

  LỆNH:
    +           Tăng PWM starter 1 bước (mặc định 10us)
    -           Giảm PWM starter 1 bước
    0  hoặc  s  Dừng starter (PWM về 1000us)
    step <us>   Đổi bước tăng/giảm (1..200us)
    pwm <us>    Đặt PWM trực tiếp (1000..2000us)
    ppr <n>     Số xung / vòng (1=nam châm, 2=quang học...) mặc định 1
    filter <us> Bộ lọc glitch RPM (20..2000us) mặc định 120us
    edge rising|falling   Cạnh kích RPM, mặc định rising
    reset       Xóa bộ đếm & lịch sử RPM
    status      In trạng thái ngay
    help        In lại danh sách lệnh
  ============================================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif
#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// ---------------- Web UI (optional, alongside Serial) ----------------
// SECURITY: anyone on this SoftAP can drive the starter via /cmd. Change WEB_PASS
// to a strong private value; the default is a weak bench placeholder.
static const char* WEB_SSID = "TEST_STARTER";
static const char* WEB_PASS = "test1234";   // <-- CHANGE ME (min 8 chars)
WebServer server(80);

// ESC PWM is driven directly through the ESP32 LEDC peripheral instead of the
// ESP32Servo library. ESP32Servo's internal timer/LEDC setup was found to
// produce an unstable pulse on some ESP32 Arduino core versions (core 3.x
// changed the LEDC API), which the ESC reads as noisy/invalid throttle and
// responds to by stuttering/cutting the motor - independent of supply
// current. See CLAUDE.md at the repo root for the full writeup.

// ---------------- Pins ----------------
#define PIN_ESC_START   25
#define PIN_RPM         33

// ---------------- ESC range ----------------
static const int ESC_SAFE_US = 1000;   // starter KHÔNG quay
static const int ESC_MIN_US  = 1000;
static const int ESC_MAX_US  = 2000;

// ---------------- ESC PWM via raw LEDC ----------------
static const int ESC_PWM_FREQ_HZ = 50;
static const int ESC_PWM_RES_BITS = 16;
static const int LEDC_CH_START = 1;

void escAttach(uint8_t pin, int legacyChannel) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)legacyChannel;
  ledcAttach(pin, ESC_PWM_FREQ_HZ, ESC_PWM_RES_BITS);
#else
  ledcSetup(legacyChannel, ESC_PWM_FREQ_HZ, ESC_PWM_RES_BITS);
  ledcAttachPin(pin, legacyChannel);
#endif
}

void escWriteUs(uint8_t pin, int legacyChannel, int us) {
  const uint32_t maxDuty = (1UL << ESC_PWM_RES_BITS) - 1;
  uint32_t duty = (uint64_t)us * maxDuty / 20000UL; // 20ms period at 50Hz
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);
#else
  (void)pin;
  ledcWrite(legacyChannel, duty);
#endif
}

// ---------------- Timing ----------------
static const uint32_t RPM_SAMPLE_MS    = 100;   // cửa sổ tính RPM
static const uint32_t STATUS_PRINT_MS  = 500;   // in trạng thái 2 lần/giây
static const uint32_t SIGNAL_TIMEOUT_MS = 1000; // quá lâu không có xung => mất tín hiệu
static const uint32_t SETTLE_MS        = 1500;  // thời gian chờ ổn định sau khi đổi PWM

// ---------------- User adjustable ----------------
int      pwmUs        = ESC_SAFE_US;   // PWM starter hiện tại (bắt đầu = không quay)
int      pwmStepUs    = 10;            // bước +/-
int      pulsesPerRev = 1;             // xung / vòng

volatile uint32_t rpmMinPulseUs = 120; // bộ lọc glitch phần cứng
int      rpmEdgeMode  = RISING;

// ---------------- RPM ISR state ----------------
volatile uint32_t isrLastRawEdgeUs      = 0;
volatile uint32_t isrLastAcceptedUs     = 0;
volatile uint32_t isrLastPeriodUs       = 0;
volatile uint32_t isrRawEdges           = 0;
volatile uint32_t isrAcceptedPulses     = 0;
volatile uint32_t isrRejectedEdges      = 0;
volatile uint32_t isrAcceptedIntervals  = 0;
volatile uint32_t isrMinDtUs            = 0xFFFFFFFFUL;
volatile uint32_t isrMaxDtUs            = 0;
volatile uint64_t isrSumDtUs            = 0;

// ---------------- Computed RPM window ----------------
struct RpmWin {
  float    rpm         = 0.0f;   // RPM ưu tiên theo chu kỳ tức thời
  float    rpmWindow   = 0.0f;   // RPM trung bình cửa sổ (chéo kiểm)
  float    avgIntervalUs = 0.0f;
  float    jitterPct   = 0.0f;   // (maxDt-minDt)/avg * 100
  float    rejectPct   = 0.0f;   // xung bị lọc / xung thô * 100
  uint32_t raw = 0, accepted = 0, rejected = 0, intervals = 0;
  bool     signalRecent = false;
  uint32_t lastComputedMs = 0;
} rpm;

// ---------------- Stability tracking ----------------
static const uint8_t RPM_HIST = 16;
float    rpmHist[RPM_HIST];
uint8_t  rpmHistCount = 0;
uint8_t  rpmHistHead  = 0;
uint32_t pwmChangedAtMs = 0;   // để tính "settling" sau khi đổi PWM

// ---------------- Serial input ----------------
String   cmdBuf = "";

// ============================================================================
//  RPM measurement
// ============================================================================
void IRAM_ATTR rpmISR() {
  uint32_t nowUs = micros();
  if (nowUs == 0) nowUs = 1;   // 0 is the "no edge yet" sentinel; avoid it at the ~71min micros() wrap
  isrRawEdges++;

  if (isrLastRawEdgeUs == 0) {
    // Cạnh đầu tiên sau reset: chỉ ghi mốc thời gian, chưa có khoảng để chấp nhận.
    isrLastRawEdgeUs = nowUs;
    return;
  }

  uint32_t dtRawUs = nowUs - isrLastRawEdgeUs;
  isrLastRawEdgeUs = nowUs;

  // Bộ lọc glitch: xung quá hẹp = nhiễu.
  if (dtRawUs < rpmMinPulseUs) {
    isrRejectedEdges++;
    return;
  }

  if (isrLastAcceptedUs != 0) {
    uint32_t dtAcc = nowUs - isrLastAcceptedUs;
    isrLastPeriodUs = dtAcc;
    isrAcceptedIntervals++;
    isrSumDtUs += dtAcc;
    if (dtAcc < isrMinDtUs) isrMinDtUs = dtAcc;
    if (dtAcc > isrMaxDtUs) isrMaxDtUs = dtAcc;
  }
  isrLastAcceptedUs = nowUs;
  isrAcceptedPulses++;
}

void resetRpmStats() {
  noInterrupts();
  isrLastRawEdgeUs = 0;
  isrLastAcceptedUs = 0;
  isrLastPeriodUs = 0;
  isrRawEdges = 0;
  isrAcceptedPulses = 0;
  isrRejectedEdges = 0;
  isrAcceptedIntervals = 0;
  isrSumDtUs = 0;
  isrMinDtUs = 0xFFFFFFFFUL;
  isrMaxDtUs = 0;
  interrupts();
  rpm = RpmWin();
  rpmHistCount = 0;
  rpmHistHead = 0;
}

void attachRpm() {
  pinMode(PIN_RPM, INPUT);       // dùng pull-up ngoài của mạch cảm biến
  attachInterrupt(digitalPinToInterrupt(PIN_RPM), rpmISR, rpmEdgeMode);
}

void reattachRpm() {
  detachInterrupt(digitalPinToInterrupt(PIN_RPM));
  attachRpm();
}

void pushRpmHist(float v) {
  rpmHist[rpmHistHead] = v;
  rpmHistHead = (rpmHistHead + 1) % RPM_HIST;
  if (rpmHistCount < RPM_HIST) rpmHistCount++;
}

// Hệ số biến thiên (%) của RPM gần đây = stddev/mean*100. Thấp = ổn định.
float rpmCvPct() {
  if (rpmHistCount < 4) return -1.0f;   // chưa đủ mẫu
  float sum = 0;
  for (uint8_t i = 0; i < rpmHistCount; i++) sum += rpmHist[i];
  float mean = sum / rpmHistCount;
  if (mean < 1.0f) return -1.0f;        // gần như đứng yên, bỏ qua
  float var = 0;
  for (uint8_t i = 0; i < rpmHistCount; i++) {
    float d = rpmHist[i] - mean;
    var += d * d;
  }
  var /= rpmHistCount;
  return sqrtf(var) / mean * 100.0f;
}

void updateRpm() {
  uint32_t nowMs = millis();
  if (nowMs - rpm.lastComputedMs < RPM_SAMPLE_MS) return;
  uint32_t windowUs = (uint32_t)(nowMs - rpm.lastComputedMs) * 1000UL;
  rpm.lastComputedMs = nowMs;

  uint32_t raw, accepted, rejected, intervals, sumDt, minDt, maxDt, lastAcc, lastPeriod;
  noInterrupts();
  raw        = isrRawEdges;
  accepted   = isrAcceptedPulses;
  rejected   = isrRejectedEdges;
  intervals  = isrAcceptedIntervals;
  sumDt      = (uint32_t)isrSumDtUs;
  minDt      = isrMinDtUs;
  maxDt      = isrMaxDtUs;
  lastAcc    = isrLastAcceptedUs;
  lastPeriod = isrLastPeriodUs;
  // reset các bộ đếm cửa sổ (giữ lastRawEdge/lastAccepted để tính chu kỳ liên tục)
  isrRawEdges = 0;
  isrAcceptedPulses = 0;
  isrRejectedEdges = 0;
  isrAcceptedIntervals = 0;
  isrSumDtUs = 0;
  isrMinDtUs = 0xFFFFFFFFUL;
  isrMaxDtUs = 0;
  interrupts();

  rpm.raw = raw; rpm.accepted = accepted; rpm.rejected = rejected; rpm.intervals = intervals;

  uint32_t nowUs = micros();
  rpm.signalRecent = (lastAcc != 0) && ((uint32_t)(nowUs - lastAcc) <= SIGNAL_TIMEOUT_MS * 1000UL);

  rpm.rejectPct = (raw > 0) ? ((float)rejected * 100.0f / (float)raw) : 0.0f;

  // RPM theo cửa sổ (trung bình)
  rpm.rpmWindow = (accepted > 0 && windowUs > 0)
                  ? ((float)accepted * 60000000.0f / ((float)windowUs * (float)pulsesPerRev))
                  : 0.0f;

  // RPM theo chu kỳ tức thời (phản ứng nhanh)
  float rpmPeriod = 0.0f;
  if (rpm.signalRecent && lastPeriod > 0)
    rpmPeriod = 60000000.0f / ((float)lastPeriod * (float)pulsesPerRev);

  // Jitter chu kỳ trong cửa sổ
  if (intervals > 0) {
    rpm.avgIntervalUs = (float)sumDt / (float)intervals;
    rpm.jitterPct = (minDt != 0xFFFFFFFFUL && rpm.avgIntervalUs > 0.0f)
                    ? ((float)(maxDt - minDt) * 100.0f / rpm.avgIntervalUs)
                    : 0.0f;
  } else {
    rpm.avgIntervalUs = 0.0f;
    rpm.jitterPct = 0.0f;
  }

  rpm.rpm = (rpm.signalRecent && rpmPeriod > 0.0f) ? rpmPeriod : rpm.rpmWindow;

  // Lưu lịch sử để đánh giá ổn định (chỉ khi có tín hiệu)
  if (rpm.signalRecent && rpm.rpm > 0.0f) pushRpmHist(rpm.rpm);
}

// ============================================================================
//  Đánh giá nhiễu & ổn định
// ============================================================================
const char* noiseVerdict() {
  if (!rpm.signalRecent && rpm.raw == 0) return "NO_SIGNAL";
  if (rpm.raw > 0 && rpm.accepted == 0)  return "NOISY";       // toàn xung rác bị lọc
  if (rpm.rejectPct > 20.0f || rpm.jitterPct > 40.0f) return "NOISY";
  if (rpm.rejectPct > 5.0f  || rpm.jitterPct > 15.0f) return "WARN";
  return "CLEAN";
}

const char* stabilityVerdict() {
  if (pwmUs <= ESC_SAFE_US) return "OFF";
  if (millis() - pwmChangedAtMs < SETTLE_MS) return "SETTLING";
  if (!rpm.signalRecent || rpm.rpm <= 0.0f) return "NO_SIGNAL";
  float cv = rpmCvPct();
  if (cv < 0.0f)  return "SETTLING";   // chưa đủ mẫu
  if (cv > 5.0f)  return "UNSTABLE";
  if (cv > 2.0f)  return "WARN";
  return "STABLE";
}

// ============================================================================
//  Output control
// ============================================================================
// Writes the current pwmUs to the starter ESC. Called on every change and
// once per loop() to keep the ESC signal refreshed.
void applyPwm() {
  escWriteUs(PIN_ESC_START, LEDC_CH_START, pwmUs);
}

void setPwm(int us, bool markChange) {
  int old = pwmUs;
  int newUs = constrain(us, ESC_MIN_US, ESC_MAX_US);
  pwmUs = newUs;
  applyPwm();
  if (markChange && pwmUs != old) {
    pwmChangedAtMs = millis();
    rpmHistCount = 0;   // reset lịch sử ổn định vì điểm làm việc đã đổi
    rpmHistHead = 0;
  }
}

// ============================================================================
//  Status print
// ============================================================================
void printStatus() {
  float cv = rpmCvPct();
  Serial.print("PWM=");    Serial.print(pwmUs);      Serial.print("us");
  if (pwmUs <= ESC_SAFE_US) Serial.print("(OFF)");
  Serial.print(" | RPM="); Serial.print(rpm.rpm, 0);
  Serial.print(" (win ");  Serial.print(rpm.rpmWindow, 0); Serial.print(")");
  Serial.print(" | raw="); Serial.print(rpm.raw);
  Serial.print(" acc=");   Serial.print(rpm.accepted);
  Serial.print(" rej=");   Serial.print(rpm.rejected);
  Serial.print(" (");      Serial.print(rpm.rejectPct, 1); Serial.print("%)");
  Serial.print(" | jit="); Serial.print(rpm.jitterPct, 1); Serial.print("%");
  Serial.print(" cv=");    if (cv < 0) Serial.print("--"); else { Serial.print(cv, 1); Serial.print("%"); }
  Serial.print(" | NOISE="); Serial.print(noiseVerdict());
  Serial.print(" STAB=");    Serial.print(stabilityVerdict());
  Serial.println();
}

void printHelp() {
  Serial.println();
  Serial.println("==== TEST_STARTER: RPM vs PWM noise/stability ====");
  Serial.println("  +           tang PWM starter 1 buoc");
  Serial.println("  -           giam PWM starter 1 buoc");
  Serial.println("  0 / s       dung starter (PWM=1000us)");
  Serial.println("  step <us>   doi buoc +/- (1..200)");
  Serial.println("  pwm <us>    dat PWM truc tiep (1000..2000)");
  Serial.println("  ppr <n>     xung/vong (mac dinh 1)");
  Serial.println("  filter <us> bo loc glitch RPM (20..2000)");
  Serial.println("  edge rising|falling   canh kich RPM");
  Serial.println("  reset       xoa bo dem RPM");
  Serial.println("  status      in trang thai ngay");
  Serial.println("  help        in menu nay");
  Serial.print  ("  hien tai: step="); Serial.print(pwmStepUs);
  Serial.print  ("us ppr=");           Serial.print(pulsesPerRev);
  Serial.print  (" filter=");          Serial.print((uint32_t)rpmMinPulseUs);
  Serial.print  ("us edge=");          Serial.println(rpmEdgeMode == FALLING ? "FALLING" : "RISING");
  Serial.println("==================================================");
  Serial.println();
}

// ============================================================================
//  Command parsing
// ============================================================================
long numberAfter(const String& s, const String& key) {
  int i = s.indexOf(key);
  if (i < 0) return -1;
  return s.substring(i + key.length()).toInt();
}

void handleWordCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (!cmd.length()) return;

  if (cmd == "help") { printHelp(); return; }
  if (cmd == "status") { printStatus(); return; }
  if (cmd == "reset") { resetRpmStats(); Serial.println("RPM stats reset."); return; }
  if (cmd == "0" || cmd == "s" || cmd == "stop") { setPwm(ESC_SAFE_US, true); Serial.println("STARTER OFF (PWM=1000us)."); return; }

  if (cmd.startsWith("step")) {
    long v = numberAfter(cmd, "step");
    if (v >= 1 && v <= 200) { pwmStepUs = (int)v; Serial.print("step="); Serial.print(pwmStepUs); Serial.println("us"); }
    else Serial.println("ERROR: step 1..200");
    return;
  }
  if (cmd.startsWith("pwm")) {
    long v = numberAfter(cmd, "pwm");
    if (v >= ESC_MIN_US && v <= ESC_MAX_US) { setPwm((int)v, true); Serial.print("PWM="); Serial.print(pwmUs); Serial.println("us"); }
    else Serial.println("ERROR: pwm 1000..2000");
    return;
  }
  if (cmd.startsWith("ppr")) {
    long v = numberAfter(cmd, "ppr");
    if (v >= 1 && v <= 12) { pulsesPerRev = (int)v; resetRpmStats(); Serial.print("ppr="); Serial.println(pulsesPerRev); }
    else Serial.println("ERROR: ppr 1..12");
    return;
  }
  if (cmd.startsWith("filter")) {
    long v = numberAfter(cmd, "filter");
    if (v >= 20 && v <= 2000) { noInterrupts(); rpmMinPulseUs = (uint32_t)v; interrupts(); resetRpmStats(); Serial.print("filter="); Serial.print((uint32_t)rpmMinPulseUs); Serial.println("us"); }
    else Serial.println("ERROR: filter 20..2000");
    return;
  }
  if (cmd == "edge rising")  { rpmEdgeMode = RISING;  reattachRpm(); resetRpmStats(); Serial.println("edge=RISING");  return; }
  if (cmd == "edge falling") { rpmEdgeMode = FALLING; reattachRpm(); resetRpmStats(); Serial.println("edge=FALLING"); return; }

  Serial.print("Unknown cmd: "); Serial.println(cmd);
}

// Entry point shared by the Web UI /cmd endpoint (and reusable by Serial):
// handles the '+'/'-' shortcuts too, then falls through to handleWordCommand()
// for everything else. Serial typing still uses its own fast per-character
// path in handleSerial() below so held-down keys keep responding instantly.
void handleCommand(String cmd) {
  cmd.trim();
  if (cmd == "+") { setPwm(pwmUs + pwmStepUs, true); Serial.print("PWM="); Serial.print(pwmUs); Serial.println("us (+)"); return; }
  if (cmd == "-") { setPwm(pwmUs - pwmStepUs, true); Serial.print("PWM="); Serial.print(pwmUs); Serial.println("us (-)"); return; }
  handleWordCommand(cmd);
}

// Đọc Serial không blocking. '+' và '-' xử lý ngay từng ký tự (bấm liên tục được).
void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '+') { setPwm(pwmUs + pwmStepUs, true); Serial.print("PWM="); Serial.print(pwmUs); Serial.println("us (+)"); continue; }
    if (c == '-') { setPwm(pwmUs - pwmStepUs, true); Serial.print("PWM="); Serial.print(pwmUs); Serial.println("us (-)"); continue; }

    if (c == '\n') {
      if (cmdBuf.length()) { handleWordCommand(cmdBuf); cmdBuf = ""; }
    } else if (c != '\r') {
      if (cmdBuf.length() < 60) cmdBuf += c;
    }
  }
}

// ============================================================================
//  Web UI (optional alternative to Serial Monitor)
// ============================================================================
String webStatusJson() {
  float cv = rpmCvPct();
  String s;
  s.reserve(384);
  s = "{";
  s += "\"pwm\":\"" + String(pwmUs) + (pwmUs <= ESC_SAFE_US ? " (OFF)" : "") + "\",";
  s += "\"rpm\":\"" + String(rpm.rpm, 0) + "\",";
  s += "\"rpmWindow\":\"" + String(rpm.rpmWindow, 0) + "\",";
  s += "\"raw\":\"" + String(rpm.raw) + "\",";
  s += "\"accepted\":\"" + String(rpm.accepted) + "\",";
  s += "\"rejected\":\"" + String(rpm.rejected) + "\",";
  s += "\"rejectPct\":\"" + String(rpm.rejectPct, 1) + "\",";
  s += "\"jitterPct\":\"" + String(rpm.jitterPct, 1) + "\",";
  s += "\"cv\":\"" + String(cv < 0 ? String("--") : String(cv, 1)) + "\",";
  s += "\"noise\":\"" + String(noiseVerdict()) + "\",";
  s += "\"stab\":\"" + String(stabilityVerdict()) + "\",";
  s += "\"step\":\"" + String(pwmStepUs) + "\",";
  s += "\"ppr\":\"" + String(pulsesPerRev) + "\",";
  s += "\"filter\":\"" + String((uint32_t)rpmMinPulseUs) + "\",";
  s += "\"edge\":\"" + String(rpmEdgeMode == FALLING ? "FALLING" : "RISING") + "\"";
  s += "}";
  return s;
}

String htmlPage() {
  return String(R"HTML(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>TEST_STARTER</title>
<style>
body{margin:0;background:#0b1020;color:#e9eefc;font-family:Arial,Helvetica,sans-serif}.wrap{max-width:900px;margin:auto;padding:16px}
h1{margin:8px 0 4px;font-size:22px}.sub{color:#9fb0d0;margin-bottom:14px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px}
.card{background:#151d33;border:1px solid #283451;border-radius:14px;padding:12px}.label{color:#9fb0d0;font-size:12px;text-transform:uppercase}.val{font-size:20px;font-weight:700;margin-top:4px}
.btns{display:flex;flex-wrap:wrap;gap:8px;margin:12px 0}.btn{border:0;border-radius:10px;padding:10px 12px;background:#293855;color:#fff;font-weight:700}.danger{background:#8a2430}.go{background:#1e7042}
input{background:#0b1020;color:#fff;border:1px solid #405071;border-radius:8px;padding:8px;width:80px}.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin:10px 0}
</style></head><body><div class="wrap">
<h1>TEST_STARTER bench</h1><div class="sub">SoftAP TEST_STARTER / test1234 — http://192.168.4.1 — THAO canh/impeller truoc khi test</div>
<div class="grid" id="cards"></div>
<h2>Starter</h2><div class="btns">
<button class="btn go" onclick="cmd('+')">+ step</button><button class="btn" onclick="cmd('-')">- step</button><button class="btn danger" onclick="cmd('0')">STOP</button>
</div>
<div class="row">PWM us <input id="pwm" value="1200"><button class="btn" onclick="cmd('pwm '+v('pwm'))">Set PWM</button></div>
<div class="row">Step us <input id="step" value="10"><button class="btn" onclick="cmd('step '+v('step'))">Set step</button></div>
<h2>RPM sensor</h2><div class="row">
Filter us <input id="filt" value="120"><button class="btn" onclick="cmd('filter '+v('filt'))">Set</button>
PPR <input id="ppr" value="1"><button class="btn" onclick="cmd('ppr '+v('ppr'))">Set</button>
<button class="btn" onclick="cmd('edge rising')">Edge Rising</button><button class="btn" onclick="cmd('edge falling')">Edge Falling</button>
<button class="btn" onclick="cmd('reset')">Reset RPM stats</button>
</div>
</div><script>
function v(id){return document.getElementById(id).value}
function cmd(c){fetch('/cmd?c='+encodeURIComponent(c)).then(()=>setTimeout(load,200))}
function load(){fetch('/api').then(r=>r.json()).then(d=>{let cards=[['PWM',d.pwm],['RPM',d.rpm],['RPM win',d.rpmWindow],['raw',d.raw],['acc',d.accepted],['rej',d.rejected],['rej%',d.rejectPct],['jit%',d.jitterPct],['cv%',d.cv],['NOISE',d.noise],['STAB',d.stab],['step',d.step],['ppr',d.ppr],['filter',d.filter],['edge',d.edge]];
 document.getElementById('cards').innerHTML=cards.map(x=>'<div class="card"><div class="label">'+x[0]+'</div><div class="val">'+x[1]+'</div></div>').join('');});}
setInterval(load,700);load();
</script></body></html>
)HTML");
}

void setupWebServer() {
  server.on("/", []() { server.send(200, "text/html", htmlPage()); });
  server.on("/api", []() { server.send(200, "application/json", webStatusJson()); });
  server.on("/cmd", []() {
    String c = server.hasArg("c") ? server.arg("c") : "";
    if (c.length()) handleCommand(c);
    server.send(200, "text/plain", "OK");
  });
  server.begin();
}

// ============================================================================
//  Setup / Loop
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  // ESC starter: arm ở 1000us (KHÔNG quay) ngay từ đầu.
  escAttach(PIN_ESC_START, LEDC_CH_START);
  pwmUs = ESC_SAFE_US;
  applyPwm();

  attachRpm();
  resetRpmStats();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WEB_SSID, WEB_PASS);
  setupWebServer();

  Serial.println();
  Serial.println("TEST_STARTER booted. Starter DISARMED at 1000us (khong quay).");
  Serial.println("Dung '+' / '-' de tang/giam PWM. Go 'help' de xem menu.");
  Serial.print("Web UI: SSID="); Serial.print(WEB_SSID); Serial.println(" PASS=test1234 URL=http://192.168.4.1");
  printHelp();

  delay(2000);   // giữ 1000us đủ lâu để ESC arm an toàn
  Serial.println("ESC armed. San sang test.");
}

uint32_t lastStatusMs = 0;

void loop() {
  handleSerial();
  server.handleClient();
  updateRpm();
  applyPwm();   // keep the starter ESC signal refreshed each loop

  uint32_t nowMs = millis();
  if (nowMs - lastStatusMs >= STATUS_PRINT_MS) {
    lastStatusMs = nowMs;
    printStatus();
  }
}
