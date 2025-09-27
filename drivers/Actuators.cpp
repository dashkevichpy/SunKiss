#include "Actuators.h"

#include "storage/Config.h"

namespace {
uint8_t relayLevel(bool on) {
  if (storage::config().relay_active_low) {
    return on ? LOW : HIGH;
  }
  return on ? HIGH : LOW;
}

void writeDigital(uint8_t pin, uint8_t level) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, level);
}
}  // namespace

void pumpsAllOff() {
  setMixPump1(false);
  setMixPump2(false);
  setPumpPhDown(false);
  setPumpPhUp(false);
  setPumpA(false);
  setPumpB(false);
}

void setMixPump1(bool on) {
  writeDigital(MIX_PUMP_1_PIN, relayLevel(on));
}

void setMixPump2(bool on) {
  writeDigital(MIX_PUMP_2_PIN, relayLevel(on));
}

void setPumpPhDown(bool on) {
  writeDigital(PUMP_PH_DOWN_PIN, on ? HIGH : LOW);
}

void setPumpPhUp(bool on) {
  writeDigital(PUMP_PH_UP_PIN, on ? HIGH : LOW);
}

void setPumpA(bool on) {
  writeDigital(PUMP_A_PIN, on ? HIGH : LOW);
}

void setPumpB(bool on) {
  writeDigital(PUMP_B_PIN, on ? HIGH : LOW);
}
