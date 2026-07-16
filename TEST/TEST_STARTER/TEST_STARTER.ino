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
#include <ESP32Servo.h>

// ---------------- Pins ----------------
#define PIN_ESC_START   25
#define PIN_RPM         33

// ---------------- ESC range ----------------
static const int ESC_SAFE_US = 1000;   // starter KHÔNG quay
static const int ESC_MIN_US  = 1000;
static const int ESC_MAX_US  = 2000;

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

Servo escStart;

// ============================================================================
//  RPM measurement
// ============================================================================
void IRAM_ATTR rpmISR() {
  uint32_t nowUs = micros();
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
void applyPwm() {
  pwmUs = constrain(pwmUs, ESC_MIN_US, ESC_MAX_US);
  escStart.writeMicroseconds(pwmUs);
}

void setPwm(int us, bool markChange) {
  int old = pwmUs;
  pwmUs = constrain(us, ESC_MIN_US, ESC_MAX_US);
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
//  Setup / Loop
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  // ESC starter: arm ở 1000us (KHÔNG quay) ngay từ đầu.
  escStart.setPeriodHertz(50);
  escStart.attach(PIN_ESC_START, ESC_MIN_US, ESC_MAX_US);
  pwmUs = ESC_SAFE_US;
  applyPwm();

  attachRpm();
  resetRpmStats();

  Serial.println();
  Serial.println("TEST_STARTER booted. Starter DISARMED at 1000us (khong quay).");
  Serial.println("Dung '+' / '-' de tang/giam PWM. Go 'help' de xem menu.");
  printHelp();

  delay(2000);   // giữ 1000us đủ lâu để ESC arm an toàn
  Serial.println("ESC armed. San sang test.");
}

uint32_t lastStatusMs = 0;

void loop() {
  handleSerial();
  updateRpm();

  uint32_t nowMs = millis();
  if (nowMs - lastStatusMs >= STATUS_PRINT_MS) {
    lastStatusMs = nowMs;
    printStatus();
  }
}
