// ═══════════════════════════════════════════════════════════════════════════
//  Accio Robotics — Controls Intern Task
//  ESP32-WROOM-32E Firmware
//  Author : Manoj V
//  Board  : esp32 by Espressif (Arduino IDE 2.x)
//  Target : ESP32-WROOM-32E  (LCSC C529587)
//
//  Required libraries (install via Arduino Library Manager):
//    - Adafruit INA219          (current / voltage sensor)
//    - ESPAsyncWebServer        (install from GitHub ZIP)
//    - AsyncTCP                 (install from GitHub ZIP, dependency of above)
//    - ArduinoJson              (v6.x)
//
//  SPIFFS data upload:
//    1. Install "ESP32 Sketch Data Upload" plugin for Arduino IDE
//    2. Place index.html inside  <sketchfolder>/data/
//    3. Tools → ESP32 Sketch Data Upload  (before flashing firmware)
//
//  Board settings in Arduino IDE:
//    Board      : "ESP32 Dev Module"
//    Upload Spd : 921600
//    CPU Freq   : 240 MHz
//    Flash Freq : 80 MHz
//    Flash Size : 4MB (32Mb)
//    Part Scheme: Default 4MB with spiffs
//    PSRAM      : Disabled
// ═══════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "config.h"
#include "encoder.h"
#include "pid.h"
#include "sensors.h"
#include "motor.h"
#include "webserver.h"

// ─────────────────────────────────────────────
//  Global PID instance + tunable gains
// ─────────────────────────────────────────────
PID pid(PID_KP, PID_KI, PID_KD);
float g_kp = PID_KP;
float g_ki = PID_KI;
float g_kd = PID_KD;

// ─────────────────────────────────────────────
//  Runtime state
// ─────────────────────────────────────────────
SensorData sensors;

float  currentVelocityRPM = 0.0f;
float  currentPosMM       = 0.0f;
long   expectedPulses     = 0;      // for manual intervention detection
bool   manualFlag         = false;

// Position control state
bool   positionMode       = false;  // true when UI sends a position target
float  positionKp         = 3.0f;   // outer position loop gain

// Timing
uint32_t lastLoopMs       = 0;
uint32_t lastBroadcastMs  = 0;
uint8_t  sensorTick       = 0;

// ─────────────────────────────────────────────
//  applyStop() — called from webserver.h on "stop" command
// ─────────────────────────────────────────────
void applyStop() {
    motorStop();
    pid.reset();
    targetVelocityRPM = 0.0f;
    positionMode      = false;
    Serial.println("[CMD] Motor stopped by UI");
}

// ─────────────────────────────────────────────
//  Homing wrapper (non-blocking gate)
// ─────────────────────────────────────────────
void runHomingIfRequested() {
    if (!homingRequested) return;
    homingRequested = false;
    applyStop();
    delay(100);
    motorHome();
    // After homing: push PID gains that may have changed via UI
    pid.setGains(g_kp, g_ki, g_kd);
}

// ─────────────────────────────────────────────
//  Manual intervention detection
//  Rule: if motor is commanded stopped but encoder
//  moves more than MANUAL_THRESHOLD_PULSES → flag
// ─────────────────────────────────────────────
void checkManualIntervention() {
    if (targetVelocityRPM > 1.0f) {
        // Motor running — update expected
        expectedPulses = encoderGetCount();
        manualFlag     = false;
        return;
    }
    long actual = encoderGetCount();
    long delta  = abs(actual - expectedPulses);
    if (delta > MANUAL_THRESHOLD_PULSES) {
        manualFlag     = true;
        expectedPulses = actual;   // accept new position as reference
        Serial.printf("[MANUAL] Intervention detected — Δ%ld pulses\n", delta);
    }
}

// ─────────────────────────────────────────────
//  Outer position loop (runs when positionMode=true)
//  Generates velocity setpoint from position error
// ─────────────────────────────────────────────
float positionToVelocity(float targetMM, float actualMM) {
    float error = targetMM - actualMM;
    // Dead zone: within 0.1 mm → stop
    if (fabsf(error) < 0.1f) return 0.0f;
    // Direction
    digitalWrite(DIR_PIN, error > 0 ? HIGH : LOW);
    // P-controller clamped to max 500 RPM
    return constrain(fabsf(error) * positionKp * 10.0f, 5.0f, 500.0f);
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[BOOT] Accio Robotics Motor Controller");

    // Limit switch inputs (hardware pull-ups on PCB; internal as backup)
    pinMode(LS1_PIN, INPUT);
    pinMode(LS2_PIN, INPUT);

    // Motor PWM + direction
    motorInit();

    // Encoder interrupts
    encoderInit();

    // Sensors (INA219 via I2C)
    if (!sensorsInit()) {
        Serial.println("[WARN] Sensor init failed — continuing without INA219");
    }

    // WiFi
    if (!wifiConnect()) {
        Serial.println("[WARN] No WiFi — running in standalone mode");
    } else {
        // Web server + WebSocket (only if WiFi connected)
        webServerInit();
    }

    // Homing — drives motor to LS1 and measures full travel
    motorHome();

    // Sync PID gains
    pid.setGains(g_kp, g_ki, g_kd);
    expectedPulses = 0;

    lastLoopMs      = millis();
    lastBroadcastMs = millis();
    Serial.println("[BOOT] Ready\n");
}

// ─────────────────────────────────────────────
//  MAIN LOOP  (10 ms control tick)
// ─────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    // ── 1. Clean up stale WebSocket clients
    ws.cleanupClients();

    // ── 2. Handle homing request from UI
    runHomingIfRequested();

    // ── 3. Control tick gate (10 ms)
    if (now - lastLoopMs < LOOP_MS) return;
    float dt      = (float)(now - lastLoopMs) / 1000.0f;
    lastLoopMs    = now;

    // ── 4. Read encoder — velocity + position
    currentVelocityRPM = encoderGetRPM(dt);
    currentPosMM       = encoderGetMM();

    // ── 5. Limit switch guard — hard stop
    bool ls1 = limitSwitch1();
    bool ls2 = limitSwitch2();

    if (ls1 && motorDirection == HOMING_DIR) {
        applyStop();
        Serial.println("[LS] LS1 hit — stopped");
        return;
    }
    if (ls2 && motorDirection != HOMING_DIR) {
        applyStop();
        Serial.println("[LS] LS2 hit — stopped");
        return;
    }

    // ── 6. Manual intervention detection
    checkManualIntervention();

    // ── 7. If stop requested — stay stopped
    if (stopRequested) {
        motorStop();
        return;
    }

    // ── 8. Position mode outer loop
    float velocitySetpoint = targetVelocityRPM;
    if (positionMode) {
        velocitySetpoint = positionToVelocity(targetPositionMM, currentPosMM);
        if (velocitySetpoint < 1.0f) {
            // Reached target
            applyStop();
            positionMode = false;
        }
    }

    // ── 9. PID inner loop — velocity → PWM
    if (velocitySetpoint > 0.0f) {
        // Sync gains if changed via UI tune command
        pid.setGains(g_kp, g_ki, g_kd);
        float pwm = pid.compute(velocitySetpoint, fabsf(currentVelocityRPM), dt);
        motorWrite((uint8_t)pwm, motorDirection);
    } else {
        motorStop();
        pid.reset();
    }

    // ── 10. Read sensors every SENSOR_EVERY_N ticks (50 ms)
    sensorTick++;
    if (sensorTick >= SENSOR_EVERY_N) {
        sensorTick = 0;
        sensors = readAllSensors();
    }

    // ── 11. Broadcast state to WebSocket UI (every WS_BROADCAST_MS)
    if (now - lastBroadcastMs >= WS_BROADCAST_MS) {
        lastBroadcastMs = now;
        broadcastState(
            currentVelocityRPM,
            currentPosMM,
            sensors.currentMA,
            sensors.busVoltage,
            sensors.supplyV,
            sensors.tempC,
            ls1, ls2,
            manualFlag,
            motorHomed
        );
    }

    // ── 12. Serial monitor debug (comment out for production)
    // Serial.printf("VEL=%.1f RPM | POS=%.2f mm | I=%.0f mA | T=%.1f C\n",
    //               currentVelocityRPM, currentPosMM,
    //               sensors.currentMA, sensors.tempC);
}
