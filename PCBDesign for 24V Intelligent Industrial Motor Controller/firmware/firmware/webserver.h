#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "config.h"

// ─────────────────────────────────────────────
//  Shared setpoints — written by WS, read by loop
// ─────────────────────────────────────────────
volatile float  targetVelocityRPM = 0.0f;   // RPM setpoint for PID
volatile float  targetPositionMM  = 0.0f;   // position setpoint (mm from home)
volatile bool   stopRequested     = false;   // UI pressed STOP
volatile bool   homingRequested   = false;   // UI requested re-home
volatile uint8_t motorDirection   = HIGH;    // HIGH=FWD, LOW=REV

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Forward declaration — defined in main .ino
extern void applyStop();

// ─────────────────────────────────────────────
//  Parse incoming WebSocket message (JSON)
// ─────────────────────────────────────────────
void onWSMessage(void* arg, uint8_t* data, size_t len) {
    StaticJsonDocument<192> doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        Serial.printf("[WS] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* cmd = doc["cmd"];
    if (!cmd) return;

    if (strcmp(cmd, "vel") == 0) {
        float v = doc["val"] | 0.0f;
        targetVelocityRPM = constrain(v, 0.0f, 3000.0f);
        stopRequested = false;
        Serial.printf("[WS] Target vel → %.1f RPM\n", targetVelocityRPM);
    }
    else if (strcmp(cmd, "pos") == 0) {
        float p = doc["val"] | 0.0f;
        targetPositionMM = constrain(p, 0.0f, 500.0f);
        Serial.printf("[WS] Target pos → %.2f mm\n", targetPositionMM);
    }
    else if (strcmp(cmd, "fwd") == 0) {
        motorDirection = HIGH;
        digitalWrite(DIR_PIN, HIGH);
    }
    else if (strcmp(cmd, "rev") == 0) {
        motorDirection = LOW;
        digitalWrite(DIR_PIN, LOW);
    }
    else if (strcmp(cmd, "stop") == 0) {
        stopRequested = true;
        targetVelocityRPM = 0.0f;
        applyStop();
    }
    else if (strcmp(cmd, "home") == 0) {
        homingRequested = true;
    }
    else if (strcmp(cmd, "tune") == 0) {
        // UI can push new PID gains: {"cmd":"tune","kp":1.2,"ki":0.6,"kd":0.05}
        extern float g_kp, g_ki, g_kd;
        g_kp = doc["kp"] | PID_KP;
        g_ki = doc["ki"] | PID_KI;
        g_kd = doc["kd"] | PID_KD;
        Serial.printf("[WS] PID tune → Kp=%.3f Ki=%.3f Kd=%.3f\n", g_kp, g_ki, g_kd);
    }
}

// ─────────────────────────────────────────────
//  WebSocket event handler
// ─────────────────────────────────────────────
void onWSEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] Client #%u connected from %s\n",
                          client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] Client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            onWSMessage(arg, data, len);
            break;
        case WS_EVT_ERROR:
            Serial.printf("[WS] Error #%u: %s\n", client->id(), (char*)data);
            break;
        default: break;
    }
}

// ─────────────────────────────────────────────
//  Broadcast JSON to all connected clients
// ─────────────────────────────────────────────
void broadcastState(float velRPM, float posMM,
                    float currMA, float busV, float supplyV, float tempC,
                    bool ls1, bool ls2, bool manualFlag, bool isHomed) {
    if (ws.count() == 0) return;   // nobody connected — skip

    StaticJsonDocument<320> doc;
    doc["vel"]    = round(velRPM * 10) / 10.0f;   // 1 decimal
    doc["pos"]    = round(posMM  * 100) / 100.0f; // 2 decimals
    doc["curr"]   = round(currMA);
    doc["busV"]   = round(busV * 100) / 100.0f;
    doc["supV"]   = round(supplyV * 10) / 10.0f;
    doc["temp"]   = round(tempC * 10) / 10.0f;
    doc["ls1"]    = ls1;
    doc["ls2"]    = ls2;
    doc["manual"] = manualFlag;
    doc["homed"]  = isHomed;
    doc["ts"]     = millis();

    String payload;
    serializeJson(doc, payload);
    ws.textAll(payload);
}

// ─────────────────────────────────────────────
//  WiFi + Server init
// ─────────────────────────────────────────────
bool wifiConnect() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[WiFi] Connecting");
    uint8_t tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 30) {
        delay(500); Serial.print("."); tries++;
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[WiFi] FAILED — check SSID/password in config.h");
        return false;
    }
    Serial.printf("\n[WiFi] Connected — IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

void webServerInit() {
    if (!SPIFFS.begin(true)) {
        Serial.println("[SPIFFS] Mount failed — did you upload data/ folder?");
        return;
    }

    // Serve UI: GET / → /data/index.html stored in SPIFFS flash
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    // 404 fallback
    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    // WebSocket handler
    ws.onEvent(onWSEvent);
    server.addHandler(&ws);

    server.begin();
    Serial.println("[WebServer] Started on port 80");
}
