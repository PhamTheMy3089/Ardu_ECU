/*
  ============================================================================
  TEST_RPM_RAWCOUNT - Phep thu toi gian: ESP32 co thay het canh tren GPIO33?
  ============================================================================
  Muc dich: co lap hoan toan cau hoi "ISR co nhan dung so canh xuat hien tren
  GPIO33 hay khong" - khong WiFi, khong WebServer, khong bo loc, khong thuat
  toan RPM. Chi dem canh tho (cnt++) va in so canh/giay qua Serial.

  Cach doc ket qua:
    - DSO152 do Fre=91Hz  ->  Serial phai in ~91.
    - Neu Serial chi in ~25-30 du DSO do 91Hz -> ESP32 THUC SU khong nhan
      duoc het canh vat ly tren GPIO33 (loai tru hoan toan loi thuat toan/
      bo loc phan mem, nghi van don ve tin hieu/GND giua 2 board).
    - Neu Serial cung in ~91 -> loi nam o thuat toan tinh RPM (TEST_STARTER.ino),
      khong phai o phan cung/ISR.

  Phan cung: giong TEST_STARTER.ino, chi dung 2 chan.
    - Starter ESC signal : GPIO 25
    - RPM sensor (Hall)  : GPIO 33
    THAO canh/impeller khoi starter truoc khi test.

  Lenh Serial: + / - tang giam PWM starter, 0 dung, help in lai menu nay.
  ============================================================================
*/

#include <Arduino.h>
#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif
#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// ---------------- Pins ----------------
#define PIN_ESC_START   25
#define PIN_RPM         33

// ---------------- ESC range ----------------
static const int ESC_SAFE_US = 1000;   // starter KHONG quay
static const int ESC_MIN_US  = 1000;
static const int ESC_MAX_US  = 2000;

// ---------------- ESC PWM via raw LEDC (khong dung ESP32Servo, xem CLAUDE.md) ----------------
static const int ESC_PWM_FREQ_HZ  = 50;
static const int ESC_PWM_RES_BITS = 16;
static const int LEDC_CH_START    = 1;

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

int pwmUs     = ESC_SAFE_US;
int pwmStepUs = 10;

void applyPwm() {
  escWriteUs(PIN_ESC_START, LEDC_CH_START, pwmUs);
}

void setPwm(int us) {
  pwmUs = constrain(us, ESC_MIN_US, ESC_MAX_US);
  applyPwm();
}

// ---------------- Bare edge counter - KHONG loc gi ca ----------------
volatile uint32_t cnt = 0;

void IRAM_ATTR rawIsr() {
  cnt++;
}

void printHelp() {
  Serial.println("TEST_RPM_RAWCOUNT - dem canh tho GPIO33, khong loc, khong WiFi.");
  Serial.println("  +/-     tang/giam PWM starter");
  Serial.println("  0       dung starter (PWM=1000us)");
  Serial.println("  help    in lai menu nay");
  Serial.println("Moi giay in: RAW_EDGES/s=<n>");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  escAttach(PIN_ESC_START, LEDC_CH_START);
  pwmUs = ESC_SAFE_US;
  applyPwm();

  pinMode(PIN_RPM, INPUT);   // dung pull-up ngoai cua mach cam bien
  attachInterrupt(digitalPinToInterrupt(PIN_RPM), rawIsr, RISING);

  Serial.println();
  Serial.println("TEST_RPM_RAWCOUNT booted. Starter DISARMED at 1000us.");
  printHelp();

  delay(2000);   // giu 1000us du lau de ESC arm an toan
  Serial.println("ESC armed. San sang test.");
}

void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '+') { setPwm(pwmUs + pwmStepUs); Serial.print("PWM="); Serial.print(pwmUs); Serial.println("us (+)"); continue; }
    if (c == '-') { setPwm(pwmUs - pwmStepUs); Serial.print("PWM="); Serial.print(pwmUs); Serial.println("us (-)"); continue; }
    if (c == '0') { setPwm(ESC_SAFE_US); Serial.println("STARTER OFF."); continue; }
    if (c == 'h') { printHelp(); continue; }
  }
}

uint32_t lastPrintMs = 0;

void loop() {
  handleSerial();
  // KHONG goi applyPwm()/ledcWrite() moi vong lap: LEDC la PWM phan cung,
  // tu phat song doc lap sau khi ledcWrite() duoc goi 1 lan (khac ESP32Servo).
  // Goi lai lien tuc khong can thiet va co the chiem dung CPU/critical-section,
  // nghi ngo la nguyen nhan gay tran cung ~25 xung/s quan sat duoc khi doi
  // chieu voi DSO152 (RAW_EDGES/s bi ghim trong khi Fre do duoc van tang).
  // setPwm() da goi applyPwm() moi khi gia tri PWM thuc su doi.

  uint32_t nowMs = millis();
  if (nowMs - lastPrintMs >= 1000) {
    lastPrintMs = nowMs;
    noInterrupts();
    uint32_t c = cnt;
    cnt = 0;
    interrupts();
    Serial.print("PWM="); Serial.print(pwmUs); Serial.print("us");
    if (pwmUs <= ESC_SAFE_US) Serial.print("(OFF)");
    Serial.print(" | RAW_EDGES/s=");
    Serial.println(c);
  }
}
