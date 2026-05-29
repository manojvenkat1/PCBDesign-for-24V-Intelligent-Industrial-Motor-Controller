#pragma once
#include <Arduino.h>
#include "config.h"

class PID {
public:
    float kp, ki, kd;

    PID(float p, float i, float d)
        : kp(p), ki(i), kd(d),
          _integral(0.0f), _prevError(0.0f), _prevOutput(0.0f) {}

    // ─────────────────────────────────────────
    //  compute() — call once every control tick
    //  setpoint : desired velocity (RPM)
    //  measured : actual velocity  (RPM)
    //  dt       : time since last call (seconds)
    //  returns  : PWM value 0–255
    // ─────────────────────────────────────────
    float compute(float setpoint, float measured, float dt) {
        if (dt <= 0.0f) return _prevOutput;

        float error = setpoint - measured;

        // Deadband — avoid integral wind-up at near-zero error
        if (fabsf(error) > PID_DEADBAND_RPM) {
            _integral += error * dt;
        }
        // Anti-windup clamp
        _integral = constrain(_integral, -PID_INTEGRAL_MAX, PID_INTEGRAL_MAX);

        float derivative = (error - _prevError) / dt;
        _prevError = error;

        float output = kp * error + ki * _integral + kd * derivative;
        output = constrain(output, 0.0f, 255.0f);

        _prevOutput = output;
        return output;
    }

    // Call when motor is stopped or direction changes
    void reset() {
        _integral   = 0.0f;
        _prevError  = 0.0f;
        _prevOutput = 0.0f;
    }

    // Runtime tuning (from WebSocket UI)
    void setGains(float p, float i, float d) {
        kp = p; ki = i; kd = d;
        reset();
    }

private:
    float _integral;
    float _prevError;
    float _prevOutput;
};
