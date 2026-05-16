# 🖥️ Smart Desk Assistant (Eye Health & Focus Edition)

![Project Status](https://img.shields.io/badge/Status-Active-brightgreen)
![Platform](https://img.shields.io/badge/Platform-ESP32-blue)
![Connectivity](https://img.shields.io/badge/Connectivity-MQTT%20%7C%20Wi--Fi-orange)

Welcome to the **Smart Desk Assistant** project! This IoT-powered desk companion is designed to improve your workspace ergonomics, protect your eye health, and boost your productivity using the Pomodoro technique.

---

## 🎯 Project Overview

The Smart Desk Assistant monitors your environment and behavior in real-time. It ensures you maintain a safe distance from your screen to protect your eyes, automatically adjusts lighting to reduce eye strain, and manages your work/break cycles to keep you focused and refreshed.

### ✨ Key Features

1. **🌐 Unified Edge Gateway & Multi-Cloud**
   - Uses **Node-RED** as a Unified Edge Gateway to manage flows and prevent race conditions between platforms.
   - Synchronizes perfectly with **Blynk** (Mobile) and **ThingsBoard** (Web/Time Series Database for trend analysis).
2. **👁️ Eye Health Monitoring (Posture & Distance)**
   - Uses an Ultrasonic Sensor to measure your distance from the screen.
   - Alerts you via RGB LED and Buzzer if you are too close (default: < 40 cm) for an extended period.
3. **⏳ Edge ML & Pomodoro State Machine**
   - Built-in countdown timer displayed on a 4-Digit TM1637 Display.
   - Automatically switches between **Work Mode** (e.g., 30 mins) and **Break Mode** (5 mins).
   - **Dynamic ML Timer:** Evaluates behavior (score and average lux) at the end of a session to automatically adjust the next work timer using Edge Logic.
4. **💡 Smart Adaptive Lighting**
   - Monitors ambient light using an LDR (Photoresistor) and automatically turns on the desk lamp when dark.
   - Supports **Manual Override** to control the lamp remotely.
5. **🌡️ Environment Monitoring**
   - Tracks Temperature and Humidity using a DHT22 sensor to ensure optimal room conditions.
6. **🔔 Smart Notifications & Local Control**
   - **Local:** RGB LED (Red = danger, Blue = break mode), Buzzer warnings, and a Node-RED Local Dashboard that works without internet.
   - **Remote:** Alerts via **Telegram Bot** if the sitting behavior becomes critical (score < 50).

---

## 🗄️ Time Series Database (ThingsBoard)

This project utilizes a Time Series Database (InfluxDB via ThingsBoard) to capture continuous telemetry data from the ESP32. Every 5 seconds, data points such as sitting distance, ambient light levels, temperature, and current behavior scores are recorded. This allows users to view historical trends and analyze their long-term sitting habits through dynamic web graphs.

---

## 🧠 Edge ML/AI Logic

Instead of relying solely on Cloud ML, this system implements **Edge Intelligence** directly on the Node-RED gateway. At the end of every work session, Node-RED evaluates the user's behavior based on the `session_end` event (analyzing the final score and average lux). Based on this localized assessment, it automatically calculates and assigns a new dynamic timer (e.g., shorter work periods for lower scores) for the next Pomodoro cycle.

---

## 📱 Smart Notifications

The system employs a multi-tier notification strategy:
- **Local Notification (On-site):** Immediate physical feedback at the desk via the Buzzer (short/long beeps depending on urgency) and RGB LED color changes based on risk levels (Red = danger, Blue = eye-rest mode).
- **Remote Notification (Cloud):** Instant alerts sent remotely via **Telegram Bot API**. If a user's sitting posture becomes critically bad (score falls below 50), a warning message is immediately pushed to their smartphone.

---

## 📊 Behavior Scoring Logic

The system starts with a base score of 100. It continuously monitors your habits and applies penalties for risky behaviors:
- **Bad Posture (Screen Proximity):** If the Ultrasonic sensor detects a distance less than the configured threshold (e.g., < 40 cm) for more than 5 consecutive seconds, it deducts `-5 points` and triggers a short buzzer warning.
- **Low Light Environment:** If the LDR detects ambient lighting below the standard healthy threshold while working, it deducts `-2 points/minute`.
- **Overworking / Skipping Breaks:** Bypassing or ignoring the Pomodoro break mode accumulates penalty points over time.

---

## 🛡️ System Reliability & Fault Tolerance

- **Auto-Reconnect & Watchdog:** The ESP32 is programmed with a non-blocking reconnect logic for both Wi-Fi and MQTT. If a connection drops, it will seamlessly attempt to restore it without freezing the main loop.
- **Offline Mode Operation:** In the event of an external internet outage (Cloud unreachable), the system continues to operate flawlessly via the Local Network (Node-RED). The standalone ESP32 will still track time, deduct points, and alert the user locally.
- **MQTT QoS 1:** Critical alerts (like `critical_penalty`) are published with QoS 1 to guarantee delivery to the Node-RED Edge Gateway.

---

## 🚀 Use Case Scenario

1. **Starting the Day:** You sit at your desk. The ESP32 initiates the Pomodoro timer (e.g., 45 mins). The TM1637 display starts the countdown.
2. **During Work:** You unconsciously lean too close to the screen (distance < 30 cm). The RGB LED shifts from Green to Yellow, then Red, followed by a short buzzer beep. Your health score drops to 85.
3. **Session Complete:** The timer hits zero. The RGB LED turns Blue, indicating the 5-minute Break Mode. Work data is published to Node-RED.
4. **Edge Intelligence (Dynamic Timer):** Node-RED analyzes the completed session (low average score, poor lighting) and automatically adjusts your next work session to 30 minutes with a 10-minute break to encourage better rest and recovery.

---

## 🛠️ Hardware Requirements

- **Microcontroller:** ESP32 (or ESP8266)
- **Sensors:**
  - Ultrasonic Sensor (HC-SR04)
  - Temperature & Humidity Sensor (DHT22)
  - Light Sensor (LDR)
- **Outputs:**
  - 4-Digit 7-Segment Display (TM1637)
  - RGB LED (Red, Green, Blue)
  - Buzzer
- **Other:** Jumper wires, Breadboard, Resistors.

---

## 🔌 Pin Configuration (ESP32)

| Component | Pin (ESP32) |
| :--- | :--- |
| Ultrasonic (SIG/TRIG) | GPIO 5 |
| DHT22 (DATA) | GPIO 4 |
| LDR (Analog) | GPIO 34 |
| TM1637 (CLK) | GPIO 22 |
| TM1637 (DIO) | GPIO 23 |
| RGB LED (Red) | GPIO 19 |
| RGB LED (Green) | GPIO 21 |
| RGB LED (Blue) | GPIO 13 |
| Buzzer | GPIO 12 |

---

## 📡 MQTT Communication Protocol

The device communicates with the broker at `<YOUR_BROKER_IP>:1883` on the following topics:

### 📥 Subscribed Topics (Commands to ESP32)
- Topic: `smartdesk/control`
- Payloads:
  - `SET_TIMER:<seconds>` - Updates work session duration.
  - `CMD:MUTE:<1/0>` - Mutes/Unmutes the buzzer.
  - `CMD:LAMP:<1/0>` - Manually turns the smart lamp ON/OFF.
  - `CMD:RESET_SCORE` - Resets health score to 100.
  - `CMD:RESET_TIMER` - Forces the timer back to Work Mode.
  - `SET_DIST:<cm>` - Adjusts the danger distance threshold.

### 📤 Published Topics (Telemetry to Cloud)
- Environment data (Temp, Humidity).
- Distance data and Eye Health Score.
- Timer status (Work/Break mode, Remaining time).
- Lamp status (Auto/Manual).

---

## 🚀 Getting Started

1. **Clone the repository:**
   ```bash
   git clone https://github.com/dekjefff/IoT-Smart-Desk-Assistance.git
   ```
2. **Open the project in Arduino IDE:**
   - Navigate to `src/smart-desk-assistant/`
   - Open `smart-desk-assistant.ino`.
3. **Install Required Libraries:**
   - `DHT sensor library` by Adafruit
   - `TM1637` by Avishay Orpaz
   - `PubSubClient` by Nick O'Leary
4. **Update Wi-Fi & MQTT Settings:**
   - Change `ssid` and `password` to your local network.
   - Update `mqtt_server` to your broker's IP address.
5. **Upload the code** to your ESP32 board.

---

## 📊 Dashboard Setup (Node-RED, ThingsBoard & Blynk)

This project supports multiple platforms for monitoring and control:

### Node-RED
1. Ensure your MQTT Broker (e.g., Mosquitto) is running.
2. Open your Node-RED workspace.
3. Go to **Menu** > **Import**.
4. Upload the `node-red-flow.json` file provided in this repository.
5. Update the MQTT Broker nodes to match your IP address and deploy the flow.

### ThingsBoard
1. Create a Device in your ThingsBoard instance and copy its **Access Token**.
2. Go to the **Dashboards** section and click **+ Add new dashboard** > **Import dashboard**.
3. Upload the `thingsboard-dashboard.json` file.
4. Update any device aliases in the dashboard to point to your newly created device.

### Blynk
*(Integration handled via Node-RED MQTT to Blynk translation)*

#### 1. Datastream Configuration (Virtual Pins)
Set up the following Datastreams in your **Blynk Web Console** (Templates -> Datastreams) to match the Node-RED configuration:

| Virtual Pin | Name | Data Type | Role (From Cloud Perspective) |
| :--- | :--- | :--- | :--- |
| **V0** | Reset Score | Integer | Command (Input) |
| **V1** | Reset Timer | Integer | Command (Input) |
| **V2** | Mute | Integer (0/1) | Command (Input) |
| **V4** | Timer Duration | Integer | Command (Input) |
| **V5** | Control Lamp | Integer (0/1) | Command (Input) |
| **V6** | Distance Setup | Integer | Command (Input) |
| **V10** | Eye Health Score | Integer | Telemetry (Output) |
| **V11** | Time Remaining | Integer | Telemetry (Output) |

#### 2. How to Create a Dashboard Widget (Example: Eye Health Score)
Once your Datastreams are configured, you can build your dashboard:
1. Go to your **Blynk Web Console** and open your Template's **Web Dashboard** tab.
2. Drag and drop a **Gauge** widget from the widget box onto the canvas.
3. Hover over the newly placed widget and click the **Gear Icon (Settings)**.
4. Set the **Title** to `Eye Health Score`.
5. Under **Datastream**, select `Eye Health Score (V10)`.
6. Set the Min/Max values (e.g., `0` to `100`).
7. Click **Save** in the widget settings, then click **Save And Apply** at the top right of the template editor.
8. Update the Node-RED Blynk nodes with your new `BLYNK_AUTH_TOKEN` to see the live score!

---

## 📁 Repository Structure

- `src/smart-desk-assistant/` - Main Arduino/ESP32 Source Code.
- `node-red-flow.json` - Node-RED flow configuration.
- `thingsboard-dashboard.json` - ThingsBoard dashboard configuration.
- `fritzing-sketch.fzz` - Fritzing circuit schematic.
- `fritzing-sketch.png` - Circuit wiring diagram.

---

*Stay Focused. Stay Healthy.* 🚀