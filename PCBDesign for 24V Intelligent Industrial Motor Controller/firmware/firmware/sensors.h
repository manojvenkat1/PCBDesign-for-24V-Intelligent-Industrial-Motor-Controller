#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "config.h"

Adafruit_INA219 ina219;   // default I2C address 0x40 (A0=A1=GND)

// ─────────────────────────────────────────────
//  Sensor data struct — filled each sensor tick
// ─────────────────────────────────────────────
struct SensorData {
    float currentMA   = 0.0f;   // motor current  (mA)
    float busVoltage  = 0.0f;   // motor bus voltage (V) via INA219
    float supplyV     = 0.0f;   // 24V rail reading via resistor divider
    float tempC       = 0.0f;   // PCB/motor temp (°C) via MCP9700AT
};

// ─────────────────────────────────────────────
//  Init — call in setup() after Wire.begin()
// ─────────────────────────────────────────────
bool sensorsInit() {
    Wire.begin(21, 22);          // SDA=IO21, SCL=IO22
    if (!ina219.begin()) {
        Serial.println("[SENSOR] INA219 not found — check wiring & LCSC C328875");
        return false;
    }
    ina219.setCalibration_16V_400mA();  // adjust if motor draws more; use 32V_2A variant
    // Configure ADC pins
    analogReadResolution(12);            // 12-bit = 0–4095
    analogSetAttenuation(ADC_11db);      // allows reading up to ~3.3 V
    pinMode(VSENSE_PIN, INPUT);
    pinMode(TEMP_PIN,   INPUT);
    Serial.println("[SENSOR] Init OK");
    return true;
}

// ─────────────────────────────────────────────
//  Individual reads
// ─────────────────────────────────────────────
float readCurrentMA() {
    return ina219.getCurrent_mA();
}

float readBusVoltage() {
    return ina219.getBusVoltage_V() + (ina219.getShuntVoltage_mV() / 1000.0f);
}

// 24V rail reconstructed from resistor divider
// Vpin = Vin × R_LOW / (R_HIGH + R_LOW)  →  Vin = Vpin × (R_HIGH + R_LOW) / R_LOW
float readSupplyVoltage() {
    int   raw   = analogRead(VSENSE_PIN);
    float vpin  = (float)raw / ADC_RESOLUTION * ADC_REF_V;
    float ratio = (VSENSE_R_HIGH + VSENSE_R_LOW) / VSENSE_R_LOW;
    return vpin * ratio;
}

// MCP9700AT: Vout = 500 mV + 10 mV/°C
float readTempC() {
    int   raw   = analogRead(TEMP_PIN);
    float volts = (float)raw / ADC_RESOLUTION * ADC_REF_V;
    return (volts - TEMP_V0) / TEMP_TC;
}

// ─────────────────────────────────────────────
//  Read all into struct (call every SENSOR_EVERY_N ticks)
// ─────────────────────────────────────────────
SensorData readAllSensors() {
    SensorData d;
    d.currentMA  = readCurrentMA();
    d.busVoltage = readBusVoltage();
    d.supplyV    = readSupplyVoltage();
    d.tempC      = readTempC();
    return d;
}
