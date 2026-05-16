/*
* Smart Desk Assistant (Phase 4 Final: Multi-Cloud, Control & Break Mode)
* - Two-way MQTT (Commands: MUTE, LAMP, RESET, SET_DIST, SET_TIMER)
* - Manual Override Logic
* - Pomodoro Break Mode (5 mins break after session ends)
*/

#include "DHT.h"
#include <TM1637Display.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ---------------- ตั้งค่าเครือข่าย & API ----------------
const char* ssid = "Lemon";
const char* password = "0963014161";
const char* mqtt_server = "192.168.100.17";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ---------------- Pin Definitions ----------------
#define SIG_PIN 5 
#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);
#define LDR_PIN 34
#define CLK_PIN 22
#define DIO_PIN 23
TM1637Display display(CLK_PIN, DIO_PIN);
#define RED_PIN 19
#define GREEN_PIN 21
#define BLUE_PIN 13
#define BUZZER_PIN 12

// ---------------- Variables (อัปเดต Phase 4) ----------------
int countdownSeconds = 1800; // เวลาทำงานปัจจุบัน
int workSessionDuration = 1800;  // 🆕 เก็บเวลาทำงานตั้งต้นที่ User/ML กำหนด (ค่าเริ่มต้น 30 นาที)
bool isBreakMode = false;        // 🆕 เช็คว่าตอนนี้อยู่ในเวลาพัก 5 นาทีหรือไม่
const int BREAK_DURATION = 300;  // 🆕 เวลาพัก 5 นาที (300 วินาที)

unsigned long lastTimerUpdate = 0;

int dangerDistance = 40; 
bool isTooClose = false;
unsigned long tooCloseStartTime = 0;

int current_score = 100;
bool criticalAlertSent = false;

unsigned long totalLightSum = 0;
int lightReadCount = 0;

unsigned long lastMqttSend = 0;
const unsigned long MQTT_INTERVAL = 5000; 
unsigned long lastWiFiCheck = 0;
unsigned long lastMqttReconnect = 0;
unsigned long lastSerialPrint = 0;

// ตัวแปรสำหรับ Control & Override
bool isMuted = false;
bool isLampOn = false;
bool isManualOverride = false; 

// ---------------- Functions ----------------

// 📥 ฟังก์ชันรับคำสั่งจาก Node-RED/Blynk/Thingsboard
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) { message += (char)payload[i]; }
  
  Serial.print("[MQTT RECEIVE] Topic: "); Serial.print(topic);
  Serial.print(" | Payload: "); Serial.println(message);

  // 1. คำสั่งปรับเวลาจาก ML (ถ้า User ไม่ได้เปิด Manual Override)
  if (message.startsWith("SET_TIMER:") && !isManualOverride) {
    int newTime = message.substring(10).toInt();
    if (newTime > 0) {
      workSessionDuration = newTime; // จำค่าที่ ML ส่งมาไว้ใช้เป็นเวลาทำงานรอบใหม่
      if (!isBreakMode) countdownSeconds = newTime; // ถ้ากำลังทำงานอยู่ให้อัปเดตเวลาเลย
    }
    Serial.print("[CMD] อัปเดตเวลาทำงานตั้งต้นเป็น: "); Serial.println(workSessionDuration);
  }
  
  // 2. คำสั่ง Mute เสียง
  else if (message.startsWith("CMD:MUTE:")) {
    isMuted = (message.substring(9).toInt() == 1);
    Serial.print("[CMD] Mute Status: "); Serial.println(isMuted);
  }

  // 3. คำสั่งเปิดไฟแมนนวล
  else if (message.startsWith("CMD:LAMP:")) {
    isLampOn = (message.substring(9).toInt() == 1);
    isManualOverride = isLampOn; // เข้าสู่โหมด Manual Override
    Serial.print("[CMD] Lamp Manual Override: "); Serial.println(isLampOn);
  }

  // 4. คำสั่ง Reset ค่าต่างๆ
  else if (message == "CMD:RESET_SCORE") {
    current_score = 100;
    Serial.println("[CMD] Reset Score = 100");
  }
  else if (message == "CMD:RESET_TIMER") {
    isBreakMode = false; // บังคับกลับสู่โหมดทำงาน
    countdownSeconds = workSessionDuration; // กลับไปที่เวลาที่ ML เคยตั้งไว้ล่าสุด
    Serial.println("[CMD] Reset Timer กลับไปเริ่มทำงานใหม่");
  }

  // 5. คำสั่งปรับ Threshold ระยะทาง
  else if (message.startsWith("SET_DIST:")) {
    int newDist = message.substring(9).toInt();
    if (newDist > 0 && newDist <= 100) dangerDistance = newDist;
    Serial.print("[CMD] เปลี่ยนระยะเตือนภัยเป็น: "); Serial.println(dangerDistance);
  }
}

void setup_wifi() {
  delay(10); WiFi.begin(ssid, password); lastWiFiCheck = millis();
}

void reconnectMQTT() {
  if (mqttClient.connect("ESP32_SmartDesk")) {
    mqttClient.subscribe("smartdesk/control"); 
  }
}

long readUltrasonic() {
  long readings[3]; int validCount = 0;
  for(int i=0; i<3; i++) {
    pinMode(SIG_PIN, OUTPUT); digitalWrite(SIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(SIG_PIN, HIGH); delayMicroseconds(5); digitalWrite(SIG_PIN, LOW);
    pinMode(SIG_PIN, INPUT);
    long duration = pulseIn(SIG_PIN, HIGH, 30000); 
    if (duration > 0) {
      long d = (duration / 2) / 29.1;
      if(d > 5 && d < 300) { readings[validCount] = d; validCount++; }
    }
    delay(15);
  }
  if (validCount == 0) return 999; 
  if (validCount == 1) return readings[0]; 
  if (validCount == 2) return (readings[0] + readings[1]) / 2; 
  if (readings[0] > readings[1]) { long temp = readings[0]; readings[0] = readings[1]; readings[1] = temp; }
  if (readings[1] > readings[2]) { long temp = readings[1]; readings[1] = readings[2]; readings[2] = temp; }
  if (readings[0] > readings[1]) { long temp = readings[0]; readings[0] = readings[1]; readings[1] = temp; }
  return readings[1]; 
}

void displayWithPrefix(uint8_t letterHex, int value) {
  uint8_t data[4]; data[0] = letterHex;
  if (value > 999) value = 999; if (value < 0) value = 0;
  if (value >= 100) { data[1] = display.encodeDigit((value / 100) % 10); data[2] = display.encodeDigit((value / 10) % 10); data[3] = display.encodeDigit(value % 10); }
  else if (value >= 10) { data[1] = 0x00; data[2] = display.encodeDigit((value / 10) % 10); data[3] = display.encodeDigit(value % 10); }
  else { data[1] = 0x00; data[2] = 0x00; data[3] = display.encodeDigit(value % 10); }
  display.setSegments(data);
}

// 💡 อัปเดตไฟ RGB แสดงสถานะ (เพิ่มโหมดพัก)
void updateRGBStatus() {
  if (isManualOverride && isLampOn) {
    // โหมด Manual: ไฟขาวสว่างสุด
    digitalWrite(RED_PIN, HIGH); digitalWrite(GREEN_PIN, HIGH); digitalWrite(BLUE_PIN, HIGH);
  } else if (isBreakMode) {
    // 🆕 โหมดพัก: ไฟสีฟ้า (Cyan) ผ่อนคลาย
    digitalWrite(RED_PIN, HIGH); digitalWrite(GREEN_PIN, LOW); digitalWrite(BLUE_PIN, LOW); 
  } else {
    // โหมด Auto (ML): สีตามสุขภาพ
    if (current_score >= 80) { digitalWrite(RED_PIN, HIGH); digitalWrite(GREEN_PIN, LOW); digitalWrite(BLUE_PIN, HIGH); } 
    else if (current_score >= 50) { digitalWrite(RED_PIN, LOW); digitalWrite(GREEN_PIN, LOW); digitalWrite(BLUE_PIN, HIGH); } 
    else { digitalWrite(RED_PIN, LOW); digitalWrite(GREEN_PIN, HIGH); digitalWrite(BLUE_PIN, HIGH); }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RED_PIN, OUTPUT); pinMode(GREEN_PIN, OUTPUT); pinMode(BLUE_PIN, OUTPUT); pinMode(BUZZER_PIN, OUTPUT);
  dht.begin(); display.setBrightness(0x0f);
  setup_wifi(); mqttClient.setServer(mqtt_server, mqtt_port); mqttClient.setCallback(mqttCallback); 
  Serial.println("System Initialized (Phase 4 Final Ready).");
}

void loop() {
  unsigned long currentMillis = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (currentMillis - lastWiFiCheck >= 10000) { WiFi.reconnect(); lastWiFiCheck = currentMillis; }
  } else {
    if (!mqttClient.connected()) {
      if (currentMillis - lastMqttReconnect >= 5000) { lastMqttReconnect = currentMillis; reconnectMQTT(); }
    } else { mqttClient.loop(); }
  }

  long filteredDistance = readUltrasonic();
  int rawLDR = analogRead(LDR_PIN);
  float temp = dht.readTemperature();
  int readableLux = map(rawLDR, 0, 4095, 0, 1000); 

  // --- Core Timer & State Machine ---
  if (currentMillis - lastTimerUpdate >= 1000) {
    lastTimerUpdate = currentMillis;
    
    // เก็บค่าแสงเฉพาะตอนที่ "กำลังทำงาน" เท่านั้น
    if (!isBreakMode) { totalLightSum += readableLux; lightReadCount++; } 

    if (countdownSeconds > 0) {
      countdownSeconds--;
    } else {
      // เมื่อเวลานับถอยหลังหมด ให้เช็คว่ากำลังหมดเวลาของ "โหมดไหน"
      if (!isBreakMode) {
        // --- 1. จบเวลาทำงาน -> เริ่มเวลาพัก 5 นาที ---
        float avgLux = lightReadCount > 0 ? ((float)totalLightSum / lightReadCount) : 0;
        
        if(mqttClient.connected()) {
          String eventPayload = "{\"event\":\"session_end\", \"score\":" + String(current_score) + ", \"avg_lux\":" + String((int)avgLux) + "}";
          mqttClient.publish("smartdesk/events", eventPayload.c_str());
        }
        
        isBreakMode = true; // เข้าสู่โหมดพัก
        countdownSeconds = BREAK_DURATION; // ตั้งเวลาพัก 5 นาที (300 วิ)
        
        // เสียงแจ้งเตือนให้พัก (ตี๊ด-- ตี๊ด--)
        if (!isMuted) { tone(BUZZER_PIN, 1000, 500); delay(600); tone(BUZZER_PIN, 1500, 500); }
        Serial.println("====== BREAK TIME! 5 MINUTES ======");

      } else {
        // --- 2. จบเวลาพัก -> ดึงเวลาจาก ML มาเริ่มทำงานรอบใหม่ ---
        isBreakMode = false;
        
        countdownSeconds = workSessionDuration; // ดึงเวลาที่ ML คืนค่ากลับมารอไว้
        current_score = 100; // รีเซ็ตคะแนนเต็ม 100
        criticalAlertSent = false;
        totalLightSum = 0; lightReadCount = 0;

        // ส่ง Event กลับไปบอก Node-RED ว่าพักเสร็จแล้ว
        if(mqttClient.connected()) {
          mqttClient.publish("smartdesk/events", "{\"event\":\"break_end\"}");
        }

        // เสียงร้องเตือน 1 ครั้งยาวๆ (1.5 วินาที)
        if (!isMuted) { tone(BUZZER_PIN, 2000, 1500); }
        
        Serial.print("====== WORK TIME STARTED ======");
        Serial.print(" (Time from ML: "); Serial.print(countdownSeconds); Serial.println(" sec)");
      }
    }

    // --- ส่วนแสดงผล 7-Segment (วนลูปตามเวลา) ---
    int cycle = countdownSeconds % 9; 
    if (cycle >= 4) { int displayTime = ((countdownSeconds / 60) * 100) + (countdownSeconds % 60); display.showNumberDecEx(displayTime, 0b01000000, true); } 
    else if (cycle == 3) { displayWithPrefix(0x6d, current_score); } 
    else if (cycle == 2) { displayWithPrefix(0x5e, filteredDistance); } 
    else if (cycle == 1) { displayWithPrefix(0x38, readableLux); } 
    else if (cycle == 0) { displayWithPrefix(0x78, (int)temp); }
  }

  // --- Proximity Logic (แจ้งเตือนท่านั่ง) ---
  // 🆕 ให้หักคะแนนเฉพาะตอน "ไม่ได้พัก" เท่านั้น
  if (!isBreakMode && filteredDistance > 0 && filteredDistance < dangerDistance) { 
    if (!isTooClose) {
      isTooClose = true; tooCloseStartTime = currentMillis;
    } else {
      if (currentMillis - tooCloseStartTime >= 60000) { // ถ้านั่งใกล้เกิน 1 นาที
        
        if (!isMuted) { // ร้องเฉพาะตอนไม่ได้ปิดเสียง
          for(int i=0; i<3; i++) { tone(BUZZER_PIN, 1500); delay(100); noTone(BUZZER_PIN); delay(100); }
        }

        current_score -= 5; if(current_score < 0) current_score = 0;
        tooCloseStartTime = currentMillis; 
        
        // ส่งแจ้งเตือนเมื่อคะแนนต่ำกว่า 50
        if (current_score < 50 && !criticalAlertSent) {
          if(mqttClient.connected()) {
             String eventPayload = "{\"event\":\"critical_penalty\", \"score\":" + String(current_score) + "}";
             mqttClient.publish("smartdesk/events", eventPayload.c_str());
          }
          criticalAlertSent = true;
        }
      }
    }
  } else {
    // ถ้าระยะกลับมาเป็นปกติ หรือ เข้าสู่โหมดพัก ให้เคลียร์สถานะการนั่งใกล้
    if (isTooClose && (filteredDistance >= dangerDistance || isBreakMode)) { 
      isTooClose = false; 
    }
  }

  updateRGBStatus(); // อัปเดตไฟสีต่างๆ

  // --- ส่ง Telemetry ให้อัปเดต Dashboard ---
  if (currentMillis - lastMqttSend >= MQTT_INTERVAL) {
    lastMqttSend = currentMillis;
    if (mqttClient.connected()) {
      // 🆕 เพิ่มการส่ง break_mode ให้ Dashboard รู้ว่ากำลังพักอยู่ (Optional เอาไปโชว์ใน Node-RED ได้)
      String payload = "{\"distance\":" + String(filteredDistance) + 
                       ",\"light\":" + String(readableLux) + 
                       ",\"temp\":" + String(temp) + 
                       ",\"score\":" + String(current_score) + 
                       ",\"muted\":" + String(isMuted) + 
                       ",\"break_mode\":" + String(isBreakMode) + "}";
      mqttClient.publish("smartdesk/telemetry", payload.c_str());
    }
  }
}