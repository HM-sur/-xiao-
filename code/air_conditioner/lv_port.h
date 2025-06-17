#ifndef LV_PORT_H
#define LV_PORT_H

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include "FT6236.h"

#define I2C_SDA 6
#define I2C_SCL 7

TFT_eSPI tft = TFT_eSPI();
FT6236 ts = FT6236();
lv_disp_draw_buf_t draw_buf;
lv_color_t buf[240 * 10];

void my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        data->state = LV_INDEV_STATE_PR;
        data->point.x = p.x;
        data->point.y = p.y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void lv_port_init() {
    tft.begin();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, sizeof(buf) / sizeof(lv_color_t));

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = [](lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
        tft.startWrite();
        tft.setAddrWindow(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1);
        tft.pushColors((uint16_t *)color_p, (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1), true);
        tft.endWrite();
        lv_disp_flush_ready(disp);
    };
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    Wire.begin(I2C_SDA, I2C_SCL);
    ts.begin(240, 240);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

#endif

