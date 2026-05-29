# Accio Robotics — Controls Intern Firmware
### Candidate: Manoj V

---

## File Structure
```
firmware/
├── src/
│   ├── main.ino        ← open this in Arduino IDE
│   ├── config.h        ← edit WiFi credentials + PID gains here
│   ├── encoder.h       ← quadrature ISR + velocity/position API
│   ├── pid.h           ← PID class with anti-windup
│   ├── sensors.h       ← INA219 + MCP9700 + ADC voltage divider
│   ├── motor.h         ← PWM, homing, limit switch guard
│   └── webserver.h     ← WiFi, AsyncWebServer, WebSocket, JSON
├── data/
│   └── index.html      ← UI — upload via SPIFFS data upload tool
├── PSEUDOCODE.txt
└── README.md
```

---

## Step-by-Step Flash Instructions

### 1. Install Arduino IDE 2.x
Download from https://www.arduino.cc/en/software

### 2. Add ESP32 board support
File → Preferences → Additional Boards Manager URLs:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
Then: Tools → Boards Manager → search "esp32" → install **esp32 by Espressif Systems**

### 3. Install libraries (Tools → Manage Libraries)
| Library | Version |
|---|---|
| Adafruit INA219 | latest |
| ArduinoJson | 6.x |

Install from GitHub ZIP (not in library manager):
- https://github.com/me-no-dev/ESPAsyncWebServer
- https://github.com/me-no-dev/AsyncTCP

### 4. Install SPIFFS upload plugin
Download: https://github.com/me-no-dev/arduino-esp32fs-plugin/releases
Place the JAR into `<Arduino>/tools/ESP32FS/tool/` and restart IDE.

### 5. Edit config.h
```cpp
#define WIFI_SSID  "YourNetworkName"
#define WIFI_PASS  "YourPassword"
```

### 6. Board settings
Tools menu:
- **Board**: ESP32 Dev Module
- **Upload Speed**: 921600
- **CPU Frequency**: 240 MHz
- **Flash Size**: 4MB (32Mb)
- **Partition Scheme**: Default 4MB with spiffs

### 7. Upload UI first (SPIFFS)
Tools → **ESP32 Sketch Data Upload**
This uploads `data/index.html` to the flash filesystem.

### 8. Flash firmware
Sketch → Upload (or Ctrl+U)
Hold BOOT button during upload if needed.

### 9. Find IP address
Open Serial Monitor at 115200 baud.
Look for: `[WiFi] Connected — IP: 192.168.x.x`

### 10. Open UI
Navigate to `http://192.168.x.x` in any browser on the same WiFi network.

---

## PID Tuning Guide
1. Set Ki=0, Kd=0 in config.h
2. Increase Kp until velocity oscillates steadily (note Ku, Tu)
3. Apply Ziegler-Nichols: Kp=0.6Ku, Ki=2Kp/Tu, Kd=KpTu/8
4. Fine-tune using Serial Plotter (Tools → Serial Plotter)
5. Starting point: Kp=1.2, Ki=0.6, Kd=0.05

---

## Assumptions Made
- Motor encoder: 500 PPR quadrature (×4 = 2000 counts/rev)
- Lead screw: 5mm travel per motor revolution
- Max safe RPM: 3000 (adjust in config.h per actual motor spec)
- Manual intervention threshold: 15 encoder pulses while motor stopped
- Limit switches: normally open, active LOW with hardware pull-up resistors
- Temperature sensor placed near DRV8874, measures PCB temperature
- INA219 measures motor current directly (shunt R1 = 0.1Ω in motor path)
