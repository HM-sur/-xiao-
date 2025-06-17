#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>

// —— 配置 —— 
const char* ssid = "XIAO_AP";
const char* password = "12345678";
const uint16_t udpPort = 1234;

#define PIN 5
#define NUMPIXELS 30

WiFiUDP udp;
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

char incomingPacket[255];
int packetLen = 0;

enum LedState { OFF, STATIC_COLOR, WATERFALL };
LedState ledState = OFF;

// 流水相关
unsigned long lastMillis = 0;
const unsigned long interval = 100;
int wfIndex = 0;

void setup() {
  Serial.begin(115200);
  pixels.begin();
  pixels.clear();
  pixels.show();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println("\nWiFi connected:");
  Serial.println(WiFi.localIP());

  udp.begin(udpPort);
  Serial.printf("UDP listening on %d\n", udpPort);
}

void loop() {
  int size = udp.parsePacket();
  if (size) {
    packetLen = udp.read(incomingPacket, sizeof(incomingPacket)-1);
    incomingPacket[packetLen] = 0;

    // 调试打印
    Serial.print("Received [");
    Serial.print(packetLen);
    Serial.print("]: ");
    Serial.println(incomingPacket);

    Serial.print("Hex: ");
    for (int i = 0; i < packetLen; i++) {
      Serial.printf("%02X ", (uint8_t)incomingPacket[i]);
    }
    Serial.println();

    // 比对指令（不含回车换行）
    if (strcmp(incomingPacket, "LED_ON") == 0) {
      ledState = STATIC_COLOR;
      showStatic();
    }
    else if (strcmp(incomingPacket, "LED_OFF") == 0) {
      ledState = OFF;
      pixels.clear();
      pixels.show();
    }
    else if (strcmp(incomingPacket, "Waterfall_Light") == 0) {
      ledState = WATERFALL;
      wfIndex = 0;
      lastMillis = millis();
    } else {
      Serial.println("Unknown cmd");
    }
  }

  // 根据状态执行流水
  if (ledState == WATERFALL) {
    unsigned long now = millis();
    if (now - lastMillis >= interval) {
      lastMillis = now;
      waterfall();
      wfIndex = (wfIndex + 1) % NUMPIXELS;
    }
  }

  delay(10);
}

void showStatic() {
  pixels.clear();
  for (int i = 0; i < NUMPIXELS; i++) {
    if (i % 3 == 0) pixels.setPixelColor(i, pixels.Color(255, 0, 0));
    else if (i % 3 == 1) pixels.setPixelColor(i, pixels.Color(0, 255, 0));
    else pixels.setPixelColor(i, pixels.Color(0, 0, 255));
  }
  pixels.setBrightness(200);
  pixels.show();
}

void waterfall() {
  // 简单尾迹流水：主灯+2个渐暗尾巴
  pixels.clear();
  for (int t = 0; t < 3; t++) {
    int pos = (wfIndex - t + NUMPIXELS) % NUMPIXELS;
    uint8_t val = 255 - t * 80;
    pixels.setPixelColor(pos, pixels.Color(0, 0, val));
  }
  pixels.show();
}
