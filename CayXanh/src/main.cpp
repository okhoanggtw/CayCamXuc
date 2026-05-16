#include <U8g2lib.h>
#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ========== CẤU HÌNH WiFi ==========
const char* ssid     = "TP-LINK_E41552";
const char* password = "";

// ========== CẤU HÌNH THINGSBOARD ==========
const char* tb_host  = "mqtt.thingsboard.cloud";
const int   tb_port  = 1883;
const char* tb_token = "MzTCWYZkbx0YpmQuLTrs";

// ========== PHẦN CỨNG ==========
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define DHTPIN         23
#define DHTTYPE        DHT11
#define SOIL_PIN       34
#define SOIL_POWER_PIN 25
#define LIGHT_PIN      35
#define LED_PIN        26

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqtt(espClient);

float nhietDo = 0, doAm = 0;
int soilPct = 0, lightPct = 0;
bool ledState = false;
bool autoMode = true;

// ========== BIỂU CẢM STYLE MARUKO ==========
void veMat(int camXuc) {
  int cx = 64, cy = 24, r = 22;

  // 1. Vẽ khuôn mặt tròn
  u8g2.drawCircle(cx, cy, r);

  // 2. Vẽ tóc mái răng cưa (Thương hiệu Maruko)
  int yToc = cy - 13;
  int yMui = cy - 7;
  u8g2.drawLine(cx-18, yMui, cx-14, yToc);
  u8g2.drawLine(cx-14, yToc, cx-10, yMui);
  u8g2.drawLine(cx-10, yMui, cx-6, yToc);
  u8g2.drawLine(cx-6, yToc, cx-2, yMui);
  u8g2.drawLine(cx-2, yMui, cx+2, yToc);
  u8g2.drawLine(cx+2, yToc, cx+6, yMui);
  u8g2.drawLine(cx+6, yMui, cx+10, yToc);
  u8g2.drawLine(cx+10, yToc, cx+14, yMui);
  u8g2.drawLine(cx+14, yMui, cx+18, yToc);

  // 3. Hai chấm má hồng mờ mờ 
  // (Lúc bị SỐC/Hoảng sẽ không có má hồng)
  if (camXuc != 4) { 
    u8g2.drawDisc(cx-12, cy+4, 2);
    u8g2.drawDisc(cx+12, cy+4, 2);
  }

  // 4. Các biểu cảm chi tiết
  if (camXuc == 0) {
    // 0: Tuyệt Vời! (Mắt híp ^ ^, cười tươi tắn)
    u8g2.drawLine(cx-8, cy-1, cx-5, cy-3); 
    u8g2.drawLine(cx-5, cy-3, cx-2, cy-1);
    u8g2.drawLine(cx+2, cy-1, cx+5, cy-3); 
    u8g2.drawLine(cx+5, cy-3, cx+8, cy-1);
    u8g2.drawCircle(cx, cy+4, 6);
    u8g2.setDrawColor(0); u8g2.drawBox(cx-7, cy-3, 14, 8); u8g2.setDrawColor(1);
  } 
  else if (camXuc == 1) {
    // 1: Ổn (Mắt chấm nhỏ xíu ngây ngô)
    u8g2.drawDisc(cx-5, cy-1, 1);
    u8g2.drawDisc(cx+5, cy-1, 1);
    u8g2.drawLine(cx-3, cy+6, cx+3, cy+6); 
  } 
  else if (camXuc == 2) {
    // 2: Nóng quá (Cau mày, thở dốc chữ O, có mồ hôi hột)
    u8g2.drawLine(cx-8, cy-3, cx-2, cy); 
    u8g2.drawLine(cx+8, cy-3, cx+2, cy);
    u8g2.drawDisc(cx-5, cy+1, 1);
    u8g2.drawDisc(cx+5, cy+1, 1);
    u8g2.drawCircle(cx, cy+7, 3); 
    // Mồ hôi
    u8g2.drawCircle(cx+16, cy-4, 2);
    u8g2.drawLine(cx+16, cy-8, cx+16, cy-6);
  } 
  else if (camXuc == 3) {
    // 3: Khát nước (Mếu máo khóc ròng T_T)
    u8g2.drawLine(cx-8, cy, cx-2, cy-3); 
    u8g2.drawLine(cx+8, cy, cx+2, cy-3);
    u8g2.drawLine(cx-7, cy, cx-3, cy); 
    u8g2.drawLine(cx+3, cy, cx+7, cy);
    u8g2.drawLine(cx-5, cy, cx-5, cy+6); // Lệ rơi
    u8g2.drawLine(cx+5, cy, cx+5, cy+6);
    u8g2.drawCircle(cx, cy+12, 5); 
    u8g2.setDrawColor(0); u8g2.drawBox(cx-6, cy+7, 12, 6); u8g2.setDrawColor(1);
  } 
  else if (camXuc == 4) {
    // 4: Ngộp nước (SỐC CẠN LỜI - Sọc dọc rơi trên mặt)
    u8g2.drawLine(cx-6, cy-6, cx-6, cy+2);
    u8g2.drawLine(cx, cy-6, cx, cy+4);
    u8g2.drawLine(cx+6, cy-6, cx+6, cy+2);
    // Mắt trống rỗng
    u8g2.drawCircle(cx-5, cy+6, 2);
    u8g2.drawCircle(cx+5, cy+6, 2);
    // Miệng há nhỏ
    u8g2.drawCircle(cx, cy+14, 2);
  } 
  else if (camXuc == 5) {
    // 5: Buồn ngủ (Mắt gạch ngang ngái ngủ, kèm bong bóng mũi)
    u8g2.drawLine(cx-8, cy, cx-2, cy); 
    u8g2.drawLine(cx+2, cy, cx+8, cy);
    u8g2.drawCircle(cx, cy+6, 2); 
    // Bong bóng mũi sụt sùi
    u8g2.drawCircle(cx+6, cy+3, 4); 
  } 
  else if (camXuc == 6) {
    // 6: Chói mắt (Nhắm tịt > < nhăn nhó)
    u8g2.drawLine(cx-8, cy-3, cx-4, cy);
    u8g2.drawLine(cx-4, cy, cx-8, cy+3);
    u8g2.drawLine(cx+8, cy-3, cx+4, cy);
    u8g2.drawLine(cx+4, cy, cx+8, cy+3);
    // Miệng hình răng cưa
    u8g2.drawLine(cx-4, cy+8, cx-2, cy+6);
    u8g2.drawLine(cx-2, cy+6, cx, cy+8);
    u8g2.drawLine(cx, cy+8, cx+2, cy+6);
    u8g2.drawLine(cx+2, cy+6, cx+4, cy+8);
  }
}

// ========== PHÂN LOẠI TỪNG YẾU TỐ ==========
int xepLoaiNhiet(float t) {
  if (t > 35)           return 2;
  if (t > 30)           return 1;
  if (t >= 20 && t <= 30) return 0;
  return 3;
}

int xepLoaiAm(float h) {
  if (h > 85) return 2;
  if (h < 40) return 2;
  if (h >= 50 && h <= 70) return 0;
  return 1;
}

int xepLoaiDat(int pct) {
  if (pct > 80)           return 4;
  if (pct < 30)           return 3;
  if (pct >= 60 && pct <= 70) return 0;
  return 1;
}

int xepLoaiSang(int pct) {
  if (pct > 90)  return 2;
  if (pct < 20)  return 3;
  if (pct >= 50) return 0;
  return 1;
}

int xepLoaiTong(float t, float h, int soil, int light) {
  if (soil > 80) return 4;
  if (soil < 30) return 3;
  if (t > 35)    return 2;
  if (light < 20) return 5;
  if (light > 90) return 6;
  if (t >= 20 && t <= 30 && h >= 50 && h <= 70 && soil >= 60 && soil <= 70 && light >= 50 && light <= 90) return 0;
  return 1;
}

const char* tenCX[] = {
  "Tuyet Voi!",   // 0
  "On Thoi",      // 1
  "Nong Qua!",    // 2
  "Khat Nuoc!",   // 3
  "Ngop Nuoc!",   // 4
  "Buon Ngu...",  // 5
  "Choi Mat!"     // 6
};

// ========== BẬT TẮT LED ==========
void setLED(bool state) {
  ledState = state;
  digitalWrite(LED_PIN, state ? HIGH : LOW);
  String payload = "{\"led\":" + String(state ? "true" : "false") + "}";
  mqtt.publish("v1/devices/me/telemetry", payload.c_str());
  Serial.printf("LED: %s\n", state ? "ON" : "OFF");
}

// ========== AUTOMATION ==========
void chayAutomation() {
  if (!autoMode) return;
  if (lightPct < 20 && !ledState) {
    setLED(true);
    Serial.println("AUTO: Bat den vi qua toi");
  }
  if (lightPct >= 50 && ledState) {
    setLED(false);
    Serial.println("AUTO: Tat den vi du sang");
  }
}

// ========== NHẬN LỆNH TỪ THINGSBOARD ==========
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.printf("Nhan [%s]: %s\n", topic, msg.c_str());

  if (msg.indexOf("\"led\":true") >= 0)   setLED(true);
  if (msg.indexOf("\"led\":false") >= 0)  setLED(false);
  if (msg.indexOf("\"auto\":true") >= 0)  autoMode = true;
  if (msg.indexOf("\"auto\":false") >= 0) autoMode = false;
}

// ========== KẾT NỐI WiFi ==========
void ketNoiWifi() {
  WiFi.begin(ssid, password);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(0, 20);
  u8g2.print("Connecting WiFi...");
  u8g2.sendBuffer();

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500); Serial.print("."); timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK! IP: " + WiFi.localIP().toString());
    u8g2.clearBuffer();
    u8g2.setCursor(0, 20); u8g2.print("WiFi OK!");
    u8g2.setCursor(0, 35); u8g2.print(WiFi.localIP().toString());
    u8g2.sendBuffer();
    delay(2000);
  } else {
    Serial.println("\nWiFi FAILED!");
    u8g2.clearBuffer();
    u8g2.setCursor(0, 20); u8g2.print("WiFi FAILED!");
    u8g2.setCursor(0, 35); u8g2.print("Kiem tra lai WiFi!");
    u8g2.sendBuffer();
    while (true);
  }
}

// ========== KẾT NỐI THINGSBOARD ==========
void ketNoiTB() {
  mqtt.setServer(tb_host, tb_port);
  mqtt.setCallback(callback);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(0, 20);
  u8g2.print("Connecting TB...");
  u8g2.sendBuffer();

  int timeout = 0;
  while (!mqtt.connected() && timeout < 5) {
    String clientId = "ESP32-" + String(random(1000, 9999));
    if (mqtt.connect(clientId.c_str(), tb_token, NULL)) {
      Serial.println("ThingsBoard OK!");
      mqtt.subscribe("v1/devices/me/rpc/request/+");
      u8g2.clearBuffer();
      u8g2.setCursor(0, 20); u8g2.print("ThingsBoard OK!");
      u8g2.setCursor(0, 35); u8g2.print("Dang gui du lieu...");
      u8g2.sendBuffer();
      delay(1000);
    } else {
      Serial.printf("TB FAILED! rc=%d\n", mqtt.state());
      timeout++; delay(2000);
    }
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(SOIL_POWER_PIN, OUTPUT);
  digitalWrite(SOIL_POWER_PIN, LOW);

  dht.begin();
  u8g2.begin();
  mqtt.setBufferSize(512);
  ketNoiWifi();
  ketNoiTB();
}

// ========== LOOP ==========
void loop() {
  if (!mqtt.connected()) ketNoiTB();
  mqtt.loop();

  static unsigned long lastRead = 0;
  if (millis() - lastRead >= 5000) {
    lastRead = millis();

    nhietDo = dht.readTemperature();
    doAm    = dht.readHumidity();

    digitalWrite(SOIL_POWER_PIN, HIGH);
    delay(10);
    int soilRaw = analogRead(SOIL_PIN);
    digitalWrite(SOIL_POWER_PIN, LOW);

    soilPct = constrain(map(soilRaw, 0, 4095, 0, 100), 0, 100);

    int lightRaw = analogRead(LIGHT_PIN);
    lightPct = constrain(map(lightRaw, 4095, 0, 0, 100), 0, 100);

    if (!isnan(nhietDo) && !isnan(doAm)) {
      int cxT = xepLoaiTong(nhietDo, doAm, soilPct, lightPct);

      String payload = "{";
      payload += "\"nhietdo\":"   + String(nhietDo, 1) + ",";
      payload += "\"doam\":"      + String(doAm, 0)    + ",";
      payload += "\"dat\":"       + String(soilPct)    + ",";
      payload += "\"sang\":"      + String(lightPct)   + ",";
      payload += "\"led\":"       + String(ledState ? "true" : "false") + ",";
      payload += "\"auto\":"      + String(autoMode  ? "true" : "false") + ",";
      payload += "\"trang_thai\":\"" + String(tenCX[cxT]) + "\"";
      payload += "}";
      mqtt.publish("v1/devices/me/telemetry", payload.c_str());

      chayAutomation();

      Serial.printf("→ T:%.1f H:%.0f Dat:%d%% Sang:%d%% [%s]\n",
        nhietDo, doAm, soilPct, lightPct, tenCX[cxT]);

      // ========== VẼ MÀN HÌNH OLED ==========
      u8g2.clearBuffer();

      // 1. Vẽ mặt Maruko siêu bự ở giữa
      veMat(cxT);

      // 2. In Tên biểu cảm (đã bỏ hết số liệu khô khan)
      u8g2.setFont(u8g2_font_8x13B_tr); // Font chữ to, đậm
      String text = tenCX[cxT];
      int textWidth = u8g2.getStrWidth(text.c_str());
      u8g2.setCursor((128 - textWidth) / 2, 60); // Tự động căn giữa text dưới khuôn mặt
      u8g2.print(text);

      u8g2.sendBuffer();
    }
  }
}