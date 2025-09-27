#include "Process.h"

#include <math.h>

#include "storage/Config.h"

#ifdef SIMULATION
#include "sim/FluidSim.h"
#endif

namespace process {
namespace {

float constrainPositive(float value, float fallback) {
  return value > 0.0f ? value : fallback;
}

}  // namespace

Process::Process(sensors::PhSensor& ph_sensor, sensors::EcSensor& ec_sensor)
    : ph_sensor_(ph_sensor), ec_sensor_(ec_sensor) {
  for (uint8_t i = 0; i < 4; ++i) {
    pump_ml_per_sec_[i] = storage::config().pump_ml_per_sec[i];
  }
}

void Process::begin() {
  storage::loadOrDefaults();
  loadFromConfig();
  resetState();
}

void Process::resetState() {
  pumpsAllOff();
  mix_pumps_on_ = false;
  state_ = State::kIdle;
  state_started_ms_ = millisNow();
  process_started_ms_ = 0;
  process_deadline_ms_ = 0;
  clearTimer();
  pump_ = PumpState{};
  log_ = DosingLog{};
  waiting_for_sensor_ = false;
  awaiting_gain_update_ = false;
  last_dose_reference_ph_ = NAN;
  last_requested_delta_ph_ = 0.0f;
  last_dose_direction_ = 0;
  last_ph_ = NAN;
  last_ec_ms_ = NAN;
  last_tds_ppm_ = NAN;
  last_temp_c_ = NAN;
  fault_reason_ = String();
}

void Process::loadFromConfig() {
  const storage::Config& cfg = storage::config();
  coarse_threshold_ = cfg.coarse_threshold;
  fine_threshold_ = min(cfg.fine_threshold, coarse_threshold_);
  dose_gain_up_ml_per_ph_per_l_ = constrain(cfg.dose_gain_up, min_gain_, max_gain_);
  dose_gain_down_ml_per_ph_per_l_ = constrain(cfg.dose_gain_down, min_gain_, max_gain_);
  fine_dose_scale_ = constrain(cfg.fine_dose_scale, 0.05f, 1.0f);
  max_pH_dose_per_pulse_per_liter_ = max(0.0f, cfg.max_ph_dose_per_pulse_per_liter);
  max_pH_total_ml_per_liter_ = max(0.0f, cfg.max_ph_total_per_liter);
  coarse_pause_ms_ = cfg.coarse_pause_ms;
  fine_pause_ms_ = cfg.fine_pause_ms;
  fert_pause_ms_ = cfg.fert_pause_ms;
  process_timeout_ms_ = cfg.process_timeout_ms;
  for (uint8_t i = 0; i < 4; ++i) {
    pump_ml_per_sec_[i] = constrainPositive(cfg.pump_ml_per_sec[i], DEFAULT_PUMP_RATES[i]);
  }
#ifdef SIMULATION
  updateSimulationContext();
#endif
}

void Process::setCoarseThreshold(float threshold) {
  threshold = constrain(threshold, 0.05f, 2.0f);
  coarse_threshold_ = threshold;
  if (fine_threshold_ > coarse_threshold_) {
    fine_threshold_ = coarse_threshold_;
  }
  storage::Config& cfg = storage::config();
  cfg.coarse_threshold = coarse_threshold_;
  if (cfg.fine_threshold > coarse_threshold_) {
    cfg.fine_threshold = coarse_threshold_;
  }
  storage::save();
}

void Process::setFineThreshold(float threshold) {
  threshold = constrain(threshold, 0.01f, coarse_threshold_);
  fine_threshold_ = threshold;
  storage::Config& cfg = storage::config();
  cfg.fine_threshold = fine_threshold_;
  storage::save();
}

void Process::setDoseGainUp(float gain) {
  gain = constrain(gain, min_gain_, max_gain_);
  dose_gain_up_ml_per_ph_per_l_ = gain;
  storage::Config& cfg = storage::config();
  cfg.dose_gain_up = gain;
  storage::save();
}

void Process::setDoseGainDown(float gain) {
  gain = constrain(gain, min_gain_, max_gain_);
  dose_gain_down_ml_per_ph_per_l_ = gain;
  storage::Config& cfg = storage::config();
  cfg.dose_gain_down = gain;
  storage::save();
}

void Process::setFineDoseScale(float scale) {
  scale = constrain(scale, 0.05f, 1.0f);
  fine_dose_scale_ = scale;
  storage::Config& cfg = storage::config();
  cfg.fine_dose_scale = scale;
  storage::save();
}

void Process::setMaxPhDosePerPulsePerLiter(float ml) {
  ml = max(0.0f, ml);
  max_pH_dose_per_pulse_per_liter_ = ml;
  storage::Config& cfg = storage::config();
  cfg.max_ph_dose_per_pulse_per_liter = ml;
  storage::save();
}

void Process::setMaxPhTotalPerLiter(float ml) {
  ml = max(0.0f, ml);
  max_pH_total_ml_per_liter_ = ml;
  storage::Config& cfg = storage::config();
  cfg.max_ph_total_per_liter = ml;
  storage::save();
}

void Process::setProcessTimeoutMs(unsigned long timeout_ms) {
  if (timeout_ms == 0) {
    process_timeout_ms_ = 0;
  } else {
    process_timeout_ms_ = constrain(timeout_ms, 60000UL, 6UL * 60UL * 60UL * 1000UL);
  }
  storage::Config& cfg = storage::config();
  cfg.process_timeout_ms = process_timeout_ms_;
  storage::save();
}

void Process::start(float batch_volume_l, float target_ph, float dose_a_ml_per_l,
                    float dose_b_ml_per_l) {
  if (state_ == State::kMix || state_ == State::kPhCoarse || state_ == State::kPhFine ||
      state_ == State::kFertA || state_ == State::kFertB) {
    return;
  }

  resetState();

  batch_volume_l_ = constrain(batch_volume_l, 1.0f, 250.0f);
  target_ph_ = constrain(target_ph, 2.0f, 12.0f);
  dose_a_ml_per_l_ = max(0.0f, dose_a_ml_per_l);
  dose_b_ml_per_l_ = max(0.0f, dose_b_ml_per_l);

  process_started_ms_ = millisNow();
  process_deadline_ms_ = process_timeout_ms_ > 0 ? process_started_ms_ + process_timeout_ms_ : 0;

#ifdef SIMULATION
  sim::FluidSim& fluid = sim::instance();
  float current_ph = ph_sensor_.hasReading() ? ph_sensor_.getPh() : target_ph_;
  fluid.reset(batch_volume_l_, current_ph, target_ph_);
  fluid.update(millisNow(), ph_sensor_, ec_sensor_);
#endif

  transitionTo(State::kMix);
}

void Process::abort(const String& reason) {
  enterFault(reason);
}

void Process::transitionTo(State new_state) {
  State previous = state_;
  state_ = new_state;
  state_started_ms_ = millisNow();
  logStateTransition(previous, new_state);
  switch (state_) {
    case State::kIdle:
      resetState();
      break;
    case State::kMix:
      enterMixState();
      break;
    case State::kPhCoarse:
      enterPhCoarse();
      break;
    case State::kPhFine:
      enterPhFine();
      break;
    case State::kFertA:
      enterFertA();
      break;
    case State::kFertB:
      enterFertB();
      break;
    case State::kDone:
      enterDone();
      break;
    case State::kFault:
      break;
  }
}

void Process::enterMixState() {
  if (!mix_pumps_on_) {
    setMixPump1(true);
    setMixPump2(true);
    logMixPump(1, true);
    logMixPump(2, true);
    mix_pumps_on_ = true;
  }
  waiting_for_sensor_ = true;
}

void Process::enterPhCoarse() {
  waiting_for_sensor_ = true;
}

void Process::enterPhFine() {
  waiting_for_sensor_ = false;
  scheduleTimer(TimerReason::kFinePause, fine_pause_ms_);
}

void Process::enterFertA() {
  float dose_ml = dose_a_ml_per_l_ * batch_volume_l_;
  if (dose_ml <= 0.0f) {
    scheduleTimer(TimerReason::kFertPause, fert_pause_ms_);
    waiting_for_sensor_ = false;
    return;
  }
  if (!startPumpDose(PumpChannel::kFertA, dose_ml)) {
    return;
  }
}

void Process::enterFertB() {
  float dose_ml = dose_b_ml_per_l_ * batch_volume_l_;
  if (dose_ml <= 0.0f) {
    scheduleTimer(TimerReason::kFertPause, fert_pause_ms_);
    waiting_for_sensor_ = false;
    return;
  }
  if (!startPumpDose(PumpChannel::kFertB, dose_ml)) {
    return;
  }
}

void Process::enterDone() {
  bool was_mixing = mix_pumps_on_;
  pumpsAllOff();
  if (was_mixing) {
    logMixPump(1, false);
    logMixPump(2, false);
  }
  mix_pumps_on_ = false;
  clearTimer();
  waiting_for_sensor_ = false;
  awaiting_gain_update_ = false;
  process_deadline_ms_ = 0;
}

void Process::enterFault(const String& reason) {
  if (pump_.active) {
    stopActivePump();
  }
  bool was_mixing = mix_pumps_on_;
  pumpsAllOff();
  if (was_mixing) {
    logMixPump(1, false);
    logMixPump(2, false);
  }
  mix_pumps_on_ = false;
  pump_.active = false;
  clearTimer();
  State previous = state_;
  state_ = State::kFault;
  logStateTransition(previous, State::kFault);
  fault_reason_ = reason;
  waiting_for_sensor_ = false;
  awaiting_gain_update_ = false;
  process_deadline_ms_ = 0;
  logFaultEvent(reason);
}

void Process::scheduleTimer(TimerReason reason, unsigned long delay_ms) {
  timer_.reason = reason;
  timer_.due_ms = millisNow() + delay_ms;
  timer_.pending = true;
}

void Process::clearTimer() {
  timer_.pending = false;
  timer_.reason = TimerReason::kNone;
  timer_.due_ms = 0;
}

void Process::handleTimer(TimerReason reason) {
  switch (reason) {
    case TimerReason::kCoarsePause:
      waiting_for_sensor_ = true;
      break;
    case TimerReason::kFinePause:
      waiting_for_sensor_ = true;
      break;
    case TimerReason::kFertPause:
      if (state_ == State::kFertA) {
        transitionTo(State::kFertB);
      } else if (state_ == State::kFertB) {
        transitionTo(State::kDone);
      }
      break;
    case TimerReason::kPumpRun:
      if (pump_.active) {
        onPumpFinished(pump_.channel);
      }
      break;
    case TimerReason::kProcessTimeout:
      if (state_ != State::kFault && state_ != State::kDone) {
        enterFault(F("Process timeout"));
      }
      break;
    case TimerReason::kNone:
      break;
  }
}

void Process::onTimer() {
  if (!timer_.pending) {
    return;
  }
  unsigned long now = millisNow();
  if ((long)(now - timer_.due_ms) < 0) {
    return;
  }
  TimerReason reason = timer_.reason;
  clearTimer();
  handleTimer(reason);
}

void Process::onSensorTick() {
#ifdef SIMULATION
  sim::FluidSim& fluid = sim::instance();
  fluid.update(millisNow(), ph_sensor_, ec_sensor_);
#endif
  if (state_ == State::kIdle || state_ == State::kFault) {
    return;
  }

  if (process_deadline_ms_ != 0 && millisNow() > process_deadline_ms_) {
    enterFault(F("Process timeout"));
    return;
  }

  if (!ph_sensor_.hasReading()) {
    return;
  }
  if (!ec_sensor_.hasReading()) {
    return;
  }

  last_ph_ = ph_sensor_.getPh();
  last_ec_ms_ = ec_sensor_.getEcMilliSiemens();
  last_tds_ppm_ = ec_sensor_.getTdsPpm();
  last_temp_c_ = ec_sensor_.hasExternalTemperature() ? ec_sensor_.getTemperatureC() : NAN;

  if (!isfinite(last_ph_) || last_ph_ < 2.0f || last_ph_ > 12.0f) {
    enterFault(F("pH out of range"));
    return;
  }
  if (!isfinite(last_ec_ms_) || last_ec_ms_ <= 0.0f) {
    enterFault(F("EC invalid"));
    return;
  }
  if (!isfinite(last_tds_ppm_) || last_tds_ppm_ <= 0.0f) {
    enterFault(F("TDS invalid"));
    return;
  }
  if (require_temperature_ && !ec_sensor_.hasExternalTemperature()) {
    enterFault(F("Temperature missing"));
    return;
  }

  if (pump_.active) {
    return;
  }

  handlePhMeasurement();
}

void Process::handlePhMeasurement() {
  if (state_ == State::kMix) {
    float delta = target_ph_ - last_ph_;
    if (fabs(delta) > coarse_threshold_) {
      transitionTo(State::kPhCoarse);
    } else {
      transitionTo(State::kPhFine);
    }
    return;
  }

  if (state_ != State::kPhCoarse && state_ != State::kPhFine) {
    return;
  }

  if (!waiting_for_sensor_) {
    return;
  }
  waiting_for_sensor_ = false;

  updateAdaptiveGain(last_ph_);

  float delta = target_ph_ - last_ph_;
  float magnitude = fabs(delta);

  if (state_ == State::kPhCoarse && magnitude <= coarse_threshold_) {
    transitionTo(State::kPhFine);
    return;
  }
  if (state_ == State::kPhFine && magnitude > coarse_threshold_) {
    transitionTo(State::kPhCoarse);
    return;
  }
  if (state_ == State::kPhFine && magnitude <= fine_threshold_) {
    transitionTo(State::kFertA);
    return;
  }

  if (magnitude < 0.01f) {
    waiting_for_sensor_ = true;
    return;
  }

  PumpChannel channel = delta > 0.0f ? PumpChannel::kPhUp : PumpChannel::kPhDown;
  float requested_volume_ml = computePhDoseVolume(delta);
  float per_pulse_limit = maxPerPulseMl();
  if (per_pulse_limit > 0.0f) {
    requested_volume_ml = min(requested_volume_ml, per_pulse_limit);
  }
  if (state_ == State::kPhFine && per_pulse_limit > 0.0f) {
    requested_volume_ml = min(requested_volume_ml, per_pulse_limit * fine_dose_scale_);
  }

  float total_limit = maxTotalMl();
  float* accumulated = nullptr;
  switch (channel) {
    case PumpChannel::kPhDown:
      accumulated = &log_.ph_down_ml;
      break;
    case PumpChannel::kPhUp:
      accumulated = &log_.ph_up_ml;
      break;
    default:
      break;
  }
  if (accumulated && total_limit > 0.0f && (*accumulated + requested_volume_ml) > total_limit) {
    enterFault(F("pH dosing limit"));
    return;
  }

  if (requested_volume_ml <= 0.0f) {
    waiting_for_sensor_ = true;
    return;
  }

  last_dose_reference_ph_ = last_ph_;
  last_requested_delta_ph_ = magnitude;
  last_dose_direction_ = delta > 0.0f ? 1 : -1;
  awaiting_gain_update_ = true;

  if (!startPumpDose(channel, requested_volume_ml)) {
    awaiting_gain_update_ = false;
    waiting_for_sensor_ = true;
    return;
  }
}

float Process::computePhDoseVolume(float delta_ph) const {
  float magnitude = fabs(delta_ph);
  float gain = delta_ph > 0.0f ? dose_gain_up_ml_per_ph_per_l_ : dose_gain_down_ml_per_ph_per_l_;
  return magnitude * batch_volume_l_ * gain;
}

void Process::updateAdaptiveGain(float current_ph) {
  if (!awaiting_gain_update_ || last_dose_direction_ == 0 || last_requested_delta_ph_ <= 0.0f ||
      pump_.active) {
    return;
  }

  float measured_delta = (current_ph - last_dose_reference_ph_) * last_dose_direction_;
  float expected_delta = last_requested_delta_ph_;
  float ratio;
  if (measured_delta <= 0.0001f) {
    ratio = 1.1f;
  } else {
    ratio = expected_delta / measured_delta;
  }
  ratio = constrain(ratio, 0.9f, 1.1f);
  if (last_dose_direction_ > 0) {
    dose_gain_up_ml_per_ph_per_l_ = constrain(dose_gain_up_ml_per_ph_per_l_ * ratio, min_gain_, max_gain_);
  } else {
    dose_gain_down_ml_per_ph_per_l_ = constrain(dose_gain_down_ml_per_ph_per_l_ * ratio, min_gain_, max_gain_);
  }
  awaiting_gain_update_ = false;
}

bool Process::startPumpDose(PumpChannel channel, float volume_ml) {
  if (pump_.active) {
    return false;
  }
  if (volume_ml <= 0.0f) {
    return false;
  }
  if ((channel == PumpChannel::kPhDown && pump_.channel == PumpChannel::kPhUp && pump_.active) ||
      (channel == PumpChannel::kPhUp && pump_.channel == PumpChannel::kPhDown && pump_.active)) {
    return false;
  }
  float rate = pumpRateFor(channel);
  if (rate <= 0.0f) {
    enterFault(F("Pump calibration missing"));
    return false;
  }
  unsigned long duration_ms = static_cast<unsigned long>((volume_ml / rate) * 1000.0f + 0.5f);
  if (duration_ms == 0) {
    duration_ms = 1;
  }
  pump_.active = true;
  pump_.channel = channel;
  pump_.volume_ml = volume_ml;
  pump_.started_ms = millisNow();
  pump_.duration_ms = duration_ms;
  logPumpChange(channel, true, volume_ml, duration_ms);
  applyPumpOutput(channel, true);
  scheduleTimer(TimerReason::kPumpRun, duration_ms);
  return true;
}

void Process::stopActivePump() {
  if (!pump_.active) {
    return;
  }
  unsigned long elapsed = millisNow() - pump_.started_ms;
  applyPumpOutput(pump_.channel, false);
  logPumpChange(pump_.channel, false, pump_.volume_ml, elapsed);
  pump_.active = false;
}

void Process::applyPumpOutput(PumpChannel channel, bool on) {
  switch (channel) {
    case PumpChannel::kPhDown:
      setPumpPhDown(on);
      break;
    case PumpChannel::kPhUp:
      setPumpPhUp(on);
      break;
    case PumpChannel::kFertA:
      setPumpA(on);
      break;
    case PumpChannel::kFertB:
      setPumpB(on);
      break;
  }
}

float Process::pumpRateFor(PumpChannel channel) const {
  return pump_ml_per_sec_[static_cast<uint8_t>(channel)];
}

void Process::onPumpFinished(PumpChannel channel) {
  if (!pump_.active || pump_.channel != channel) {
    return;
  }
  stopActivePump();
  float volume = pump_.volume_ml;
  pump_.volume_ml = 0.0f;
  pump_.started_ms = 0;
  pump_.duration_ms = 0;
  switch (channel) {
    case PumpChannel::kPhDown:
      log_.ph_down_ml += volume;
      break;
    case PumpChannel::kPhUp:
      log_.ph_up_ml += volume;
      break;
    case PumpChannel::kFertA:
      log_.fert_a_ml += volume;
      break;
    case PumpChannel::kFertB:
      log_.fert_b_ml += volume;
      break;
  }

  if (channel == PumpChannel::kPhDown || channel == PumpChannel::kPhUp) {
    float total_limit = maxTotalMl();
    float accumulated = (channel == PumpChannel::kPhDown) ? log_.ph_down_ml : log_.ph_up_ml;
    if (total_limit > 0.0f && accumulated > total_limit + 1e-3f) {
      enterFault(F("pH total limit"));
      return;
    }
#ifdef SIMULATION
    sim::instance().notifyDose(channel, volume, batch_volume_l_, target_ph_);
#endif
    if (state_ == State::kPhFine) {
      scheduleTimer(TimerReason::kFinePause, fine_pause_ms_);
    } else {
      scheduleTimer(TimerReason::kCoarsePause, coarse_pause_ms_);
    }
  } else {
#ifdef SIMULATION
    sim::instance().notifyDose(channel, volume, batch_volume_l_, target_ph_);
#endif
    scheduleTimer(TimerReason::kFertPause, fert_pause_ms_);
  }
}

void Process::setPumpCalibration(PumpChannel channel, float ml_per_sec) {
  if (!isfinite(ml_per_sec) || ml_per_sec <= 0.0f) {
    return;
  }
  uint8_t index = static_cast<uint8_t>(channel);
  pump_ml_per_sec_[index] = ml_per_sec;
  storage::Config& cfg = storage::config();
  cfg.pump_ml_per_sec[index] = ml_per_sec;
  storage::save();
}

float Process::getPumpCalibration(PumpChannel channel) const {
  return pump_ml_per_sec_[static_cast<uint8_t>(channel)];
}

const __FlashStringHelper* Process::stateName(State state) const {
  switch (state) {
    case State::kIdle:
      return F("IDLE");
    case State::kMix:
      return F("MIX");
    case State::kPhCoarse:
      return F("PH_COARSE");
    case State::kPhFine:
      return F("PH_FINE");
    case State::kFertA:
      return F("FERT_A");
    case State::kFertB:
      return F("FERT_B");
    case State::kDone:
      return F("DONE");
    case State::kFault:
      return F("FAULT");
  }
  return F("UNKNOWN");
}

const __FlashStringHelper* Process::pumpChannelName(PumpChannel channel) const {
  switch (channel) {
    case PumpChannel::kPhDown:
      return F("PH_DOWN");
    case PumpChannel::kPhUp:
      return F("PH_UP");
    case PumpChannel::kFertA:
      return F("FERT_A");
    case PumpChannel::kFertB:
      return F("FERT_B");
  }
  return F("PUMP");
}

void Process::logStateTransition(State from, State to) {
  if (!Serial) {
    return;
  }
  if (from == to) {
    return;
  }
  Serial.print(F("[state] "));
  Serial.print(stateName(from));
  Serial.print(F(" -> "));
  Serial.println(stateName(to));
}

void Process::logPumpChange(PumpChannel channel, bool on, float volume_ml, unsigned long duration_ms) {
  if (!Serial) {
    return;
  }
  Serial.print(F("[pump] "));
  Serial.print(pumpChannelName(channel));
  Serial.print(on ? F(" ON") : F(" OFF"));
  Serial.print(F(" volume="));
  Serial.print(volume_ml, 2);
  Serial.print(F("ml"));
  Serial.print(F(" duration="));
  Serial.print(duration_ms);
  Serial.println(F("ms"));
}

void Process::logMixPump(uint8_t index, bool on) {
  if (!Serial) {
    return;
  }
  Serial.print(F("[mix] pump"));
  Serial.print(index);
  Serial.println(on ? F(" ON") : F(" OFF"));
}

void Process::logFaultEvent(const String& reason) {
  if (!Serial) {
    return;
  }
  Serial.print(F("[fault] "));
  Serial.println(reason);
  Serial.print(F("        pH="));
  if (isfinite(last_ph_)) {
    Serial.print(last_ph_, 2);
  } else {
    Serial.print(F("--"));
  }
  Serial.print(F(" EC="));
  if (isfinite(last_ec_ms_)) {
    Serial.print(last_ec_ms_, 3);
    Serial.print(F("mS"));
  } else {
    Serial.print(F("--"));
  }
  Serial.print(F(" TDS="));
  if (isfinite(last_tds_ppm_)) {
    Serial.print(last_tds_ppm_, 1);
  } else {
    Serial.print(F("--"));
  }
  Serial.print(F(" temp="));
  if (isfinite(last_temp_c_)) {
    Serial.print(last_temp_c_, 1);
    Serial.print(F("C"));
  } else {
    Serial.print(F("--"));
  }
  Serial.println();
  Serial.print(F("        doses A="));
  Serial.print(log_.fert_a_ml, 1);
  Serial.print(F("ml B="));
  Serial.print(log_.fert_b_ml, 1);
  Serial.print(F("ml UP="));
  Serial.print(log_.ph_up_ml, 2);
  Serial.print(F("ml DOWN="));
  Serial.print(log_.ph_down_ml, 2);
  Serial.println(F("ml"));
}

#ifdef SIMULATION
void Process::updateSimulationContext() {
  sim::FluidSim& fluid = sim::instance();
  float current_ph = ph_sensor_.hasReading() ? ph_sensor_.getPh() : target_ph_;
  fluid.reset(batch_volume_l_ > 0.0f ? batch_volume_l_ : 10.0f, current_ph, target_ph_);
  fluid.update(millisNow(), ph_sensor_, ec_sensor_);
}
#endif

}  // namespace process

