#include <WiFi.h>
#include <WiFiUdp.h>

#define RELAY_PIN 1  // 继电器控制引脚（确认你使用的是 IO1）

const char* ssid = "XIAO_AP";       // 由 ESP32C3 开启的 AP 名称
const char* password = "12345678";  // 密码

WiFiUDP udp;
const uint16_t udpPort = 1234;
char incomingPacket[255];

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);       // 设置继电器引脚为输出
  digitalWrite(RELAY_PIN, LOW);     // 初始关闭继电器

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n连接成功，IP 地址: ");
  Serial.println(WiFi.localIP());

  udp.begin(udpPort);
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = 0;

    Serial.print("收到UDP数据：");
    Serial.println(incomingPacket);

    if (strcmp(incomingPacket, "DELAY_ON") == 0) {
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("继电器已开启");
    } else if (strcmp(incomingPacket, "DELAY_OFF") == 0) {
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("继电器已关闭");
    } else {
      // 其他内容不处理，仅打印
      Serial.println("未识别的命令，忽略处理");
    }
  }

  delay(10);
}
