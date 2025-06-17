#include <WiFi.h>
#include <WiFiUdp.h>

#define FAN_PIN A0  // 风扇控制引脚

const char* ssid = "XIAO_AP";         // AP名称
const char* password = "12345678";    // AP密码

WiFiUDP udp;
const uint16_t udpPort = 1234;
char incomingPacket[255];

void setup() {
  Serial.begin(115200);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW); // 默认关闭风扇

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  udp.begin(udpPort);
  Serial.printf("UDP listening on port %d\n", udpPort);
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = 0;

    Serial.print("Received UDP data: ");
    Serial.println(incomingPacket);

    if (strcmp(incomingPacket, "FAN_ON") == 0) {
      digitalWrite(FAN_PIN, HIGH);
      Serial.println("Fan turned ON");
    }
    else if (strcmp(incomingPacket, "FAN_OFF") == 0) {
      digitalWrite(FAN_PIN, LOW);
      Serial.println("Fan turned OFF");
    }
    // 其他数据不响应，仅打印
  }

  delay(10);
}
