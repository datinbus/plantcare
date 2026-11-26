#pragma once

#include <Arduino.h>
#include <Wire.h>

class LiquidCrystal_I2C : public Print {
public:
  LiquidCrystal_I2C(uint8_t address, uint8_t columns, uint8_t rows)
      : _address(address), _columns(columns), _rows(rows), _backlight(0x08) {}

  void init() {
    Wire.begin();
    delay(50);
    _sendNibble(0x03);
    delay(5);
    _sendNibble(0x03);
    delayMicroseconds(150);
    _sendNibble(0x03);
    _sendNibble(0x02);
    _sendCommand(0x28);
    _sendCommand(0x0C);
    clear();
    _sendCommand(0x06);
  }

  void backlight() { _backlight = 0x08; }
  void noBacklight() { _backlight = 0x00; }

  void clear() {
    _sendCommand(0x01);
    delay(2);
  }

  void setCursor(uint8_t column, uint8_t row) {
    static const uint8_t rowOffsets[] = {0x00, 0x40, 0x14, 0x54};
    if (row >= _rows) row = _rows - 1;
    if (column >= _columns) column = _columns - 1;
    _sendCommand(0x80 | (column + rowOffsets[row]));
  }

  size_t write(uint8_t value) override {
    _send(value, 0x01);
    return 1;
  }

private:
  uint8_t _address;
  uint8_t _columns;
  uint8_t _rows;
  uint8_t _backlight;

  void _sendCommand(uint8_t value) { _send(value, 0x00); }

  void _send(uint8_t value, uint8_t mode) {
    _sendNibble((value >> 4) & 0x0F, mode);
    _sendNibble(value & 0x0F, mode);
  }

  void _sendNibble(uint8_t nibble, uint8_t mode = 0x00) {
    uint8_t data = (nibble << 4) | mode | _backlight;
    Wire.beginTransmission(_address);
    Wire.write(data);
    Wire.endTransmission();
    Wire.beginTransmission(_address);
    Wire.write(data | 0x04);
    Wire.endTransmission();
    delayMicroseconds(1);
    Wire.beginTransmission(_address);
    Wire.write(data);
    Wire.endTransmission();
  }
};
