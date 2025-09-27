#pragma once

#include <Arduino.h>

#include "drivers/Actuators.h"
#include "sensors/Sensors.h"
#include "storage/Config.h"

namespace process {

enum class State : uint8_t {
  kIdle = 0,
  kMix,
  kPhCoarse,
  kPhFine,
  kFertA,
  kFertB,
  kDone,
  kFault,
};

enum class PumpChannel : uint8_t {
  kPhDown = 0,
  kPhUp,
  kFertA,
  kFertB,
};

struct DosingLog {
  float ph_down_ml{0.0f};
  float ph_up_ml{0.0f};
  float fert_a_ml{0.0f};
  float fert_b_ml{0.0f};
};

class Process {
 public:
  Process(sensors::PhSensor& ph_sensor, sensors::EcSensor& ec_sensor);

  void begin();

  void start(float batch_volume_l, float target_ph, float dose_a_ml_per_l,
             float dose_b_ml_per_l);
  void abort(const String& reason);

  void onSensorTick();
  void onPumpFinished(PumpChannel channel);
  void onTimer();

  State state() const { return state_; }
  bool isActive() const { return state_ != State::kIdle && state_ != State::kDone && state_ != State::kFault; }

  const DosingLog& log() const { return log_; }
  float targetPh() const { return target_ph_; }
  float batchVolumeLiters() const { return batch_volume_l_; }

  void setCoarseThreshold(float threshold);
  void setFineThreshold(float threshold);
  void setDoseGainUp(float gain);
  void setDoseGainDown(float gain);
  void setFineDoseScale(float scale);
  void setMaxPhDosePerPulsePerLiter(float ml);
  void setMaxPhTotalPerLiter(float ml);
  void setProcessTimeoutMs(unsigned long timeout_ms);
  void requireTemperature(bool required) { require_temperature_ = required; }

  void setPumpCalibration(PumpChannel channel, float ml_per_sec);
  float getPumpCalibration(PumpChannel channel) const;

  bool hasPendingTimer() const { return timer_.pending; }
  unsigned long nextTimerDueMs() const { return timer_.due_ms; }
  String faultReason() const { return fault_reason_; }

 private:
  enum class TimerReason : uint8_t {
    kNone = 0,
    kCoarsePause,
    kFinePause,
    kFertPause,
    kPumpRun,
    kProcessTimeout,
  };

  struct TimerState {
    TimerReason reason{TimerReason::kNone};
    unsigned long due_ms{0};
    bool pending{false};
  };

  struct PumpState {
    bool active{false};
    PumpChannel channel{PumpChannel::kPhDown};
    float volume_ml{0.0f};
    unsigned long started_ms{0};
    unsigned long duration_ms{0};
  };

  struct PumpCalibrationData {
    uint16_t signature;
    float ml_per_sec[4];
  };

  static constexpr uint16_t PUMP_SIGNATURE = 0x504D;  // "PM"
  static constexpr float DEFAULT_PUMP_RATES[4] = {1.0f, 1.0f, 3.0f, 3.0f};

  void resetState();
  void transitionTo(State new_state);
  void enterMixState();
  void enterPhCoarse();
  void enterPhFine();
  void enterFertA();
  void enterFertB();
  void enterDone();
  void enterFault(const String& reason);

  void scheduleTimer(TimerReason reason, unsigned long delay_ms);
  void clearTimer();
  void handleTimer(TimerReason reason);

  bool startPumpDose(PumpChannel channel, float volume_ml);
  void stopActivePump();
  float pumpRateFor(PumpChannel channel) const;
  void applyPumpOutput(PumpChannel channel, bool on);

  float computePhDoseVolume(float delta_ph) const;
  void handlePhMeasurement();
  void updateAdaptiveGain(float current_ph);

  void loadFromConfig();
  void logStateTransition(State from, State to);
  void logPumpChange(PumpChannel channel, bool on, float volume_ml, unsigned long duration_ms);
  void logMixPump(uint8_t index, bool on);
  void logFaultEvent(const String& reason);
  const __FlashStringHelper* stateName(State state) const;
  const __FlashStringHelper* pumpChannelName(PumpChannel channel) const;

#ifdef SIMULATION
  void updateSimulationContext();
#endif

  float maxPerPulseMl() const { return max_pH_dose_per_pulse_per_liter_ * batch_volume_l_; }
  float maxTotalMl() const { return max_pH_total_ml_per_liter_ * batch_volume_l_; }

  unsigned long millisNow() const { return millis(); }

  sensors::PhSensor& ph_sensor_;
  sensors::EcSensor& ec_sensor_;

  State state_{State::kIdle};
  unsigned long state_started_ms_{0};
  unsigned long process_started_ms_{0};
  unsigned long process_deadline_ms_{0};

  TimerState timer_{};
  PumpState pump_{};

  float batch_volume_l_{0.0f};
  float target_ph_{6.5f};
  float dose_a_ml_per_l_{0.0f};
  float dose_b_ml_per_l_{0.0f};

  float dose_gain_up_ml_per_ph_per_l_{0.2f};
  float dose_gain_down_ml_per_ph_per_l_{0.2f};
  float fine_dose_scale_{0.25f};
  float coarse_threshold_{0.3f};
  float fine_threshold_{0.1f};
  float max_pH_dose_per_pulse_per_liter_{0.5f};
  float max_pH_total_ml_per_liter_{5.0f};
  unsigned long coarse_pause_ms_{60000UL};
  unsigned long fine_pause_ms_{180000UL};
  unsigned long fert_pause_ms_{180000UL};
  unsigned long process_timeout_ms_{3600000UL};

  float pump_ml_per_sec_[4];

  DosingLog log_{};
  bool mix_pumps_on_{false};

  float last_ph_{NAN};
  float last_ec_ms_{NAN};
  float last_tds_ppm_{NAN};
  float last_temp_c_{NAN};
  bool require_temperature_{false};

  bool waiting_for_sensor_{false};
  bool awaiting_gain_update_{false};
  float last_dose_reference_ph_{NAN};
  float last_requested_delta_ph_{0.0f};
  int last_dose_direction_{0};

  String fault_reason_;

  static constexpr float min_gain_ = 0.02f;
  static constexpr float max_gain_ = 2.0f;
};

}  // namespace process

