#include "Sensors.h"

#include <math.h>

#include "pins.h"
#include "storage/Config.h"

namespace sensors {
namespace {
constexpr unsigned long VCC_SAMPLE_INTERVAL_MS = 1000;

constexpr float PH_REFERENCE_VALUES[3] = {4.01f, 6.86f, 9.18f};

float computeMedian(const uint16_t* window, uint8_t count) {
  if (count == 0) {
    return 0.0f;
  }
  uint16_t sorted[MEDIAN_WINDOW];
  for (uint8_t i = 0; i < count; ++i) {
    sorted[i] = window[i];
  }
  for (uint8_t i = 0; i < count; ++i) {
    for (uint8_t j = i + 1; j < count; ++j) {
      if (sorted[j] < sorted[i]) {
        uint16_t tmp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = tmp;
      }
    }
  }
  if ((count & 0x01) != 0) {
    return static_cast<float>(sorted[count / 2]);
  }
  return (static_cast<float>(sorted[count / 2 - 1]) +
          static_cast<float>(sorted[count / 2])) *
         0.5f;
}

float applyEma(float value, float ema, bool* initialised) {
  if (!*initialised) {
    *initialised = true;
    return value;
  }
  return EMA_ALPHA * value + (1.0f - EMA_ALPHA) * ema;
}

}  // namespace

uint32_t readVcc_mV() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delayMicroseconds(10);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC)) {
  }
  uint16_t result = ADC;
  if (result == 0) {
    return 0;
  }
  constexpr uint32_t CALIBRATED_CONST = 1125300UL;
  return CALIBRATED_CONST / result;
}

void VccMeter::begin() { last_sample_ = 0; }

void VccMeter::update(unsigned long now) {
  if (now - last_sample_ < VCC_SAMPLE_INTERVAL_MS) {
    return;
  }
  last_sample_ = now;
  uint32_t vcc = readVcc_mV();
  if (vcc == 0) {
    return;
  }
  has_reading_ = true;
  vcc_millivolts_ = static_cast<uint16_t>(vcc);
  float volts = static_cast<float>(vcc) / 1000.0f;
  ema_ = applyEma(volts, ema_, &ema_initialized_);
  vcc_volts_ = ema_;
}

void PhSensor::begin() {
  loadCalibration();
  resetCalibrationCaptures();
}

void PhSensor::update(unsigned long now, float vcc_volts) {
#ifdef SIMULATION
  if (simulated_mode_) {
    last_sample_ = now;
    last_report_ = now;
    updateSimulatedVoltage();
    latest_ph_ = simulated_ph_;
    has_reading_ = true;
    return;
  }
#endif
  if (now - last_sample_ >= RAW_SAMPLE_INTERVAL_MS) {
    last_sample_ = now;
    uint16_t raw = analogRead(PH_SENSOR_PIN);
    raw_window_[window_index_] = raw;
    window_index_ = (window_index_ + 1) % MEDIAN_WINDOW;
    if (window_count_ < MEDIAN_WINDOW) {
      ++window_count_;
    }
    float median_raw = computeMedian(raw_window_, window_count_);
    float voltage = median_raw * vcc_volts / 1023.0f;
    filtered_voltage_ = applyEma(voltage, filtered_voltage_, &ema_initialized_);
  }

  if (now - last_report_ >= REPORT_INTERVAL_MS) {
    last_report_ = now;
    latest_ph_ = gain_ * filtered_voltage_ + offset_;
    has_reading_ = true;
  }
}

CommandResult PhSensor::handleCommand(const String& command, String& response) {
  String trimmed = command;
  trimmed.trim();
  if (!trimmed.startsWith("CAL_PH")) {
    return CommandResult::kNotHandled;
  }

  String payload = trimmed.substring(6);
  payload.trim();
  String payload_upper = payload;
  payload_upper.toUpperCase();

  if (payload_upper == "START") {
    calibration_mode_ = true;
    resetCalibrationCaptures();
    response = F("CAL_PH: enter calibration mode");
    return CommandResult::kOk;
  }

  if (payload_upper == "ABORT") {
    calibration_mode_ = false;
    resetCalibrationCaptures();
    response = F("CAL_PH: abort");
    return CommandResult::kOk;
  }

  if (payload_upper == "READ") {
    String status;
    status.reserve(128);
    status += F("PH a=");
    status += String(gain_, 4);
    status += F(" b=");
    status += String(offset_, 4);
    status += F(" calibrated=");
    status += calibration_valid_ ? F("yes") : F("no");
    status += '\n';
    if (!calibration_valid_) {
      status += F("Warning: default pH calibration in use\n");
    }
    for (uint8_t i = 0; i < 3; ++i) {
      status += F("ref ");
      status += String(PH_REFERENCE_VALUES[i], 2);
      status += F(": ");
      if (stored_valid_[i]) {
        status += String(stored_voltages_[i], 4);
      } else {
        status += F("--");
      }
      status += '\n';
    }
    response = status;
    return CommandResult::kOk;
  }

  if (!calibration_mode_) {
    response = F("CAL_PH: not in calibration mode");
    return CommandResult::kError;
  }

  if (payload_upper.startsWith("POINT")) {
    int space_index = payload.indexOf(' ');
    if (space_index < 0) {
      response = F("CAL_PH: missing reference");
      return CommandResult::kError;
    }
    String argument = payload.substring(space_index + 1);
    argument.trim();
    float reference = argument.toFloat();
    int index = referenceIndexFor(reference);
    if (index < 0) {
      response = F("CAL_PH: invalid reference");
      return CommandResult::kError;
    }
    if (!has_reading_) {
      response = F("CAL_PH: no measurement available");
      return CommandResult::kError;
    }
    working_voltages_[index] = filtered_voltage_;
    working_valid_[index] = true;
    response = F("CAL_PH: captured V=");
    response += String(filtered_voltage_, 4);
    return CommandResult::kOk;
  }

  if (payload_upper == "SAVE") {
    if (!working_valid_[0] || !working_valid_[1] || !working_valid_[2]) {
      response = F("CAL_PH: need all 3 points");
      return CommandResult::kError;
    }
    updateRegression();
    for (uint8_t i = 0; i < 3; ++i) {
      stored_voltages_[i] = working_voltages_[i];
      stored_valid_[i] = true;
    }
    saveCalibration();
    calibration_mode_ = false;
    resetCalibrationCaptures();
    response = F("CAL_PH: saved a=");
    response += String(gain_, 4);
    response += F(" b=");
    response += String(offset_, 4);
    return CommandResult::kOk;
  }

  response = F("CAL_PH: unknown command");
  return CommandResult::kError;
}

void PhSensor::printStatus(Stream& response) const {
  response.print(F("PH a="));
  response.print(gain_, 4);
  response.print(F(" b="));
  response.print(offset_, 4);
  response.print(F(" calibrated="));
  response.println(calibration_valid_ ? F("yes") : F("no"));
  if (!calibration_valid_) {
    response.println(F("Warning: default pH calibration in use"));
  }
  for (uint8_t i = 0; i < 3; ++i) {
    response.print(F("ref "));
    response.print(PH_REFERENCE_VALUES[i], 2);
    response.print(F(": "));
    if (stored_valid_[i]) {
      response.println(stored_voltages_[i], 4);
    } else {
      response.println(F("--"));
    }
  }
  if (calibration_mode_) {
    response.println(F("Pending captures:"));
    for (uint8_t i = 0; i < 3; ++i) {
      response.print(PH_REFERENCE_VALUES[i], 2);
      response.print(F(": "));
      if (working_valid_[i]) {
        response.println(working_voltages_[i], 4);
      } else {
        response.println(F("--"));
      }
    }
  }
}

void PhSensor::loadCalibration() {
  storage::loadOrDefaults();
  const storage::Config& cfg = storage::config();
  gain_ = cfg.ph_gain;
  offset_ = cfg.ph_offset;
  for (uint8_t i = 0; i < 3; ++i) {
    stored_voltages_[i] = cfg.ph_reference_voltages[i];
    stored_valid_[i] = (cfg.ph_valid_mask >> i) & 0x01;
  }
  calibration_valid_ = (cfg.ph_valid_mask & 0x07) == 0x07;
}

void PhSensor::saveCalibration() {
  storage::Config& cfg = storage::config();
  cfg.ph_gain = gain_;
  cfg.ph_offset = offset_;
  cfg.ph_valid_mask = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    cfg.ph_reference_voltages[i] = stored_voltages_[i];
    if (stored_valid_[i]) {
      cfg.ph_valid_mask |= (1 << i);
    }
  }
  storage::save();
  calibration_valid_ = (cfg.ph_valid_mask & 0x07) == 0x07;
}

void PhSensor::resetCalibrationCaptures() {
  for (uint8_t i = 0; i < 3; ++i) {
    working_voltages_[i] = NAN;
    working_valid_[i] = false;
  }
}

int PhSensor::referenceIndexFor(float reference) const {
  for (uint8_t i = 0; i < 3; ++i) {
    if (fabs(reference - PH_REFERENCE_VALUES[i]) < 0.05f) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void PhSensor::updateRegression() {
  float sum_x = 0.0f;
  float sum_y = 0.0f;
  float sum_xy = 0.0f;
  float sum_x2 = 0.0f;
  for (uint8_t i = 0; i < 3; ++i) {
    if (!working_valid_[i]) {
      return;
    }
    float x = working_voltages_[i];
    float y = PH_REFERENCE_VALUES[i];
    sum_x += x;
    sum_y += y;
    sum_xy += x * y;
    sum_x2 += x * x;
  }
  float n = 3.0f;
  float denom = (n * sum_x2) - (sum_x * sum_x);
  if (fabs(denom) < 1e-6f) {
    return;
  }
  gain_ = ((n * sum_xy) - (sum_x * sum_y)) / denom;
  offset_ = (sum_y - gain_ * sum_x) / n;
}

#ifdef SIMULATION
void PhSensor::setSimulatedPh(float ph) {
  if (!isfinite(ph)) {
    simulated_mode_ = false;
    return;
  }
  simulated_mode_ = true;
  simulated_ph_ = ph;
  updateSimulatedVoltage();
  latest_ph_ = ph;
  has_reading_ = true;
}

void PhSensor::updateSimulatedVoltage() {
  float effective_gain = fabs(gain_) < 1e-3f ? -5.70f : gain_;
  float voltage = (simulated_ph_ - offset_) / effective_gain;
  if (!isfinite(voltage)) {
    voltage = 0.0f;
  }
  filtered_voltage_ = voltage;
  ema_initialized_ = true;
}
#endif

void EcSensor::begin() {
  loadConfiguration();
  pending_k_ = k_value_;
  has_pending_k_ = false;
}

void EcSensor::update(unsigned long now, float vcc_volts) {
#ifdef SIMULATION
  if (simulated_mode_) {
    last_sample_ = now;
    last_report_ = now;
    latest_ec_ms_ = simulated_ec_ms_;
    latest_ec25_ms_ = simulated_ec_ms_;
    latest_tds_ppm_ = simulated_tds_ppm_;
    has_reading_ = true;
    return;
  }
#endif
  if (now - last_sample_ >= RAW_SAMPLE_INTERVAL_MS) {
    last_sample_ = now;
    uint16_t raw = analogRead(TDS_SENSOR_PIN);
    raw_window_[window_index_] = raw;
    window_index_ = (window_index_ + 1) % MEDIAN_WINDOW;
    if (window_count_ < MEDIAN_WINDOW) {
      ++window_count_;
    }
    float median_raw = computeMedian(raw_window_, window_count_);
    float voltage = median_raw * vcc_volts / 1023.0f;
    filtered_voltage_ = applyEma(voltage, filtered_voltage_, &ema_initialized_);
  }

  if (now - last_report_ >= REPORT_INTERVAL_MS) {
    last_report_ = now;
    float voltage = filtered_voltage_;
    float ec_raw = (133.42f * voltage * voltage * voltage -
                    255.86f * voltage * voltage + 857.39f * voltage);
    ec_raw *= k_value_;
    latest_ec_ms_ = ec_raw / 1000.0f;
    float compensation = 1.0f + alpha_ * (temperature_c_ - 25.0f);
    if (compensation <= 0.0f) {
      compensation = 1.0f;
    }
    latest_ec25_ms_ = latest_ec_ms_ / compensation;
    latest_tds_ppm_ = latest_ec25_ms_ * static_cast<float>(tds_factor_);
    has_reading_ = true;
  }
}

void EcSensor::setTemperatureC(float temperature) {
  temperature_c_ = temperature;
  temperature_valid_ = true;
}

void EcSensor::setAlpha(float alpha) {
  if (!isfinite(alpha) || alpha < 0.0f) {
    alpha = storage::config().ec_alpha;
  }
  alpha_ = constrain(alpha, 0.0f, 0.2f);
  saveConfiguration();
}

void EcSensor::setTdsFactor(int factor) {
  if (factor <= 0) {
    return;
  }
  tds_factor_ = factor;
  saveConfiguration();
}

void EcSensor::setK(float k_value) {
  if (k_value <= 0.0f) {
    return;
  }
  k_value_ = k_value;
  pending_k_ = k_value_;
  has_pending_k_ = false;
  saveConfiguration();
}

CommandResult EcSensor::handleCommand(const String& command, String& response) {
  if (command.startsWith("SET_T ")) {
    float value = command.substring(6).toFloat();
    setTemperatureC(value);
    response = F("Temperature set to ");
    response += String(temperature_c_, 2);
    return CommandResult::kOk;
  }
  if (command.startsWith("SET_EC_ALPHA ")) {
    float value = command.substring(13).toFloat();
    setAlpha(value);
    response = F("EC alpha set to ");
    response += String(alpha_, 4);
    return CommandResult::kOk;
  }
  if (command.startsWith("SET_TDSFACTOR ")) {
    int value = command.substring(14).toInt();
    setTdsFactor(value);
    response = F("TDS factor set to ");
    response += String(tds_factor_);
    return CommandResult::kOk;
  }
  if (command.startsWith("SET_K ")) {
    float value = command.substring(6).toFloat();
    setK(value);
    response = F("Cell constant set to ");
    response += String(k_value_, 3);
    return CommandResult::kOk;
  }
  if (command == "CAL_EC START") {
    calibration_mode_ = true;
    pending_k_ = k_value_;
    has_pending_k_ = true;
    response = F("CAL_EC: enter calibration mode");
    return CommandResult::kOk;
  }
  if (command.startsWith("CAL_EC POINT ")) {
    if (!calibration_mode_) {
      response = F("CAL_EC: not in calibration mode");
      return CommandResult::kError;
    }
    float reference = command.substring(13).toFloat();
    if (!has_reading_ || latest_ec25_ms_ <= 0.0f) {
      response = F("CAL_EC: no measurement available");
      return CommandResult::kError;
    }
    if (reference <= 0.0f) {
      response = F("CAL_EC: invalid reference");
      return CommandResult::kError;
    }
    float measured = latest_ec25_ms_;
    pending_k_ *= reference / measured;
    has_pending_k_ = true;
    response = F("CAL_EC: provisional K=");
    response += String(pending_k_, 4);
    return CommandResult::kOk;
  }
  if (command == "CAL_EC SAVE") {
    if (!calibration_mode_) {
      response = F("CAL_EC: not in calibration mode");
      return CommandResult::kError;
    }
    if (has_pending_k_) {
      k_value_ = pending_k_;
      saveConfiguration();
    }
    calibration_mode_ = false;
    has_pending_k_ = false;
    response = F("CAL_EC: saved K=");
    response += String(k_value_, 4);
    return CommandResult::kOk;
  }
  if (command == "CAL_EC ABORT") {
    calibration_mode_ = false;
    pending_k_ = k_value_;
    has_pending_k_ = false;
    response = F("CAL_EC: abort");
    return CommandResult::kOk;
  }
  if (command == "CAL_EC READ") {
    String status;
    status.reserve(96);
    status += F("EC K=");
    status += String(k_value_, 4);
    status += F(" alpha=");
    status += String(alpha_, 4);
    status += F(" tdsFactor=");
    status += String(tds_factor_);
    if (calibration_mode_ && has_pending_k_) {
      status += F("\nPending K=");
      status += String(pending_k_, 4);
    }
    response = status;
    return CommandResult::kOk;
  }
  response = F("EC: command not recognized");
  return CommandResult::kNotHandled;
}

void EcSensor::printStatus(Stream& response) const {
  response.print(F("EC K="));
  response.print(k_value_, 4);
  response.print(F(" alpha="));
  response.print(alpha_, 4);
  response.print(F(" tdsFactor="));
  response.println(tds_factor_);
  if (calibration_mode_ && has_pending_k_) {
    response.print(F("Pending K="));
    response.println(pending_k_, 4);
  }
}

void EcSensor::loadConfiguration() {
  storage::loadOrDefaults();
  const storage::Config& cfg = storage::config();
  k_value_ = cfg.ec_k_cell;
  alpha_ = cfg.ec_alpha;
  tds_factor_ = static_cast<int>(cfg.ec_tds_factor);
}

void EcSensor::saveConfiguration() {
  storage::Config& cfg = storage::config();
  cfg.ec_k_cell = k_value_;
  cfg.ec_alpha = alpha_;
  cfg.ec_tds_factor = tds_factor_;
  storage::save();
}

#ifdef SIMULATION
void EcSensor::setSimulatedSolution(float ec_ms, float tds_ppm) {
  if (!isfinite(ec_ms) || ec_ms < 0.0f) {
    simulated_mode_ = false;
    return;
  }
  simulated_mode_ = true;
  simulated_ec_ms_ = ec_ms;
  simulated_tds_ppm_ = isfinite(tds_ppm) ? tds_ppm : ec_ms * static_cast<float>(tds_factor_);
  latest_ec_ms_ = simulated_ec_ms_;
  latest_ec25_ms_ = simulated_ec_ms_;
  latest_tds_ppm_ = simulated_tds_ppm_;
  has_reading_ = true;
}
#endif

SensorPollingManager::SensorPollingManager(VccMeter& vcc_meter, PhSensor& ph_sensor,
                                           EcSensor& ec_sensor)
    : vcc_meter_(vcc_meter), ph_sensor_(ph_sensor), ec_sensor_(ec_sensor) {}

void SensorPollingManager::begin() {
  vcc_meter_.begin();
  ph_sensor_.begin();
  ec_sensor_.begin();
}

void SensorPollingManager::update() {
  unsigned long now = millis();
  vcc_meter_.update(now);
  float vcc = vcc_meter_.hasReading() ? vcc_meter_.getVccVolts() : 5.0f;
  ph_sensor_.update(now, vcc);
  ec_sensor_.update(now, vcc);
}

}  // namespace sensors

