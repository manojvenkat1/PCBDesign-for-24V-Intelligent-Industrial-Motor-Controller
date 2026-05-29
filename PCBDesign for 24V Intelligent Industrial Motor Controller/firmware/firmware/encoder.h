#pragma once
#include <Arduino.h>
#include "config.h"

// ─────────────────────────────────────────────
//  Shared state (written by ISR, read by loop)
// ─────────────────────────────────────────────
volatile long  encCount     = 0;   // absolute pulse count (signed)
volatile long  encPrev      = 0;   // snapshot for velocity calc
portMUX_TYPE   encMux       = portMUX_INITIALIZER_UNLOCKED;

// ─────────────────────────────────────────────
//  ISR — must live in IRAM, executes in < 1 µs
//  Quadrature decode: A edge → read B to get direction
// ─────────────────────────────────────────────
void IRAM_ATTR encoderISR_A() {
    portENTER_CRITICAL_ISR(&encMux);
    bool a = digitalRead(ENC_A_PIN);
    bool b = digitalRead(ENC_B_PIN);
    // rising A with B=LOW → CW (+1), B=HIGH → CCW (-1)
    encCount += (a ^ b) ? -1 : 1;
    portEXIT_CRITICAL_ISR(&encMux);
}

void IRAM_ATTR encoderISR_B() {
    portENTER_CRITICAL_ISR(&encMux);
    bool a = digitalRead(ENC_A_PIN);
    bool b = digitalRead(ENC_B_PIN);
    // rising B with A=HIGH → CW (+1), A=LOW → CCW (-1)
    encCount += (a ^ b) ? 1 : -1;
    portEXIT_CRITICAL_ISR(&encMux);
}

// ─────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────
void encoderInit() {
    pinMode(ENC_A_PIN, INPUT);
    pinMode(ENC_B_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(ENC_A_PIN), encoderISR_A, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B_PIN), encoderISR_B, CHANGE);
}

// Returns snapshot of absolute count (atomic read)
long encoderGetCount() {
    portENTER_CRITICAL(&encMux);
    long c = encCount;
    portEXIT_CRITICAL(&encMux);
    return c;
}

// Resets absolute count to zero (call after homing)
void encoderReset() {
    portENTER_CRITICAL(&encMux);
    encCount = 0;
    encPrev  = 0;
    portEXIT_CRITICAL(&encMux);
}

// Returns velocity in RPM based on pulse delta since last call
// Call once per LOOP_MS tick — dt in seconds
float encoderGetRPM(float dt) {
    portENTER_CRITICAL(&encMux);
    long cur = encCount;
    portEXIT_CRITICAL(&encMux);

    long delta = cur - encPrev;
    encPrev    = cur;

    // delta pulses / PULSES_PER_REV / dt → revolutions per second → RPM
    return (float)delta / (float)PULSES_PER_REV / dt * 60.0f;
}

// Returns absolute position in mm from home
float encoderGetMM() {
    return (float)encoderGetCount() * MM_PER_PULSE;
}
