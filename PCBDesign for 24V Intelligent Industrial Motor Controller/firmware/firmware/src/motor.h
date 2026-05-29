#pragma once
#include <Arduino.h>
#include "config.h"
#include "encoder.h"

// ─────────────────────────────────────────────
//  Motor state
// ─────────────────────────────────────────────
bool  motorHomed   = false;
float motorMaxMM   = 0.0f;    // set during homing (LS1→LS2 distance)

// ─────────────────────────────────────────────
//  Init PWM channel and direction pin
// ─────────────────────────────────────────────
void motorInit() {
    ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_BITS);
    ledcAttachPin(PWM_PIN, PWM_CHANNEL);
    pinMode(DIR_PIN, OUTPUT);
    ledcWrite(PWM_CHANNEL, 0);
    digitalWrite(DIR_PIN, HIGH);
    Serial.println("[MOTOR] PWM init OK — 20kHz / 8-bit");
}

// ─────────────────────────────────────────────
//  Read limit switches (active LOW — pulled high)
// ─────────────────────────────────────────────
bool limitSwitch1() { return digitalRead(LS1_PIN) == LOW; }   // home end
bool limitSwitch2() { return digitalRead(LS2_PIN) == LOW; }   // far  end

// ─────────────────────────────────────────────
//  Apply PWM + direction with limit switch guard
//  Returns actual PWM written (0 if blocked by LS)
// ─────────────────────────────────────────────
uint8_t motorWrite(uint8_t pwm, uint8_t dir) {
    // Hard stop if limit switch triggered in that direction
    if (dir == HOMING_DIR && limitSwitch1()) { pwm = 0; }
    if (dir != HOMING_DIR && limitSwitch2()) { pwm = 0; }

    digitalWrite(DIR_PIN, dir);
    ledcWrite(PWM_CHANNEL, pwm);
    return pwm;
}

// ─────────────────────────────────────────────
//  Hard stop — zero PWM, reset PID externally
// ─────────────────────────────────────────────
void motorStop() {
    ledcWrite(PWM_CHANNEL, 0);
}

// ─────────────────────────────────────────────
//  Homing routine — blocking, call from setup()
//  Phase 1: drive toward LS1 at low speed until triggered
//  Phase 2: back off slightly
//  Phase 3: drive to LS2 at low speed to measure total travel
//  Phase 4: return to home (LS1), zero encoder
// ─────────────────────────────────────────────
void motorHome() {
    Serial.println("[HOME] Phase 1 — moving to LS1");
    digitalWrite(DIR_PIN, HOMING_DIR);
    ledcWrite(PWM_CHANNEL, HOMING_PWM);

    // Wait until LS1 triggers (timeout 15s)
    uint32_t t0 = millis();
    while (!limitSwitch1()) {
        if (millis() - t0 > 15000) {
            Serial.println("[HOME] TIMEOUT phase 1 — check LS1 wiring");
            motorStop();
            return;
        }
        delay(1);
    }
    motorStop();
    encoderReset();
    Serial.println("[HOME] LS1 reached — encoder zeroed");
    delay(200);

    // Phase 2 — back off 5 mm
    Serial.println("[HOME] Phase 2 — backing off 5 mm");
    long backOff = (long)(5.0f / MM_PER_PULSE);
    encoderReset();
    digitalWrite(DIR_PIN, !HOMING_DIR);
    ledcWrite(PWM_CHANNEL, HOMING_PWM);
    while (encoderGetCount() < backOff) delay(1);
    motorStop();
    delay(200);

    // Phase 3 — drive to LS2 to measure full travel
    Serial.println("[HOME] Phase 3 — measuring travel to LS2");
    encoderReset();
    digitalWrite(DIR_PIN, !HOMING_DIR);
    ledcWrite(PWM_CHANNEL, HOMING_PWM);
    t0 = millis();
    while (!limitSwitch2()) {
        if (millis() - t0 > 30000) {
            Serial.println("[HOME] TIMEOUT phase 3 — check LS2 wiring");
            motorStop();
            return;
        }
        delay(1);
    }
    motorStop();
    motorMaxMM = encoderGetMM();
    Serial.printf("[HOME] Total travel: %.2f mm\n", motorMaxMM);
    delay(200);

    // Phase 4 — return to home
    Serial.println("[HOME] Phase 4 — returning to LS1");
    digitalWrite(DIR_PIN, HOMING_DIR);
    ledcWrite(PWM_CHANNEL, HOMING_PWM);
    t0 = millis();
    while (!limitSwitch1()) {
        if (millis() - t0 > 30000) { motorStop(); return; }
        delay(1);
    }
    motorStop();
    encoderReset();
    motorHomed = true;
    Serial.println("[HOME] Complete — system ready");
}
