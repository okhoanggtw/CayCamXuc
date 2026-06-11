#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>

// ================= CONFIGURATION =================
const char* ssid     = "111";
const char* password = "12345678";

const char* tb_host  = "mqtt.thingsboard.cloud";
const int   tb_port  = 1883;
const char* tb_token = "MzTCWYZkbx0YpmQuLTrs";

// ================= HARDWARE PIN MAP =================
#define DHTPIN          16
#define DHTTYPE         DHT11
#define LIGHT_PIN       35
#define SOIL_PIN        34
#define SOIL_POWER_PIN  25
#define RELAY_BOM       27
// *** GHI CHÚ RELAY ***
// Relay đã được chuyển sang active-LOW (kích mức THẤP) để tương thích 5V:
//   digitalWrite(RELAY_BOM, LOW)   --> BẬT bơm
//   digitalWrite(RELAY_BOM, HIGH)  --> TẮT bơm

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqtt(espClient);

// ================= SYSTEM DATA VARIABLES =================
float nhietDo = 0;
float doAm = 0;
int soilPct = 0;
int lightPct = 0;
int soilPctPrev    = 0;     // ← lưu giá trị soilPct chu kỳ trước
bool soilIncreasing = false; // ← cờ: độ ẩm đất đang tăng (bơm đang có tác dụng)
bool heThongBat   = true;
bool bomDangChay = false;
bool bomThuCong  = false;
unsigned long bomBatLuc = 0;
const unsigned long BOM_THOI_GIAN = 3000; // ← 3 giây (đổi từ 5000)

// ================= TIMING MANAGER =================
unsigned long lastOLED   = 0;
unsigned long lastSensor = 0;
unsigned long lastMQTT   = 0;
unsigned long lastDHT    = 0;

// ================= SOIL STATE MACHINE =================
enum SoilState { SOIL_IDLE, SOIL_POWER_ON, SOIL_SAMPLING, SOIL_DONE };
SoilState soilState     = SOIL_IDLE;
unsigned long soilTimer = 0;
int soilSampleCount     = 0;
long soilTotal          = 0;

// ================= MQTT QUEUE =================
#define QUEUE_SIZE 8
struct MqttMsg { char topic[64]; char payload[256]; };
MqttMsg mqttQueue[QUEUE_SIZE];
int qHead = 0, qTail = 0;

void mqttEnqueue(const char* topic, const char* payload) {
  int next = (qTail + 1) % QUEUE_SIZE;
  if (next == qHead) return;
  strncpy(mqttQueue[qTail].topic,   topic,   63);
  strncpy(mqttQueue[qTail].payload, payload, 255);
  qTail = next;
}

void mqttFlush() {
  if (qHead == qTail) return;
  if (!mqtt.connected()) return;
  mqtt.publish(mqttQueue[qHead].topic, mqttQueue[qHead].payload);
  qHead = (qHead + 1) % QUEUE_SIZE;
}

// ================= FreeRTOS MUTEX =================
SemaphoreHandle_t i2cMutex;

// ================= EMOTION NAME =================
String layTenCamXuc(int cx) {
  switch(cx) {
    case 0: return "TO VUI QUA!";
    case 1: return "TO DANG ON";
    case 2: return "TO NONG QUA!";
    case 3: return "TO KHAT NUOC!";
    case 4: return "TO BI UNG!";
    case 5: return "TO BUON NGU...";
    case 6: return "TO CHOI MAT!";
    case 7: return "DANG TUOI CAY!";
    default: return "TO DANG ON";
  }
}

// ================= CLASSIFICATION LOGIC =================
int xepLoaiTong(float t, float h, int soil, int light) {
  // Chỉ hiện "ĐANG TƯỚI" khi bơm chạy VÀ độ ẩm đất đang thực sự tăng
  if (bomDangChay && soilIncreasing) return 7;
  if (soil >= 80) return 4;
  if (soil < 1)   return 3; // ← ngưỡng khát: < 1%
  if (t > 40)     return 2;
  if (light > 90) return 6;
  if (light < 10) return 5;
  if (t >= 24 && t <= 32 &&
      h >= 45 && h <= 75 &&
      soil >= 20 && soil <= 75 &&
      light >= 35 && light <= 80) return 0;
  return 1;
}

// ================= OLED DRAW =================
void oledDraw(int camXuc) {
  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;

  u8g2.clearBuffer();

  int eyeY   = 26 + (int)(sin(millis() / 250.0) * 2);
  int mouthY = 47 + (int)(sin(millis() / 250.0) * 2);
  int talk   = (millis() / 180) % 4;

  u8g2.drawRFrame(0, 0, 128, 64, 6);

  if (camXuc == 99) {
    u8g2.drawCircle(64, 28, 14);
    u8g2.drawLine(64, 14, 64, 22);
    u8g2.setDrawColor(0);
    u8g2.drawBox(54, 22, 20, 10);
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(22, 52, "HE THONG TAT");
    u8g2.sendBuffer();
    xSemaphoreGive(i2cMutex);
    return;
  }
  else if (camXuc == 7) {
    u8g2.drawLine(34, eyeY,   42, eyeY-5);
    u8g2.drawLine(42, eyeY-5, 50, eyeY);
    u8g2.drawLine(78, eyeY,   86, eyeY-5);
    u8g2.drawLine(86, eyeY-5, 94, eyeY);
    u8g2.drawDisc(20, 36, 4);
    u8g2.drawDisc(108, 36, 4);
    u8g2.setDrawColor(0);
    u8g2.drawDisc(20, 36, 2);
    u8g2.drawDisc(108, 36, 2);
    u8g2.setDrawColor(1);
    u8g2.drawEllipse(64, mouthY, 14, 7);
    u8g2.drawLine(50, mouthY, 78, mouthY);
    int d1 = (millis() / 80)      % 50;
    int d2 = (millis() / 70)      % 50;
    int d3 = (millis() / 90)      % 50;
    int d4 = (millis() / 75 + 25) % 50;
    u8g2.drawLine(20,  d1, 20,  d1+5);
    u8g2.drawLine(50,  d2, 50,  d2+5);
    u8g2.drawLine(78,  d3, 78,  d3+5);
    u8g2.drawLine(108, d4, 108, d4+5);
  }
  else if (camXuc == 0) {
    u8g2.drawDisc(42, eyeY, 9);
    u8g2.drawDisc(86, eyeY, 9);
    u8g2.setDrawColor(0);
    u8g2.drawDisc(39, eyeY-2, 2);
    u8g2.drawDisc(83, eyeY-2, 2);
    u8g2.setDrawColor(1);
    u8g2.drawCircle(25, 38, 4);
    u8g2.drawCircle(103, 38, 4);
    if (talk == 0)      u8g2.drawEllipse(64, mouthY, 12, 6);
    else if (talk == 1) u8g2.drawDisc(64, mouthY, 8);
    else                u8g2.drawEllipse(64, mouthY, 10, 4);
  }
  else if (camXuc == 1) {
    bool blink = ((millis()/2200) % 2);
    if (blink) {
      u8g2.drawDisc(42, eyeY, 7);
      u8g2.drawDisc(86, eyeY, 7);
    } else {
      u8g2.drawLine(34, eyeY, 50, eyeY);
      u8g2.drawLine(78, eyeY, 94, eyeY);
    }
    if (talk % 2 == 0) u8g2.drawLine(52, mouthY, 76, mouthY);
    else               u8g2.drawEllipse(64, mouthY, 8, 3);
  }
  else if (camXuc == 2) {
    u8g2.drawEllipse(42, eyeY, 10, 7);
    u8g2.drawEllipse(86, eyeY, 10, 7);
    u8g2.drawDisc(42, eyeY, 4);
    u8g2.drawDisc(86, eyeY, 4);
    int sweat = (millis()/100) % 12;
    u8g2.drawDisc(106, 18+sweat, 3);
    if (talk == 0) u8g2.drawCircle(64, mouthY, 7);
    else           u8g2.drawEllipse(64, mouthY, 10, 6);
  }
  else if (camXuc == 3) {
    u8g2.drawEllipse(42, eyeY, 12, 10);
    u8g2.drawEllipse(86, eyeY, 12, 10);
    u8g2.drawDisc(42, eyeY+1, 5);
    u8g2.drawDisc(86, eyeY+1, 5);
    u8g2.setDrawColor(0);
    u8g2.drawDisc(39, eyeY-2, 2);
    u8g2.drawDisc(83, eyeY-2, 2);
    u8g2.setDrawColor(1);
    int tear = (millis()/90) % 12;
    u8g2.drawLine(42, eyeY+10, 42, eyeY+15+tear);
    u8g2.drawLine(86, eyeY+10, 86, eyeY+15+tear);
    if (talk % 2 == 0) u8g2.drawEllipse(64, mouthY+4, 11, 5);
    else               u8g2.drawEllipse(64, mouthY+5, 8, 4);
  }
  else if (camXuc == 4) {
    int wave = (int)(sin(millis()/120.0) * 2);
    u8g2.drawCircle(42, eyeY, 10);
    u8g2.drawCircle(86, eyeY, 10);
    u8g2.drawCircle(42, eyeY, 5+wave);
    u8g2.drawCircle(86, eyeY, 5+wave);
    u8g2.drawLine(52, mouthY+4, 76, mouthY);
    int bubble = (millis()/150) % 20;
    u8g2.drawCircle(100, 40-bubble, 4);
    u8g2.drawCircle(108, 24-bubble, 2);
    u8g2.drawDisc(18, 20, 3);
    u8g2.drawDisc(112, 18, 2);
  }
  else if (camXuc == 5) {
    u8g2.drawLine(32, eyeY, 52, eyeY);
    u8g2.drawLine(76, eyeY, 96, eyeY);
    u8g2.drawCircle(64, mouthY, 2);
    int bubble = 4 + (millis()/250) % 6;
    u8g2.drawCircle(90, 40, bubble);
    if ((millis()/500) % 2) {
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.drawStr(100, 20, "Z");
      u8g2.drawStr(110, 12, "Z");
    }
  }
  else if (camXuc == 6) {
    int twitch = (millis()/100) % 4;
    u8g2.drawLine(32, eyeY-6+twitch, 52, eyeY+6);
    u8g2.drawLine(32, eyeY+6, 52, eyeY-6+twitch);
    u8g2.drawLine(76, eyeY-6+twitch, 96, eyeY+6);
    u8g2.drawLine(76, eyeY+6, 96, eyeY-6+twitch);
    if (talk == 0) u8g2.drawBox(52, mouthY, 24, 4);
    else           u8g2.drawBox(56, mouthY, 16, 3);
    u8g2.drawLine(10, 8, 24, 20);
    u8g2.drawLine(118, 8, 104, 20);
  }

  u8g2.setFont(u8g2_font_6x10_tr);
  String txt = layTenCamXuc(camXuc);
  int w = u8g2.getStrWidth(txt.c_str());
  u8g2.drawStr((128-w)/2, 60, txt.c_str());
  u8g2.sendBuffer();
  xSemaphoreGive(i2cMutex);
}

// ================= RPC CALLBACK =================
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();

  bool isTrue = msg.indexOf("\"value\":true")  >= 0 ||
                msg.indexOf("\"value\": true") >= 0 ||
                msg.indexOf("params\":true")   >= 0 ||
                msg.indexOf("params\": true")  >= 0;

  String resTopic = String(topic);
  resTopic.replace("request", "response");

  if (msg.indexOf("setHeThong") >= 0) {
    heThongBat = isTrue;
    if (!heThongBat) {
      bomDangChay = false;
      bomThuCong  = false;
      soilState   = SOIL_IDLE;
      digitalWrite(RELAY_BOM, LOW);
      digitalWrite(SOIL_POWER_PIN, LOW);
      mqttEnqueue(resTopic.c_str(), "{\"value\":false}");
      mqttEnqueue("v1/devices/me/telemetry", "{\"he_thong\":false,\"bom\":false}");
    } else {
      mqttEnqueue(resTopic.c_str(), "{\"value\":true}");
      mqttEnqueue("v1/devices/me/telemetry", "{\"he_thong\":true}");
    }
  }

  if (msg.indexOf("setBom") >= 0) {
    if (isTrue) {
      bomDangChay = true;
      bomThuCong  = true;
      bomBatLuc   = millis();
      digitalWrite(RELAY_BOM, LOW);
      mqttEnqueue(resTopic.c_str(), "{\"value\":true}");
      mqttEnqueue("v1/devices/me/telemetry", "{\"bom\":true}");
    } else {
      bomDangChay = false;
      bomThuCong  = false;
      digitalWrite(RELAY_BOM, HIGH);
      mqttEnqueue(resTopic.c_str(), "{\"value\":false}");
      mqttEnqueue("v1/devices/me/telemetry", "{\"bom\":false}");
    }
  }
}

// ================= WIFI CONNECT =================
void ketNoiWifi() {
  WiFi.begin(ssid, password);

  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    u8g2.clearBuffer();
    u8g2.drawRFrame(0, 0, 128, 64, 6);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(10, 25, "Connecting WiFi...");
    u8g2.drawStr(20, 45, ssid);
    u8g2.sendBuffer();
    xSemaphoreGive(i2cMutex);
  }

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK!");
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      u8g2.clearBuffer();
      u8g2.drawRFrame(0, 0, 128, 64, 6);
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.drawStr(25, 25, "WiFi OK!");
      u8g2.drawStr(10, 45, WiFi.localIP().toString().c_str());
      u8g2.sendBuffer();
      xSemaphoreGive(i2cMutex);
    }
    delay(1500);
  } else {
    Serial.println("\nWiFi FAILED!");
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      u8g2.clearBuffer();
      u8g2.drawRFrame(0, 0, 128, 64, 6);
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.drawStr(15, 20, "WiFi FAILED!");
      u8g2.drawStr(5,  35, "Kiem tra lai:");
      u8g2.drawStr(5,  50, ssid);
      u8g2.sendBuffer();
      xSemaphoreGive(i2cMutex);
    }
    delay(3000);
    ESP.restart();
  }
}

// ================= THINGSBOARD CONNECT =================
void ketNoiTB() {
  mqtt.setCallback(callback);
  mqtt.setServer(tb_host, tb_port);

  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    u8g2.clearBuffer();
    u8g2.drawRFrame(0, 0, 128, 64, 6);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(10, 30, "Connecting TB...");
    u8g2.sendBuffer();
    xSemaphoreGive(i2cMutex);
  }

  int timeout = 0;
  while (!mqtt.connected() && timeout < 5) {
    String cid = "ESP32-" + String(random(1000, 9999));
    if (mqtt.connect(cid.c_str(), tb_token, NULL)) {
      Serial.println("ThingsBoard OK!");
      mqtt.subscribe("v1/devices/me/rpc/request/+");
      if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        u8g2.clearBuffer();
        u8g2.drawRFrame(0, 0, 128, 64, 6);
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(20, 30, "TB OK!");
        u8g2.sendBuffer();
        xSemaphoreGive(i2cMutex);
      }
      delay(1000);
    } else {
      timeout++;
      delay(1000);
    }
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  i2cMutex = xSemaphoreCreateMutex();

  dht.begin();
  Wire.begin(21, 22);
  u8g2.begin();

  pinMode(SOIL_POWER_PIN, OUTPUT);
  digitalWrite(SOIL_POWER_PIN, LOW);

  pinMode(RELAY_BOM, OUTPUT);
  digitalWrite(RELAY_BOM, HIGH); // Tắt bơm khi vừa khởi động (Active LOW)

  mqtt.setBufferSize(512);
  ketNoiWifi();
  ketNoiTB();
}

// ================= MAIN LOOP =================
void loop() {
  unsigned long now = millis();

  // 1. DUY TRÌ KẾT NỐI MQTT
  if (!mqtt.connected()) ketNoiTB();
  mqtt.loop();
  mqttFlush();

  // 2. KHI HỆ THỐNG TẮT
  if (!heThongBat) {
    bomDangChay = false;
    bomThuCong  = false;
    digitalWrite(RELAY_BOM, LOW);
    if (now - lastOLED >= 200) {
      lastOLED = now;
      oledDraw(99);
    }
    return;
  }

  // 3. ĐỌC DHT11 (mỗi 2 giây)
  if (now - lastDHT >= 2000) {
    lastDHT = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) nhietDo = t;
    if (!isnan(h)) doAm    = h;
  }

  // 4. STATE MACHINE ĐỌC ĐỘ ẨM ĐẤT
  switch (soilState) {
    case SOIL_IDLE:
      if (now - lastSensor >= 1000) {
        soilState       = SOIL_POWER_ON;
        soilTimer       = now;
        soilSampleCount = 0;
        soilTotal       = 0;
        digitalWrite(SOIL_POWER_PIN, HIGH);
      }
      break;

    case SOIL_POWER_ON:
      if (now - soilTimer >= 10) {
        soilState = SOIL_SAMPLING;
        soilTimer = now;
      }
      break;

    case SOIL_SAMPLING:
      if (now - soilTimer >= 2) {
        soilTimer = now;
        soilTotal += analogRead(SOIL_PIN);
        soilSampleCount++;
        if (soilSampleCount >= 15) soilState = SOIL_DONE;
      }
      break;

    case SOIL_DONE: {
      digitalWrite(SOIL_POWER_PIN, LOW);

      int newSoil  = constrain(map(soilTotal / 15, 0, 4095, 0, 100), 0, 100);
      soilIncreasing = (newSoil > soilPctPrev); // ← so sánh với chu kỳ trước
      soilPctPrev    = soilPct;                  // ← lưu lại trước khi cập nhật
      soilPct        = newSoil;

      lightPct = constrain(map(analogRead(LIGHT_PIN), 4095, 0, 0, 100), 0, 100);
      lastSensor = now;
      soilState  = SOIL_IDLE;

      // BẬT BƠM TỰ ĐỘNG khi đất khô (ngưỡng < 1%)
      if (soilPct < 1 && !bomDangChay) {
        bomDangChay = true;
        bomThuCong  = false;
        bomBatLuc   = now;
        digitalWrite(RELAY_BOM, LOW);
        Serial.println(">>> BOM: BAT (tu dong)");
        mqttEnqueue("v1/devices/me/telemetry", "{\"bom\":true}");
      }

      // TẮT BƠM TỰ ĐỘNG khi đủ nước (>= 10%) hoặc hết 3 giây
      if (bomDangChay && !bomThuCong &&
          (now - bomBatLuc >= BOM_THOI_GIAN || soilPct >= 10)) {
        bomDangChay    = false;
        soilIncreasing = false; // reset cờ khi bơm tắt
        digitalWrite(RELAY_BOM, HIGH);
        Serial.println(">>> BOM: TAT");
        mqttEnqueue("v1/devices/me/telemetry", "{\"bom\":false}");
      }

      // GỬI DỮ LIỆU THINGSBOARD (mỗi 1 giây)
      if (now - lastMQTT >= 1000) {
        lastMQTT = now;

        int cxIndex = xepLoaiTong(nhietDo, doAm, soilPct, lightPct);
        String tenCX = layTenCamXuc(cxIndex);

        String pl = "{";
        pl += "\"nhietdo\":"   + String(nhietDo, 1) + ",";
        pl += "\"doam\":"      + String(doAm, 0)    + ",";
        pl += "\"dat\":"       + String(soilPct)    + ",";
        pl += "\"sang\":"      + String(lightPct)   + ",";
        pl += "\"camxuc\":\"" + tenCX               + "\",";
        pl += "\"bom\":"       + String(bomDangChay ? "true" : "false") + ",";
        pl += "\"he_thong\":true}";

        mqttEnqueue("v1/devices/me/telemetry", pl.c_str());

        Serial.println("========================");
        Serial.printf("Nhiet do    : %.1f\n", nhietDo);
        Serial.printf("Do am KK    : %.0f\n", doAm);
        Serial.printf("Do am dat   : %d\n",   soilPct);
        Serial.printf("Anh sang    : %d\n",   lightPct);
        Serial.printf("Cam xuc     : %s\n",   tenCX.c_str());
        Serial.printf("Bom         : %s\n",   bomDangChay ? "BAT" : "TAT");
        Serial.printf("Dat tang    : %s\n",   soilIncreasing ? "CO" : "KHONG");
      }
      break;
    }
  }

  // 5. RENDER OLED
  unsigned long oledInterval = bomDangChay ? 150 : 200;
  if (now - lastOLED >= oledInterval) {
    lastOLED = now;
    int cxT = xepLoaiTong(nhietDo, doAm, soilPct, lightPct);
    oledDraw(cxT);
  }
}