// =====================================================
// ================= SMART PLANT ========================
// =====================================================

#include <U8g2lib.h>
#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>

// ================= WIFI =================
const char* ssid     = "FPT telecom";
const char* password = "123456a@";

// ================= THINGSBOARD =================
const char* tb_host  = "mqtt.thingsboard.cloud";
const int   tb_port  = 1883;
const char* tb_token = "MzTCWYZkbx0YpmQuLTrs";

// ================= OLED =================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ================= SENSOR =================
#define DHTPIN         23
#define DHTTYPE        DHT11
#define SOIL_PIN       34
#define SOIL_POWER_PIN 25
#define LIGHT_PIN      35

DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient mqtt(espClient);

// ================= DATA =================
float nhietDo = 0;
float doAm = 0;

int soilPct = 0;
int lightPct = 0;

// =====================================================
// ================= TEN CAM XUC ========================
// =====================================================
String tenCamXuc(int cx) {

  switch(cx) {

    case 0: return "TO VUI QUA!";
    case 1: return "TO DANG ON";
    case 2: return "TO NONG QUA!";
    case 3: return "TO KHAT NUOC!";
    case 4: return "TO BI UNG!";
    case 5: return "TO BUON NGU...";
    case 6: return "TO CHOI MAT!";

    default: return "???";
  }
}

// =====================================================
// ================= BIỂU CẢM CUTE =====================
// =====================================================
void veMat(int camXuc) {

  int eyeY = 26 + sin(millis()/250.0) * 2;

  int mouthY = 47 + sin(millis()/250.0) * 2;

  int talk = (millis()/180)%4;

  // viền
  u8g2.drawRFrame(0,0,128,64,6);

  // =================================================
  // 0 = TO VUI QUA!
  // =================================================
  if (camXuc == 0) {

    u8g2.drawDisc(42, eyeY, 9);
    u8g2.drawDisc(86, eyeY, 9);

    u8g2.setDrawColor(0);

    u8g2.drawDisc(39, eyeY-2, 2);
    u8g2.drawDisc(83, eyeY-2, 2);

    u8g2.setDrawColor(1);

    u8g2.drawCircle(25, 38, 4);
    u8g2.drawCircle(103, 38, 4);

    if (talk == 0)
      u8g2.drawEllipse(64, mouthY, 12, 6);

    else if (talk == 1)
      u8g2.drawDisc(64, mouthY, 8);

    else
      u8g2.drawEllipse(64, mouthY, 10, 4);
  }

  // =================================================
  // 1 = TO DANG ON
  // =================================================
  else if (camXuc == 1) {

    bool blink = ((millis()/2200)%2);

    if (blink) {

      u8g2.drawDisc(42, eyeY, 7);
      u8g2.drawDisc(86, eyeY, 7);

    } else {

      u8g2.drawLine(34, eyeY, 50, eyeY);
      u8g2.drawLine(78, eyeY, 94, eyeY);
    }

    if (talk % 2 == 0)
      u8g2.drawLine(52, mouthY, 76, mouthY);

    else
      u8g2.drawEllipse(64, mouthY, 8, 3);
  }

  // =================================================
  // 2 = TO NONG QUA!
  // =================================================
  else if (camXuc == 2) {

    u8g2.drawEllipse(42, eyeY, 10, 7);
    u8g2.drawEllipse(86, eyeY, 10, 7);

    u8g2.drawDisc(42, eyeY, 4);
    u8g2.drawDisc(86, eyeY, 4);

    int sweat = (millis()/100)%12;

    u8g2.drawDisc(106, 18+sweat, 3);

    if (talk == 0)
      u8g2.drawCircle(64, mouthY, 7);

    else
      u8g2.drawEllipse(64, mouthY, 10, 6);
  }

  // =================================================
  // 3 = TO KHAT NUOC!
  // =================================================
  else if (camXuc == 3) {

    u8g2.drawEllipse(42, eyeY, 12, 10);
    u8g2.drawEllipse(86, eyeY, 12, 10);

    u8g2.drawDisc(42, eyeY+1, 5);
    u8g2.drawDisc(86, eyeY+1, 5);

    u8g2.setDrawColor(0);

    u8g2.drawDisc(39, eyeY-2, 2);
    u8g2.drawDisc(83, eyeY-2, 2);

    u8g2.setDrawColor(1);

    int tear = (millis()/90)%12;

    u8g2.drawLine(42, eyeY+10, 42, eyeY+15+tear);
    u8g2.drawLine(86, eyeY+10, 86, eyeY+15+tear);

    if (talk % 2 == 0)
      u8g2.drawEllipse(64, mouthY+4, 11, 5);

    else
      u8g2.drawEllipse(64, mouthY+5, 8, 4);
  }

  // =================================================
  // 4 = TO BI UNG!
  // =================================================
  else if (camXuc == 4) {

    int wave = sin(millis()/120.0) * 2;

    u8g2.drawCircle(42, eyeY, 10);
    u8g2.drawCircle(86, eyeY, 10);

    u8g2.drawCircle(42, eyeY, 5+wave);
    u8g2.drawCircle(86, eyeY, 5+wave);

    u8g2.drawLine(52, mouthY+4, 76, mouthY);

    int bubble = (millis()/150)%20;

    u8g2.drawCircle(100, 40-bubble, 4);
    u8g2.drawCircle(108, 24-bubble, 2);

    u8g2.drawDisc(18, 20, 3);
    u8g2.drawDisc(112, 18, 2);
  }

  // =================================================
  // 5 = TO BUON NGU...
  // =================================================
  else if (camXuc == 5) {

    u8g2.drawLine(32, eyeY, 52, eyeY);
    u8g2.drawLine(76, eyeY, 96, eyeY);

    u8g2.drawCircle(64, mouthY, 2);

    int bubble = 4 + (millis()/250)%6;

    u8g2.drawCircle(90, 40, bubble);

    if ((millis()/500)%2) {

      u8g2.setFont(u8g2_font_6x10_tr);

      u8g2.drawStr(100, 20, "Z");
      u8g2.drawStr(110, 12, "Z");
    }
  }

  // =================================================
  // 6 = TO CHOI MAT!
  // =================================================
  else if (camXuc == 6) {

    int twitch = (millis()/100)%4;

    u8g2.drawLine(32, eyeY-6+twitch, 52, eyeY+6);
    u8g2.drawLine(32, eyeY+6, 52, eyeY-6+twitch);

    u8g2.drawLine(76, eyeY-6+twitch, 96, eyeY+6);
    u8g2.drawLine(76, eyeY+6, 96, eyeY-6+twitch);

    if (talk == 0)
      u8g2.drawBox(52, mouthY, 24, 4);

    else
      u8g2.drawBox(56, mouthY, 16, 3);

    u8g2.drawLine(10, 8, 24, 20);
    u8g2.drawLine(118, 8, 104, 20);
  }

  // ===== TEN CAM XUC =====
  u8g2.setFont(u8g2_font_6x10_tr);

  String txt = tenCamXuc(camXuc);

  int w = u8g2.getStrWidth(txt.c_str());

  u8g2.drawStr((128-w)/2, 60, txt.c_str());
}

// =====================================================
// ================= XẾP LOẠI ===========================
// =====================================================
int xepLoaiTong(float t, float h, int soil, int light) {

  // ===== BI UNG =====
  if (soil >= 80)
    return 4;

  // ===== KHAT NUOC =====
  if (soil < 10)
    return 3;

  // ===== NONG =====
  if (t > 40)
    return 2;

  // ===== CHOI MAT =====
  if (light > 90)
    return 6;

  // ===== BUON NGU =====
  if (light < 10)
    return 5;

  // ===== TO VUI QUA =====
  if (
      t >= 24 && t <= 32 &&
      h >= 45 && h <= 75 &&
      soil >= 20 && soil <= 75 &&
      light >= 35 && light <= 80
  )
    return 0;

  // ===== TO DANG ON =====
  return 1;
}
// =====================================================
// ================= WIFI ===============================
// =====================================================
void ketNoiWifi() {

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
}

// =====================================================
// ================= THINGSBOARD ========================
// =====================================================
void ketNoiTB() {

  mqtt.setServer(tb_host, tb_port);

  while (!mqtt.connected()) {

    String clientId = "ESP32-" + String(random(1000,9999));

    if (mqtt.connect(clientId.c_str(), tb_token, NULL)) {

      Serial.println("ThingsBoard Connected!");

    } else {

      Serial.println("TB Failed!");

      delay(1000);
    }
  }
}

// =====================================================
// ================= SETUP ==============================
// =====================================================
void setup() {

  Serial.begin(115200);

  dht.begin();

  u8g2.begin();

  pinMode(SOIL_POWER_PIN, OUTPUT);

  digitalWrite(SOIL_POWER_PIN, LOW);

  mqtt.setBufferSize(512);

  ketNoiWifi();

  ketNoiTB();
}

// =====================================================
// ================= LOOP ===============================
// =====================================================
void loop() {

  if (!mqtt.connected())
    ketNoiTB();

  mqtt.loop();

  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate >= 1000) {

    lastUpdate = millis();

    nhietDo = dht.readTemperature();

    doAm = dht.readHumidity();

    // ===== SOIL =====
    digitalWrite(SOIL_POWER_PIN, HIGH);

    delay(10);

    int soilRaw = analogRead(SOIL_PIN);

    digitalWrite(SOIL_POWER_PIN, LOW);

    // ===== CALIB CHO 2 QUE THEP =====
    // ===== SOIL =====
digitalWrite(SOIL_POWER_PIN, HIGH);

delay(10);

// ===== DOC NHIEU LAN CHONG NHIEU =====
long total = 0;

for(int i = 0; i < 15; i++) {

  total += analogRead(SOIL_PIN);

  delay(2);
}

 soilRaw = total / 15;

digitalWrite(SOIL_POWER_PIN, LOW);

// ===== MAP DO AM DAT =====
// Board cua ban:
// KHO  = gia tri thap
// UOT  = gia tri cao

soilPct = constrain(
  map(soilRaw, 0, 4095, 0, 100),
  0,
  100
);

// ===== DEBUG SERIAL =====
Serial.print("Soil Raw: ");
Serial.println(soilRaw);

Serial.print("Soil %: ");
Serial.println(soilPct);

    // ===== LIGHT =====
    int lightRaw = analogRead(LIGHT_PIN);

    lightPct = constrain(
      map(lightRaw, 4095, 0, 0, 100),
      0,
      100
    );

    // ===== CAM XUC =====
    int cxT = xepLoaiTong(
      nhietDo,
      doAm,
      soilPct,
      lightPct
    );

    String tenCX = tenCamXuc(cxT);

    // ===== SEND THINGSBOARD =====
    String payload = "{";

    payload += "\"nhietdo\":" + String(nhietDo,1) + ",";
    payload += "\"doam\":" + String(doAm,0) + ",";
    payload += "\"dat\":" + String(soilPct) + ",";
    payload += "\"sang\":" + String(lightPct) + ",";
    payload += "\"camxuc\":\"" + tenCX + "\"";

    payload += "}";

    mqtt.publish(
      "v1/devices/me/telemetry",
      payload.c_str()
    );

    // ===== SERIAL =====
    Serial.println("========================");

    Serial.print("Nhiet do: ");
    Serial.println(nhietDo);

    Serial.print("Do am KK: ");
    Serial.println(doAm);

    Serial.print("Do am dat: ");
    Serial.println(soilPct);

    Serial.print("Anh sang: ");
    Serial.println(lightPct);

    Serial.print("Cam xuc: ");
    Serial.println(tenCX);
  }

  // ===== OLED =====
  int cxT = xepLoaiTong(
    nhietDo,
    doAm,
    soilPct,
    lightPct
  );

  u8g2.clearBuffer();

  veMat(cxT);

  u8g2.sendBuffer();

  // animation mượt
  delay(25);
}