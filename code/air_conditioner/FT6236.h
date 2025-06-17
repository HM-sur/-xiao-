#ifndef FT6236_H
#define FT6236_H

#include <Arduino.h>
#include <Wire.h>

#define FT6236_ADDR 0x38
#define FT6236_REG_NUM_TOUCHES 0x02
#define FT6236_REG_DATA 0x03

typedef struct {
  uint16_t x;
  uint16_t y;
} TS_Point;

class FT6236 {
  public:
    FT6236() {}
    bool begin(uint16_t width = 240, uint16_t height = 240);
    bool touched();
    TS_Point getPoint();

  private:
    uint16_t _width, _height;
    TS_Point _point;
};

#endif

