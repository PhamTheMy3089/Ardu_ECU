# Lessons Learned

## Do not use the ESP32Servo library for ESC PWM output

**What happened**: The firmware used `Servo` from `ESP32Servo.h` to drive the
pump/starter ESCs (`escPump.writeMicroseconds()` / `escStart.writeMicroseconds()`).
On the bench, the starter motor stuttered/stalled even with adequate battery
current. The user independently wrote a test sketch using the ESP32 LEDC API
directly (`ledcAttach`/`ledcWrite`, computing duty cycle by hand) on the exact
same hardware and power supply — it ran smoothly, no stutter.

**Root cause**: ESP32Servo's internal timer/LEDC setup is version-sensitive.
ESP32 Arduino core 3.x changed the LEDC API (`ledcSetup`/`ledcAttachPin` →
`ledcAttach`), and a mismatched/outdated ESP32Servo version can end up
generating a subtly malformed PWM pulse (wrong frequency/timer config) that
looks fine in code but that the ESC reads as noisy/invalid throttle input —
causing it to cut or stutter the motor. This is **independent of and can be
mistaken for a power-supply current problem** (a real, separate issue we'd
already diagnosed earlier in the same debugging session) — don't assume one
explains the stutter without ruling out the other.

**Fix applied**: Replaced `Servo`/`ESP32Servo` with raw LEDC PWM
(`ledcAttach`+`ledcWrite` on core ≥3, `ledcSetup`+`ledcAttachPin`+`ledcWrite`
on core <3, gated by `ESP_ARDUINO_VERSION_MAJOR`) in both
`PROJECT_CURRENT/Firmware/ECU_TestV1_EGT_DRY_START_PATCH/ECU_TestV1_EGT_DRY_START_PATCH.ino`
and `TEST/TEST_STARTER/TEST_STARTER.ino`. See `escAttach()`/`escWriteUs()` in
the main firmware for the pattern to reuse for any future ESC/servo output on
this project.

**Takeaway for future work on this repo**: If a servo/ESC/PWM-driven actuator
misbehaves (stutter, cutting out, erratic behavior) on this ESP32 project,
don't stop at "check the power supply" — also suspect the PWM generation
library itself, especially after any ESP32 Arduino core or library version
bump. Prefer raw LEDC over ESP32Servo for ESC control on this project going
forward.
