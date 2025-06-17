#include "FT6236.h"

bool FT6236::begin(uint16_t width, uint16_t height) {
  _width = width;
  _height = height;
  Wire.beginTransmission(FT6236_ADDR);
  return (Wire.endTransmission() == 0);
}

bool FT6236::touched() {
  Wire.beginTransmission(FT6236_ADDR);
  Wire.write(FT6236_REG_NUM_TOUCHES);
  if (Wire.endTransmission() != 0) return false;

  Wire.requestFrom(FT6236_ADDR, 1);
  if (Wire.available()) {
    uint8_t touches = Wire.read();
    return (touches == 1 || touches == 2);
  }
  return false;
}

TS_Point FT6236::getPoint() {
  Wire.beginTransmission(FT6236_ADDR);
  Wire.write(FT6236_REG_DATA);
  Wire.endTransmission();

  Wire.requestFrom(FT6236_ADDR, 4);
  uint8_t data[4];
  for (int i = 0; i < 4 && Wire.available(); i++) {
    data[i] = Wire.read();
  }

  uint16_t x = ((data[0] & 0x0F) << 8) | data[1];
  uint16_t y = ((data[2] & 0x0F) << 8) | data[3];

  if (x > _width) x = _width;
  if (y > _height) y = _height;

  _point.x = x;
  _point.y = y;
  return _point;
}

