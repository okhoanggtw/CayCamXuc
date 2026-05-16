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
// ================= BIỂU CẢM CUTE =====================
// =====================================================
void veMat(int camXuc) {

  int cx = 64;
  int cy = 32;

  bool blink = ((millis() / 2200) % 2 == 0);

  int talk = (millis() / 180) % 4;

  int move = sin(millis() / 250.0) * 2;

  cy += move;

  // =================================================
  // 0 = TO VUI QUA!
  // =================================================
  if (camXuc == 0) {

    // mắt cười
    u8g2.drawLine(35, 25, 45, 20);
    u8g2.drawLine(45, 20, 55, 25);

    u8g2.drawLine(73, 25, 83, 20);
    u8g2.drawLine(83, 20, 93, 25);

    // miệng nói chuyện
    if (talk == 0)
      u8g2.drawCircle(cx, 46, 6);

    else if (talk == 1)
      u8g2.drawDisc(cx, 46, 8);

    else if (talk == 2)
      u8g2.drawEllipse(cx, 46, 10, 6);

    else
      u8g2.drawDisc(cx, 46, 5);
  }

  // =================================================
  // 1 = TO DANG ON
  // =================================================
  else if (camXuc == 1) {

    if (blink) {

      u8g2.drawDisc(45, 26, 3);
      u8g2.drawDisc(83, 26, 3);

    } else {

      u8g2.drawLine(40, 26, 50, 26);
      u8g2.drawLine(78, 26, 88, 26);
    }

    if (talk % 2 == 0)
      u8g2.drawLine(55, 46, 73, 46);

    else
      u8g2.drawEllipse(cx, 46, 8, 3);
  }

  // =================================================
  // 2 = TO NONG QUA!
  // =================================================
  else if (camXuc == 2) {

    // mắt cau có
    u8g2.drawLine(38, 20, 50, 28);
    u8g2.drawLine(78, 28, 90, 20);

    // miệng thở
    if (talk == 0)
      u8g2.drawCircle(cx, 48, 5);

    else if (talk == 1)
      u8g2.drawCircle(cx, 48, 8);

    else
      u8g2.drawCircle(cx, 48, 6);

    // mồ hôi
    int sweat = (millis()/100)%12;

    u8g2.drawDisc(100, 20+sweat, 2);
  }

  // =================================================
  // 3 = TO KHAT NUOC!
  // =================================================
  else if (camXuc == 3) {

    // mắt buồn
    u8g2.drawLine(38, 30, 50, 22);
    u8g2.drawLine(78, 22, 90, 30);

    // nước mắt
    int tear = (millis()/120)%12;

    u8g2.drawLine(45, 30, 45, 35+tear);
    u8g2.drawLine(83, 30, 83, 35+tear);

    // miệng mếu
    if (talk % 2 == 0)
      u8g2.drawEllipse(cx, 50, 10, 4);

    else
      u8g2.drawEllipse(cx, 52, 8, 5);
  }

  // =================================================
  // 4 = CUU TO VOI!
  // =================================================
  else if (camXuc == 4) {

    int shake = ((millis()/100)%2)*2;

    // mắt X_X
    u8g2.drawLine(38+shake, 22, 48+shake, 32);
    u8g2.drawLine(48+shake, 22, 38+shake, 32);

    u8g2.drawLine(80+shake, 22, 90+shake, 32);
    u8g2.drawLine(90+shake, 22, 80+shake, 32);

    // miệng sốc
    if (talk == 0)
      u8g2.drawDisc(cx, 50, 5);

    else if (talk == 1)
      u8g2.drawDisc(cx, 50, 8);

    else
      u8g2.drawDisc(cx, 50, 6);

    // bong bóng
    int bubble = (millis()/150)%15;

    u8g2.drawCircle(100, 20-bubble, 3);
    u8g2.drawCircle(106, 10-bubble, 2);
  }

  // =================================================
  // 5 = TO BUON NGU...
  // =================================================
  else if (camXuc == 5) {

    // mắt ngủ
    u8g2.drawLine(38, 28, 50, 28);
    u8g2.drawLine(78, 28, 90, 28);

    // miệng nhỏ
    if (talk % 2 == 0)
      u8g2.drawCircle(cx, 46, 2);

    else
      u8g2.drawCircle(cx, 48, 3);

    // bong bóng ngủ
    int bubble = 4 + (millis()/250)%5;

    u8g2.drawCircle(90, 40, bubble);

    // chữ Z
    if ((millis()/500)%2) {

      u8g2.setFont(u8g2_font_6x10_tr);

      u8g2.drawStr(98, 18, "Z");
      u8g2.drawStr(108, 10, "Z");
    }
  }

  // =================================================
  // 6 = TO CHOI MAT!
  // =================================================
  else if (camXuc == 6) {

    int twitch = (millis()/120)%3;

    // mắt chói
    u8g2.drawLine(38, 22+twitch, 50, 32);
    u8g2.drawLine(38, 32, 50, 22+twitch);

    u8g2.drawLine(78, 22+twitch, 90, 32);
    u8g2.drawLine(78, 32, 90, 22+twitch);

    // miệng khó chịu
    if (talk == 0)
      u8g2.drawBox(54, 48, 20, 3);

    else if (talk == 1)
      u8g2.drawBox(50, 48, 28, 4);

    else
      u8g2.drawBox(56, 48, 16, 3);

    // tia sáng
    u8g2.drawLine(15, 10, 25, 20);
    u8g2.drawLine(113, 10, 103, 20);
  }
}

// =====================================================
// ================= XẾP LOẠI ===========================
// =====================================================
int xepLoaiTong(float t, float h, int soil, int light) {

  if (soil > 80) return 4;

  if (soil < 30) return 3;

  if (t > 35) return 2;

  if (light < 20) return 5;

  if (light > 90) return 6;

  if (t >= 20 && t <= 30 &&
      h >= 50 && h <= 70 &&
      soil >= 60 && soil <= 70 &&
      light >= 50 && light <= 90)
      return 0;

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

  // ===== UPDATE SENSOR MOI 1 GIAY =====
  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate >= 1000) {

    lastUpdate = millis();

    nhietDo = dht.readTemperature();

    doAm = dht.readHumidity();

    digitalWrite(SOIL_POWER_PIN, HIGH);

    delay(10);

    int soilRaw = analogRead(SOIL_PIN);

    digitalWrite(SOIL_POWER_PIN, LOW);

    soilPct = constrain(
      map(soilRaw, 0, 4095, 0, 100),
      0,
      100
    );

    int lightRaw = analogRead(LIGHT_PIN);

    lightPct = constrain(
      map(lightRaw, 4095, 0, 0, 100),
      0,
      100
    );

    // ===== SEND THINGSBOARD =====
    String payload = "{";

    payload += "\"nhietdo\":" + String(nhietDo,1) + ",";
    payload += "\"doam\":" + String(doAm,0) + ",";
    payload += "\"dat\":" + String(soilPct) + ",";
    payload += "\"sang\":" + String(lightPct);

    payload += "}";

    mqtt.publish(
      "v1/devices/me/telemetry",
      payload.c_str()
    );

    // ===== SERIAL =====
    Serial.println("===================================");

    Serial.print("Nhiet do: ");
    Serial.print(nhietDo);
    Serial.println(" C");

    Serial.print("Do am KK: ");
    Serial.print(doAm);
    Serial.println(" %");

    Serial.print("Do am dat: ");
    Serial.print(soilPct);
    Serial.println(" %");

    Serial.print("Anh sang: ");
    Serial.print(lightPct);
    Serial.println(" %");
  }

  // ===== OLED ANIMATION =====
  int cxT = xepLoaiTong(
    nhietDo,
    doAm,
    soilPct,
    lightPct
  );

  u8g2.clearBuffer();

  veMat(cxT);

  u8g2.sendBuffer();

  delay(2000);
}