# Tab5 - M5Stack Smart Home Dashboard & Macro Keypad

Tab5 is a versatile, dual-purpose firmware for M5Stack (ESP32) devices, combining a robust **Smart Home Dashboard** for Home Assistant with a **PC Macro Keypad** (Stream Deck alternative). Built with high-performance graphics using **LVGL**.

## 🚀 Features

### 🏠 Smart Home Dashboard
- **Home Assistant Integration:** Seamless 2-way communication via MQTT.
- **Unified Tile Interface:** Visualizes entities as interactive tiles.
  - **Sensors:** Display temperature, humidity, power usage, etc.
  - **Switches:** Toggle lights, plugs, and relays.
  - **Scenes:** Trigger complex home automation scenes.
- **Web-Based Configuration:** No code changes needed to rearrange layout!
  - Built-in Web Admin Panel served directly from the device.
  - Configure MQTT, WiFi, and Tile layouts visually.

### 🎮 Game Mode (Macro Keypad)
- Turns your M5Stack into a programmable macro keyboard.
- **WebSocket Connectivity:** Low-latency communication with the host PC.
- **Desktop Companion App:** Includes an Electron-based host application (`tab5-game-controls`) that receives commands and simulates key presses on your computer.

## 🛠️ Architecture

The project is structured into modular components:

- **Firmware (`src/`)**: C++ code for the ESP32.
  - **Core**: Power, Display, and Config management.
  - **Network**: WiFi, MQTT, and WebSocket servers.
  - **UI/Tiles**: LVGL rendering logic and touch handling.
  - **Web**: Embedded web server for the admin interface.
- **Electron App (`electron-app/`)**: Node.js/Electron client for handling macro commands on Windows/Linux/macOS.
- **Tools (`mdi-extractor/`)**: Utilities for processing Material Design Icons for the embedded display.

## 📦 Getting Started

### 1. Firmware Setup
1. Open `Tab5_LVGL.ino` in your Arduino IDE or PlatformIO.
2. Ensure you have the required libraries installed (LVGL, ArduinoJson, PubSubClient, etc.).
3. Flash the firmware to your M5Stack device.
4. **Initial Setup:**
   - On first boot, the device creates a WiFi Access Point.
   - Connect to it and navigate to the displayed IP to configure your local WiFi and MQTT Broker credentials.

### 2. Desktop App (For Game Mode)
1. Navigate to the `electron-app` directory.
2. Install dependencies:
   ```bash
   cd electron-app
   npm install
   ```
3. Run the application:
   ```bash
   npm start
   ```

## ⚙️ Configuration
Access the **Web Admin Panel** by navigating to the device's IP address in your browser.
- **Layout Editor:** Add, remove, and arrange tiles for Home, Weather, and Game tabs.
- **HA Bridge:** Map Home Assistant Entity IDs to specific tiles.
- **System:** Manage WiFi and MQTT settings.

## 📄 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
