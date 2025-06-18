#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <U8g2lib.h>

// ===== 硬件 & 库 初始化 =====
// OLED（全缓存模式，硬件 I2C）
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

#define BUZZER_PIN D7

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
// 主菜单 5 项，新增 "AC"
const uint8_t MENU_COUNT = 5;
const char* menuTitles[MENU_COUNT] = { "AP 信息", "LED", "Delay", "Fan", "AC" };

uint8_t currentMenu = 0;   // 主菜单索引 0–4
uint8_t depth       = 0;   // 0=主菜单,1=二级,2=三级

uint8_t subOption = 0;   // 当前层选项索引
uint8_t ledPage   = 0;   // LED 子菜单分页（0 或 1）
uint8_t acPage = 0;

void setup() {
  Serial.begin(115200);
  pinMode(BTN_EXIT, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // OLED 初始化
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB14_tr);

  // 启动 AP
  WiFi.softAP(ssid, password);
  delay(500);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  drawMenu();
}

void loop() {
  unsigned long now = millis();
  // UDP 心跳
  if (now - lastBroadcast > 1000) {
    lastBroadcast = now;
    udp.beginPacket("192.168.4.255", udpPort);
    udp.print("Broadcast from C3");
    udp.endPacket();
  }

  int v = analogRead(JOY_PIN);
  bool backPressed = (digitalRead(BTN_EXIT) == LOW);

  if (now - lastJoyMove > DEBOUNCE) {
    if (depth == 0) {
      // 主菜单
      if (v < TH_DOWN_LOW) {
        currentMenu = (currentMenu + 1) % MENU_COUNT;
        drawMenu();
        lastJoyMove = now;
      } else if (v > TH_UP_LOW && v < TH_UP_HIGH) {
        currentMenu = (currentMenu + MENU_COUNT - 1) % MENU_COUNT;
        drawMenu();
        lastJoyMove = now;
      } else if (v > TH_ENTER) {
        depth = 1;
        subOption = 0;
        ledPage = 0;
        drawMenu();
        lastJoyMove = now;
      }
    }
    else if (depth == 1) {
      // 二级菜单
      if (currentMenu == 1) {
        // LED 子菜单分页逻辑 (3 项分两页)
        const uint8_t total = 3, pageSize = 2;
        if (v < TH_DOWN_LOW) {
          subOption = (subOption + 1) % total;
          ledPage = subOption / pageSize;
          drawMenu();
          lastJoyMove = now;
        } else if (v > TH_UP_LOW && v < TH_UP_HIGH) {
          subOption = (subOption + total - 1) % total;
          ledPage = subOption / pageSize;
          drawMenu();
          lastJoyMove = now;
        } else if (v > TH_ENTER) {
          depth = 2;
          subOption = subOption % 2;  // 进入页内第一项或第二项
          drawMenu();
          lastJoyMove = now;
        }
      }
      else if (currentMenu == 4) {
        // AC 二级 (Switch/Temp)
        if (v < TH_DOWN_LOW) {
          subOption = (subOption + 1) % 2;
          drawMenu();
          lastJoyMove = now;
        } else if (v > TH_UP_LOW && v < TH_UP_HIGH) {
          subOption = (subOption + 2 - 1) % 2;
          drawMenu();
          lastJoyMove = now;
        } else if (v > TH_ENTER) {
          depth = 2;
          acPage = subOption;
          drawMenu();
          lastJoyMove = now;
        }
      }
      else {
          // Delay/Fan 子菜单
  if (v < TH_DOWN_LOW) {
    subOption = (subOption + 1) % 2;
    drawMenu();
    lastJoyMove = now;
  } else if (v > TH_UP_LOW && v < TH_UP_HIGH) {
    subOption = (subOption + 2 - 1) % 2;
    drawMenu();
    lastJoyMove = now;
  } else if (v > TH_ENTER) {
    // ✅ 修改：直接发送 UDP，不进入 depth=2
    sendProtocol(currentMenu, subOption, 0);  // page=0 仅作占位
    beep();
    lastJoyMove = now;
  }
      }
    }
    else {
      // depth == 2, 三级菜单
      if (v < TH_DOWN_LOW) {
        subOption = (subOption + 1) % 2;
        drawMenu();
        lastJoyMove = now;
      } else if (v > TH_UP_LOW && v < TH_UP_HIGH) {
        subOption = (subOption + 1) % 2;
        drawMenu();
        lastJoyMove = now;
      } else if (v > TH_ENTER) {
        uint8_t page = (currentMenu == 4) ? acPage : ledPage;
sendProtocol(currentMenu, subOption, page);
        lastJoyMove = now;
      }
    }
  }

  // 退出键
  if (backPressed) {
    if (depth == 2) {
      depth = 1;
    } else if (depth == 1) {
      depth = 0;
    }
    subOption = 0;
    drawMenu();
    while (digitalRead(BTN_EXIT) == LOW) delay(10);
    lastJoyMove = now;
  }
}

void drawMenu() {
  u8g2.clearBuffer();

  if (depth == 0) {
    // 主菜单
    int start = currentMenu;
    if (start > MENU_COUNT - 3) start = MENU_COUNT - 3;
    if (start < 0) start = 0;
    for (int i = 0; i < 3; i++) {
      int idx = start + i, y = i*21 + 16;
      if (idx == currentMenu) {
        u8g2.drawBox(0, y-14, 128, 21);
        u8g2.setDrawColor(0);
        u8g2.drawStr(2, y, menuTitles[idx]);
        u8g2.setDrawColor(1);
      } else {
        u8g2.drawStr(2, y, menuTitles[idx]);
      }
    }
  }
  else if (depth == 1) {
    // 二级菜单
    if (currentMenu == 0) {
      IPAddress ip=WiFi.softAPIP();char b[32];
      u8g2.drawStr(0,16,"=== AP 信息 ===");
      snprintf(b,32,"SSID:%s",ssid);    u8g2.drawStr(0,34,b);
      snprintf(b,32,"IP:  %s",ip.toString().c_str());u8g2.drawStr(0,52,b);
    }
    else if (currentMenu == 1) {
      // LED 子菜单，分页显示 2 项
      const char* opts[3] = { "ON", "OFF", "Waterfall Light" };
      uint8_t pageSize = 2, startIdx = ledPage * pageSize;
      for (int i = 0; i < pageSize; i++) {
        int idx = startIdx + i, y = i*32 + 24;
        if (idx >= 3) break;
        if (idx == subOption) {
          u8g2.drawBox(0, y-16, 128, 32);
          u8g2.setDrawColor(0);
          u8g2.drawStr(2, y, opts[idx]);
          u8g2.setDrawColor(1);
        } else {
          u8g2.drawStr(2, y, opts[idx]);
        }
      }
    }
    else if (currentMenu == 4) {
      // AC 二级菜单
      const char* acOpts[2] = { "Switch", "Temp" };
      for (int i = 0; i < 2; i++) {
        int y = i*32 + 24;
        if (i == subOption) {
          u8g2.drawBox(0, y-16, 128, 32);
          u8g2.setDrawColor(0);
          u8g2.drawStr(2, y, acOpts[i]);
          u8g2.setDrawColor(1);
        } else {
          u8g2.drawStr(2, y, acOpts[i]);
        }
      }
    }
    else {
      // Delay / Fan 二级菜单
      const char* opts[2] = { " ON", " OFF" };
      for (int i = 0; i < 2; i++) {
        int y = i*32 + 24;
        if (i == subOption) {
          u8g2.drawBox(0, y-16, 128, 32);
          u8g2.setDrawColor(0);
          u8g2.drawStr(2, y, opts[i]);
          u8g2.setDrawColor(1);
        } else {
          u8g2.drawStr(2, y, opts[i]);
        }
      }
    }
  }
  else {
    // 三级菜单
    if (currentMenu == 1) {
      // LED 三级保持原逻辑
      const char* opts[2] = { "ON", "OFF" };
      for (int i = 0; i < 2; i++) {
        int y = i*32 + 24;
        if (i == subOption) {
          u8g2.drawBox(0, y-16, 128, 32);
          u8g2.setDrawColor(0);
          u8g2.drawStr(2, y, opts[i]);
          u8g2.setDrawColor(1);
        } else {
          u8g2.drawStr(2, y, opts[i]);
        }
      }
    }
    else if (currentMenu == 4) {
      // AC 三级菜单
      if (subOption < 2 && depth == 2) {
        if (acPage == 0) {
          // Switch -> ON/OFF
          const char* sw[2] = { "ON", "OFF" };
          for (int i = 0; i < 2; i++) {
            int y = i*32 + 24;
            if (i == subOption) {
              u8g2.drawBox(0, y-16, 128, 32);
              u8g2.setDrawColor(0);
              u8g2.drawStr(2, y, sw[i]);
              u8g2.setDrawColor(1);
            } else {
              u8g2.drawStr(2, y, sw[i]);
            }
          }
        } else {
          // Temp -> + / -
          const char* tp[2] = { "+", "-" };
          for (int i = 0; i < 2; i++) {
            int y = i*32 + 24;
            if (i == subOption) {
              u8g2.drawBox(0, y-16, 128, 32);
              u8g2.setDrawColor(0);
              u8g2.drawStr(2, y, tp[i]);
              u8g2.setDrawColor(1);
            } else {
              u8g2.drawStr(2, y, tp[i]);
            }
          }
        }
      }
    }
    else {
      // Delay / Fan 三级菜单保持原样
      const char* opts[2] = { "ON", "OFF" };
      for (int i = 0; i < 2; i++) {
        int y = i*32 + 24;
        if (i == subOption) {
          u8g2.drawBox(0, y-16, 128, 32);
          u8g2.setDrawColor(0);
          u8g2.drawStr(2, y, opts[i]);
          u8g2.setDrawColor(1);
        } else {
          u8g2.drawStr(2, y, opts[i]);
        }
      }
    }
  }

  u8g2.sendBuffer();
}

void sendProtocol(uint8_t menuId, uint8_t opt, uint8_t page) {
  const char* payload = nullptr;
  if (menuId == 4) {
    // AC
    if (page == 0) { // Switch
      payload = (opt == 0) ? "AIR_ON" : "AIR_OFF"; beep();
    } else {        // Temp
      payload = (opt == 0) ? "AIR_UP"   : "AIR_DOWN"; beep();
    }
  } else {
    // 保留原有其他菜单协议
    switch (menuId) {
      case 1: payload = (opt == 0) ? "LED_ON"  : "LED_OFF"; beep(); break;
      case 2: payload = (opt == 0) ? "DELAY_ON": "DELAY_OFF"; beep(); break;
      case 3: payload = (opt == 0) ? "FAN_ON"  : "FAN_OFF"; beep(); break;
    }
  }
  if (payload) {
    udp.beginPacket("192.168.4.255", udpPort);
    udp.print(payload);
    udp.endPacket();
    Serial.println(payload);
  }
}

void beep() {
  digitalWrite(BUZZER_PIN, HIGH);
  Serial.println("beep");
  delay(100); // 响 100 毫秒
  digitalWrite(BUZZER_PIN, LOW);
}
