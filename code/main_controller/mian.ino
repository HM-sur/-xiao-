#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <U8g2lib.h>

// ===== 硬件 & 库 初始化 =====
// OLED（全缓存模式，硬件 I2C）
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// 摇杆（模拟量）和退出键 PIN 定义
const int JOY_PIN   = A0;  // 摇杆信号接 A0
const int BTN_EXIT  = D1;  // 退出按键接 D1

// 摇杆阈值
const int TH_DOWN_LOW  = 1200;  // 下移
const int TH_UP_LOW    = 3200;  // 上移下边界
const int TH_UP_HIGH   = 3600;  // 上移上边界
const int TH_ENTER     = 3900;  // 进入/确认

// 防抖 & 定时
unsigned long lastJoyMove    = 0;
const unsigned long DEBOUNCE = 200;    // ms
unsigned long lastBroadcast  = 0;

// WiFi AP + UDP 广播
const char* ssid     = "XIAO_AP";
const char* password = "12345678";
WiFiUDP udp;
const int udpPort    = 1234;

// ===== 菜单系统 =====
const uint8_t MENU_COUNT = 4;
const char* menuTitles[MENU_COUNT] = { "AP 信息", "LED", "Delay", "Fan" };

uint8_t currentMenu = 0;   // 主菜单索引 0–3
uint8_t depth       = 0;   // 0=主列表, 1=子菜单

// 子菜单选项索引（LED 多于2项，需分页）
// subOption = 当前选中的菜单项（0~2）
// ledPage = LED子菜单当前显示页（0 或 1）
uint8_t subOption = 0;   
uint8_t ledPage   = 0;   // 新增变量，LED菜单分页

void setup() {
  Serial.begin(115200);
  pinMode(BTN_EXIT, INPUT_PULLUP);

  // OLED 初始化
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB14_tr); // 大字体，一屏3行

  // 启动 WiFi AP
  WiFi.softAP(ssid, password);
  delay(500);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  // 首次绘制
  drawMenu();
}

void loop() {
  unsigned long now = millis();

  // —— UDP 广播（保活或测试）——  
  if (now - lastBroadcast > 1000) {
    lastBroadcast = now;
    udp.beginPacket("192.168.4.255", udpPort);
    udp.print("Broadcast from C3");
    udp.endPacket();
  }

  // —— 读取摇杆 & 按键 ——  
  int v = analogRead(JOY_PIN);
  bool exitPressed = (digitalRead(BTN_EXIT) == LOW);

  if (now - lastJoyMove > DEBOUNCE) {
    if (depth == 0) {
      // 主菜单导航
      if (v < TH_DOWN_LOW) {
        currentMenu = (currentMenu + 1) % MENU_COUNT;
        drawMenu();
        lastJoyMove = now;
      }
      else if (v > TH_UP_LOW && v < TH_UP_HIGH) {
        currentMenu = (currentMenu + MENU_COUNT - 1) % MENU_COUNT;
        drawMenu();
        lastJoyMove = now;
      }
      else if (v > TH_ENTER) {
        // 进入子菜单
        depth = 1;
        subOption = 0;
        ledPage = 0;           // 进入LED菜单时重置分页
        drawMenu();
        lastJoyMove = now;
      }
    } else {
      // 子菜单导航
      if (currentMenu == 1) {
        // LED 子菜单多项处理（3项分页显示，每页2项）
        const uint8_t totalItems = 3;
        const uint8_t pageSize = 2;

        if (v < TH_DOWN_LOW) {
          // 下移
          subOption = (subOption + 1) % totalItems;
          ledPage = subOption / pageSize;  // 自动翻页
          drawMenu();
          lastJoyMove = now;
        }
        else if (v > TH_UP_LOW && v < TH_UP_HIGH) {
          // 上移
          subOption = (subOption + totalItems - 1) % totalItems;
          ledPage = subOption / pageSize;
          drawMenu();
          lastJoyMove = now;
        }
        else if (v > TH_ENTER) {
          sendProtocol(currentMenu, subOption);
          lastJoyMove = now;
        }
      } else {
        // 其他子菜单只有两项，保持不变
        if (v < TH_DOWN_LOW) {
          subOption = (subOption + 1) % 2;
          drawMenu();
          lastJoyMove = now;
        }
        else if (v > TH_UP_LOW && v < TH_UP_HIGH) {
          subOption = (subOption + 2 - 1) % 2;
          drawMenu();
          lastJoyMove = now;
        }
        else if (v > TH_ENTER) {
          sendProtocol(currentMenu, subOption);
          lastJoyMove = now;
        }
      }
    }
  }

  // 退出至主菜单
  if (exitPressed) {
    if (depth == 1) {
      depth = 0;
      drawMenu();
    }
    // 等待松手防抖
    while (digitalRead(BTN_EXIT) == LOW) { delay(10); }
    lastJoyMove = now;
  }
}

// 绘制当前菜单（主或子）
void drawMenu() {
  u8g2.clearBuffer();

  if (depth == 0) {
    // 主菜单，一屏最多 3 项
    int start = currentMenu;
    if (start > MENU_COUNT - 3) start = MENU_COUNT - 3;
    if (start < 0) start = 0;
    for (int i = 0; i < 3; i++) {
      int idx = start + i;
      if (idx == currentMenu) {
        u8g2.drawBox(0, i*21, 128, 21);
        u8g2.setDrawColor(0);
        u8g2.drawStr(2, i*21 + 16, menuTitles[idx]);
        u8g2.setDrawColor(1);
      } else {
        u8g2.drawStr(2, i*21 + 16, menuTitles[idx]);
      }
    }
  } else {
    // 子菜单
    if (currentMenu == 0) {
      // AP 信息
      IPAddress ip = WiFi.softAPIP();
      char buf[32];
      u8g2.drawStr(0, 16, "=== AP 信息 ===");
      snprintf(buf, sizeof(buf), "SSID:%s", ssid);
      u8g2.drawStr(0, 34, buf);
      snprintf(buf, sizeof(buf), "IP:  %s", ip.toString().c_str());
      u8g2.drawStr(0, 52, buf);
    } else if (currentMenu == 1) {
      // LED 子菜单，多项分页显示，每页显示两项
      const char* ledOptions[3] = { "ON", "OFF", "Waterfall Light" };
      const uint8_t pageSize = 2;
      uint8_t startIndex = ledPage * pageSize;

      for (int i = 0; i < pageSize; i++) {
        int itemIndex = startIndex + i;
        if (itemIndex >= 3) break; // 防止越界

        if (itemIndex == subOption) {
          u8g2.drawBox(0, i*32, 128, 32);
          u8g2.setDrawColor(0);
          u8g2.drawStr(2, i*32 + 24, ledOptions[itemIndex]);
          u8g2.setDrawColor(1);
        } else {
          u8g2.drawStr(2, i*32 + 24, ledOptions[itemIndex]);
        }
      }
    } else {
      // Delay 和 Fan 子菜单，保持原样（两项）
      const char* opts[2] = { " ON", " OFF" };
      for (int i = 0; i < 2; i++) {
        if (i == subOption) {
          u8g2.drawBox(0, i*32, 128, 32);
          u8g2.setDrawColor(0);
          u8g2.drawStr(2, i*32 + 24, opts[i]);
          u8g2.setDrawColor(1);
        } else {
          u8g2.drawStr(2, i*32 + 24, opts[i]);
        }
      }
    }
  }

  u8g2.sendBuffer();
}

// 根据菜单 ID 和 subOption 发送对应广播协议
void sendProtocol(uint8_t menuId, uint8_t opt) {
  const char* payload = nullptr;

  switch (menuId) {
    case 1: // LED
      if (opt == 0) payload = "LED_ON";
      else if (opt == 1) payload = "LED_OFF";
      else if (opt == 2) payload = "WATERFALL_LIGHT";
      break;
    case 2: // Delay
      payload = (opt == 0) ? "DELAY_ON" : "DELAY_OFF";
      break;
    case 3: // Fan
      payload = (opt == 0) ? "FAN_ON" : "FAN_OFF";
      break;
    default:
      return;  // AP 信息不发送
  }

  Serial.print("Send protocol: ");
  Serial.println(payload);

  udp.beginPacket("192.168.4.255", udpPort);
  udp.print(payload);
  udp.endPacket();
}
