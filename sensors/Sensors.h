#pragma once

#include <Arduino.h>
#include <Stream.h>

#include "../storage/Config.h"

namespace sensors {

static constexpr uint8_t MEDIAN_WINDOW = 5;
static constexpr float EMA_ALPHA = 0.2f;
static constexpr unsigned long RAW_SAMPLE_INTERVAL_MS = 100;
static constexpr unsigned long REPORT_INTERVAL_MS = 500;

uint32_t readVcc_mV();

class VccMeter {
 public:
  void begin();
  void update(unsigned long now);
  bool hasReading() const { return has_reading_; }
  uint16_t getVccMilliVolts() const { return vcc_millivolts_; }
  float getVccVolts() const { return vcc_volts_; }

 private:
  unsigned long last_sample_{0};
  uint16_t vcc_millivolts_{5000};
  float vcc_volts_{5.0f};
  float ema_{0.0f};
  bool ema_initialized_{false};
  bool has_reading_{false};
};

enum class CommandResult : uint8_t {
  kNotHandled = 0,
  kOk,
  kError,
};

class PhSensor {
 public:
  void begin();
  void update(unsigned long now, float vcc_volts);

  float getPh() const { return latest_ph_; }
  float getVoltage() const { return filtered_voltage_; }
  bool hasReading() const { return has_reading_; }
  bool isCalibrated() const { return calibration_valid_; }

  CommandResult handleCommand(const String& command, String& response);
  void printStatus(Stream& response) const;

#ifdef SIMULATION
  void setSimulatedSolution(float ec_ms, float tds_ppm);
#endif

#ifdef SIMULATION
  void setSimulatedPh(float ph);
#endif

 private:
  void loadCalibration();
  void saveCalibration();
  void resetCalibrationCaptures();
  int referenceIndexFor(float reference) const;
  void updateRegression();
#ifdef SIMULATION
  void updateSimulatedVoltage();
#endif

  float gain_{-5.70f};
  float offset_{21.34f};
  bool calibration_valid_{false};

  uint16_t raw_window_[MEDIAN_WINDOW] = {0};
  uint8_t window_count_{0};
  uint8_t window_index_{0};
  float filtered_voltage_{0.0f};
  bool ema_initialized_{false};

  unsigned long last_sample_{0};
  unsigned long last_report_{0};
  float latest_ph_{0.0f};
  bool has_reading_{false};

  bool calibration_mode_{false};
  float stored_voltages_[3] = {NAN, NAN, NAN};
  bool stored_valid_[3] = {false, false, false};
  float working_voltages_[3] = {NAN, NAN, NAN};
  bool working_valid_[3] = {false, false, false};
#ifdef SIMULATION
  bool simulated_mode_{false};
  float simulated_ph_{7.0f};
#endif
};

class EcSensor {
 public:
  void begin();
  void update(unsigned long now, float vcc_volts);

  float getEcMilliSiemens() const { return latest_ec_ms_; }
  float getEc25MilliSiemens() const { return latest_ec25_ms_; }
  float getTdsPpm() const { return latest_tds_ppm_; }
  float getVoltage() const { return filtered_voltage_; }
  bool hasReading() const { return has_reading_; }

  float getTemperatureC() const { return temperature_c_; }
  bool hasExternalTemperature() const { return temperature_valid_; }
  float getAlpha() const { return alpha_; }
  float getCellConstant() const { return k_value_; }
  int getTdsFactor() const { return tds_factor_; }

  void setTemperatureC(float temperature);
  void setAlpha(float alpha);
  void setTdsFactor(int factor);
  void setK(float k_value);

  CommandResult handleCommand(const String& command, String& response);
  void printStatus(Stream& response) const;

 private:
  void loadConfiguration();
  void saveConfiguration();

  uint16_t raw_window_[MEDIAN_WINDOW] = {0};
  uint8_t window_count_{0};
  uint8_t window_index_{0};
  float filtered_voltage_{0.0f};
  bool ema_initialized_{false};

  unsigned long last_sample_{0};
  unsigned long last_report_{0};
  bool has_reading_{false};

  float latest_ec_ms_{0.0f};
  float latest_ec25_ms_{0.0f};
  float latest_tds_ppm_{0.0f};

  float temperature_c_{25.0f};
  bool temperature_valid_{false};
  float alpha_{0.02f};
  float k_value_{1.0f};
  int tds_factor_{500};

  bool calibration_mode_{false};
  float pending_k_{1.0f};
  bool has_pending_k_{false};
#ifdef SIMULATION
  bool simulated_mode_{false};
  float simulated_ec_ms_{0.0f};
  float simulated_tds_ppm_{0.0f};
#endif
};

class SensorPollingManager {
 public:
  SensorPollingManager(VccMeter& vcc_meter, PhSensor& ph_sensor, EcSensor& ec_sensor);

  void begin();
  void update();

  float getSupplyVolts() const { return vcc_meter_.getVccVolts(); }

 private:
  VccMeter& vcc_meter_;
  PhSensor& ph_sensor_;
  EcSensor& ec_sensor_;
};

}  // namespace sensors

