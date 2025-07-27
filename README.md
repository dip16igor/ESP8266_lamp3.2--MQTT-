# ESP8266 Smart Lamp with MQTT

[Русская версия (Russian version)](README_RU.md)

A smart lamp based on ESP8266 with MQTT protocol support, manual brightness control via encoder, and Telegram bot integration.

## ⚙️ Features

- **Hardware Control:**
  - Manual brightness control using rotary encoder
  - DC-DC converter for efficient LED power management
  - MCP4725 12-bit DAC for precise brightness control
  - 16-LED NeoPixel strip for visual feedback
- **Communication:**
  - MQTT protocol support for home automation integration
  - Telegram bot for remote control and notifications
  - WiFi connectivity with automatic reconnection
- **Visual Effects:**
  - Rainbow animation on LED strip
  - Smooth brightness transitions
  - Status indication through LED colors
- **Smart Features:**
  - Gamma correction for natural brightness perception
  - Automatic state recovery after power loss
  - Memory efficient operation with heap monitoring

## 🔌 Hardware Components

- **ESP8266** microcontroller
- **MCP4725** DAC for LED control
- **Rotary Encoder** with button for manual control
- **NeoPixel (WS2812B)** LED strip with 16 LEDs
- **DC-DC Converter** for LED power management

## 📚 Libraries Used

- `Adafruit_MCP4725` - For DAC control
- `Adafruit_NeoPixel` - For LED strip control
- `PubSubClient` - For MQTT communication
- `FastBot` - For Telegram bot integration
- `EncButton` - For encoder handling

## 🛠️ Setup

1. Create `src/secrets.h` with your configuration:
```cpp
#pragma once

// WiFi Settings
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Telegram Bot Settings
#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define ADMIN_ID "YOUR_CHAT_ID"

// MQTT Server Settings
#define mqtt_server "YOUR_MQTT_SERVER"
```

## 🤖 MQTT Topics

- **Input** (`Home/Light1/Action`):
  - `LIGHT_ON` - Turn the light on
  - `LIGHT_OFF` - Turn the light off
  - `Status` - Request current state
- **Output** (`Home/Light1/Log`):
  - Status messages and confirmations
- **Monitoring** (`Home/Light1/FreeHeap`):
  - Memory usage statistics

## 📱 Control Methods

1. **Manual Control:**
   - Rotate encoder for brightness adjustment
   - Click encoder for quick on/off
   - Long press for special functions

2. **MQTT Control:**
   - Send commands via MQTT for home automation
   - Integrate with systems like Home Assistant

3. **Telegram Commands:**
   - `/start` - Show available commands
   - `/ping` - Get status (uptime, WiFi signal)
   - `/restart` - Restart device

## 💡 LED Indicators

The NeoPixel strip shows:
- Current brightness level
- Operation mode
- Connection status
- Error states

Colors indicate:
- Green - Normal operation
- Red - Off or error state
- Blue - Connecting
- Purple - Command received
- Cyan - Special modes
