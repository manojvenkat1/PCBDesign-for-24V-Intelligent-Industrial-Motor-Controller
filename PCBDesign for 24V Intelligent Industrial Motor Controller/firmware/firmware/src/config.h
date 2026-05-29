#pragma once

// ─────────────────────────────────────────────
//  NETWORK
// ─────────────────────────────────────────────
#define WIFI_SSID     "YourNetworkName"
#define WIFI_PASS     "YourPassword"

// ─────────────────────────────────────────────
//  PIN MAP  (ESP32-WROOM-32E)
// ─────────────────────────────────────────────
#define PWM_PIN       12      // IO12 → DRV8874 IN1
#define DIR_PIN       13      // IO13 → DRV8874 IN2
#define ENC_A_PIN     34      // IO34 input-only (encoder channel A)
#define ENC_B_PIN     35      // IO35 input-only (encoder channel B)
#define LS1_PIN       36      // IO36 input-only (limit switch 1 — home)
#define LS2_PIN       39      // IO39 input-only (limit switch 2 — far end)
#define VSENSE_PIN    33      // IO33 ADC1_CH5  (24V voltage divider)
#define TEMP_PIN      32      // IO32 ADC1_CH4  (MCP9700AT temperature)

// ─────────────────────────────────────────────
//  PWM
// ─────────────────────────────────────────────
#define PWM_CHANNEL   0
#define PWM_FREQ_HZ   20000   // 20 kHz — above audible range
#define PWM_BITS      8       // resolution 0–255

// ─────────────────────────────────────────────
//  ENCODER & MECHANICS
// ─────────────────────────────────────────────
#define PPR               500      // pulses per revolution (quadrature → ×4 decoding)
#define PULSES_PER_REV    (PPR * 4)
#define LEAD_MM           5.0f     // mm of linear travel per motor revolution
#define MM_PER_PULSE      (LEAD_MM / PULSES_PER_REV)

// ─────────────────────────────────────────────
//  PID  (velocity loop — RPM)
// ─────────────────────────────────────────────
#define PID_KP            1.2f
#define PID_KI            0.6f
#define PID_KD            0.05f
#define PID_INTEGRAL_MAX  120.0f   // anti-windup clamp
#define PID_DEADBAND_RPM  3.0f     // skip integral below this error

// ─────────────────────────────────────────────
//  TIMING
// ─────────────────────────────────────────────
#define LOOP_MS           10       // main control tick period (ms)
#define SENSOR_EVERY_N    5        // read slow sensors every N ticks
#define WS_BROADCAST_MS   50       // WebSocket broadcast period (ms)

// ─────────────────────────────────────────────
//  SENSING
// ─────────────────────────────────────────────
#define VSENSE_R_HIGH     100000.0f   // 100 kΩ upper divider
#define VSENSE_R_LOW      10000.0f    // 10 kΩ lower divider
#define ADC_REF_V         3.3f
#define ADC_RESOLUTION    4095.0f     // 12-bit
// MCP9700AT: Vout = 500mV + 10mV/°C
#define TEMP_V0           0.500f
#define TEMP_TC           0.010f

// ─────────────────────────────────────────────
//  MANUAL INTERVENTION
// ─────────────────────────────────────────────
#define MANUAL_THRESHOLD_PULSES  15  // pulses moved while motor stopped → flag

// ─────────────────────────────────────────────
//  HOMING
// ─────────────────────────────────────────────
#define HOMING_PWM        60         // slow PWM during homing
#define HOMING_DIR        LOW        // direction toward LS1
