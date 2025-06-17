#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// === WiFi & UDP 配置 ===
const char* ssid = "XIAO_AP";
const char* password = "12345678";

WiFiUDP udp;
const uint16_t udpPort = 1234;
char incomingPacket[255];

// === 屏幕和 LVGL 缓冲 ===
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[240 * 10];
static TFT_eSPI tft = TFT_eSPI();

// === 状态变量 ===
bool acOn = false;
int temperature = 25;

// === LVGL 控件 ===
lv_obj_t *labelStatus;
lv_obj_t *labelTemp;
lv_obj_t *btnOnOff;
lv_obj_t *btnUp;
lv_obj_t *btnDown;

// === 按钮事件处理 ===
void btn_event_cb(lv_event_t *e) {
  lv_obj_t *btn = lv_event_get_target(e);

  if (btn == btnOnOff) {
    acOn = !acOn;
    lv_label_set_text(labelStatus, acOn ? "Status: ON" : "Status: OFF");
    lv_label_set_text_fmt(labelTemp, "Temp: %d°C", temperature);
  } else if (acOn && btn == btnUp) {
    temperature++;
    lv_label_set_text_fmt(labelTemp, "Temp: %d°C", temperature);
  } else if (acOn && btn == btnDown) {
    temperature--;
    lv_label_set_text_fmt(labelTemp, "Temp: %d°C", temperature);
  }
}

// === UI 初始化 ===
void ui_init() {
  labelStatus = lv_label_create(lv_scr_act());
  lv_label_set_text(labelStatus, "Status: OFF");
  lv_obj_align(labelStatus, LV_ALIGN_TOP_MID, 0, 20);

  labelTemp = lv_label_create(lv_scr_act());
  lv_label_set_text(labelTemp, "Temp: 25°C");
  lv_obj_align(labelTemp, LV_ALIGN_CENTER, 0, 0);

  btnOnOff = lv_btn_create(lv_scr_act());
  lv_obj_align(btnOnOff, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_event_cb(btnOnOff, btn_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *label1 = lv_label_create(btnOnOff);
  lv_label_set_text(label1, "On/Off");

  btnUp = lv_btn_create(lv_scr_act());
  lv_obj_align(btnUp, LV_ALIGN_RIGHT_MID, -30, -20);
  lv_obj_add_event_cb(btnUp, btn_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *label2 = lv_label_create(btnUp);
  lv_label_set_text(label2, "+");

  btnDown = lv_btn_create(lv_scr_act());
  lv_obj_align(btnDown, LV_ALIGN_RIGHT_MID, -30, 30);
  lv_obj_add_event_cb(btnDown, btn_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *label3 = lv_label_create(btnDown);
  lv_label_set_text(label3, "-");
}

// === 屏幕刷新回调 ===
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

void setup() {
  Serial.begin(115200);

  // === 连接 WiFi ===
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n连接成功，IP 地址:");
  Serial.println(WiFi.localIP());

  udp.begin(udpPort);

  // === 初始化屏幕和 LVGL ===
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, 240 * 10);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.hor_res = 240;
  disp_drv.ver_res = 240;
  lv_disp_drv_register(&disp_drv);

  // === 加载界面 ===
  ui_init();
}

void loop() {
  // === 处理UDP数据 ===
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = 0;

    Serial.print("收到UDP数据：");
    Serial.println(incomingPacket);

    // === 控制逻辑 ===
    if (strcmp(incomingPacket, "AIR_ON") == 0) {
      acOn = true;
      lv_label_set_text(labelStatus, "Status: ON");
      lv_label_set_text_fmt(labelTemp, "Temp: %d°C", temperature);
    } else if (strcmp(incomingPacket, "AIR_OFF") == 0) {
      acOn = false;
      lv_label_set_text(labelStatus, "Status: OFF");
      lv_label_set_text_fmt(labelTemp, "Temp: %d°C", temperature);
    } else if (strcmp(incomingPacket, "AIR_UP") == 0 && acOn && temperature < 30) {
      temperature++;
      lv_label_set_text_fmt(labelTemp, "Temp: %d°C", temperature);
    } else if (strcmp(incomingPacket, "AIR_DOWN") == 0 && acOn && temperature > 16) {
      temperature--;
      lv_label_set_text_fmt(labelTemp, "Temp: %d°C", temperature);
    } else {
      // 未识别命令，仅串口输出
      Serial.println("无效命令，未处理");
    }
  }

  // === 刷新界面 ===
  lv_timer_handler();
  delay(5);
}