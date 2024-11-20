## User Interface
<div style="display: flex; justify-content: center; align-items: center; gap: 10px;">
  <img src="./UI/UI.jpg" alt="Home" width="500">
  <img src="./UI/UII.jpg" alt="Logs" width="400">
</div>

## Features
- **Environmental Monitoring:**
  - Measures temperature, humidity, and calculates heat index using the Adafruit AHTX0 sensor.
  - Real-time data updates via WebSocket.
- **Motion Detection:**
  - Detects motion using the RCWL-0516 microwave radar sensor and triggers an audible buzzer and LED indicator.
  - Automatically turns off indicators after a configurable timeout.
- **Wi-Fi Access Point:**
  - Hosts an HTML interface for monitoring data.
  - Automatically shuts down if no clients connect for a specified duration.
- **SPIFFS Support:**
  - Serves an HTML file stored in the ESP32's file system.

## Pin Configuration
| Component        | GPIO Pin |
|-------------------|----------|
| Passive Buzzer    | GPIO1    |
| LED               | GPIO4    |
| RCWL-0516         | GPIO5    |
| AHT30 (I2C SDA)   | GPIO21   |
| AHT30 (I2C SCL)   | GPIO22   |
