#pragma once

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

#include "../sensors/Sensors.h"

namespace ui {

class LcdPages {
 public:
  LcdPages(LiquidCrystal_I2C& lcd, sensors::PhSensor& ph_sensor,
           sensors::EcSensor& ec_sensor, sensors::VccMeter& vcc_meter);

  void begin();
  void update();
  void forceRefresh();

 private:
  void renderPage(bool force = false);
  void renderPhEcPage(bool force);
  void renderTemperaturePage(bool force);
  void renderTdsPage(bool force);
  void writeLine(uint8_t row, const String& text, bool force);
  String padLine(const String& text) const;

  LiquidCrystal_I2C& lcd_;
  sensors::PhSensor& ph_;
  sensors::EcSensor& ec_;
  sensors::VccMeter& vcc_;

  unsigned long last_page_switch_{0};
  uint8_t current_page_{0};
  String cached_lines_[2];
};

}  // namespace ui

