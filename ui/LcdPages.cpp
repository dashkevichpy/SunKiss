#include "LcdPages.h"

namespace ui {
namespace {
constexpr unsigned long PAGE_INTERVAL_MS = 5000;
constexpr uint8_t TOTAL_PAGES = 3;

String formatFloat(float value, uint8_t digits) {
  String s = String(value, digits);
  s.trim();
  return s;
}

String formatInt(int value) { return String(value); }

}  // namespace

LcdPages::LcdPages(LiquidCrystal_I2C& lcd, sensors::PhSensor& ph_sensor,
                   sensors::EcSensor& ec_sensor, sensors::VccMeter& vcc_meter)
    : lcd_(lcd), ph_(ph_sensor), ec_(ec_sensor), vcc_(vcc_meter) {
  cached_lines_[0] = String();
  cached_lines_[1] = String();
}

void LcdPages::begin() {
  lcd_.init();
  lcd_.backlight();
  forceRefresh();
}

void LcdPages::update() {
  unsigned long now = millis();
  if (now - last_page_switch_ >= PAGE_INTERVAL_MS) {
    current_page_ = (current_page_ + 1) % TOTAL_PAGES;
    last_page_switch_ = now;
    forceRefresh();
  } else {
    renderPage();
  }
}

void LcdPages::forceRefresh() {
  cached_lines_[0] = String();
  cached_lines_[1] = String();
  renderPage(true);
}

void LcdPages::renderPage(bool force) {
  switch (current_page_) {
    case 0:
      renderPhEcPage(force);
      break;
    case 1:
      renderTemperaturePage(force);
      break;
    case 2:
    default:
      renderTdsPage(force);
      break;
  }
}

void LcdPages::renderPhEcPage(bool force) {
  String line0 = F("pH:");
  if (ph_.hasReading()) {
    line0 += formatFloat(ph_.getPh(), 2);
  } else {
    line0 += F("--");
  }

  String line1 = F("EC:");
  if (ec_.hasReading()) {
    line1 += formatFloat(ec_.getEc25MilliSiemens(), 2);
    line1 += F("mS/cm");
  } else {
    line1 += F("--");
  }

  writeLine(0, line0, force);
  writeLine(1, line1, force);
}

void LcdPages::renderTemperaturePage(bool force) {
  String line0 = F("T:");
  if (ec_.hasExternalTemperature()) {
    line0 += formatFloat(ec_.getTemperatureC(), 1);
    line0 += F("C");
  } else {
    line0 += F("--");
  }

  String line1 = F("Vcc:");
  if (vcc_.hasReading()) {
    line1 += formatFloat(vcc_.getVccVolts(), 2);
    line1 += F("V");
  } else {
    line1 += F("--");
  }

  writeLine(0, line0, force);
  writeLine(1, line1, force);
}

void LcdPages::renderTdsPage(bool force) {
  String line0 = F("TDS:");
  if (ec_.hasReading()) {
    line0 += formatInt(static_cast<int>(ec_.getTdsPpm() + 0.5f));
    line0 += F("ppm");
  } else {
    line0 += F("--");
  }

  String line1 = F("K:");
  line1 += formatFloat(ec_.getCellConstant(), 2);
  line1 += F(" F:");
  line1 += formatInt(ec_.getTdsFactor());

  writeLine(0, line0, force);
  writeLine(1, line1, force);
}

void LcdPages::writeLine(uint8_t row, const String& text, bool force) {
  String padded = padLine(text);
  if (!force && cached_lines_[row] == padded) {
    return;
  }
  lcd_.setCursor(0, row);
  lcd_.print(padded);
  cached_lines_[row] = padded;
}

String LcdPages::padLine(const String& text) const {
  String output = text;
  if (output.length() > 16) {
    output = output.substring(0, 16);
  }
  while (output.length() < 16) {
    output += ' ';
  }
  return output;
}

}  // namespace ui

