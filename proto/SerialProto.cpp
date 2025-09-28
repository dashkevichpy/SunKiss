#include "SerialProto.h"

#include <ctype.h>
#include <math.h>

namespace proto {
namespace {
bool isFinished(unsigned long now, unsigned long due_ms) {
  return static_cast<long>(now - due_ms) >= 0;
}

}  // namespace

SerialProto::SerialProto(Stream& io, process::Process& process, sensors::PhSensor& ph_sensor,
                         sensors::EcSensor& ec_sensor, sensors::VccMeter& vcc_meter)
    : io_(io),
      process_(process),
      ph_sensor_(ph_sensor),
      ec_sensor_(ec_sensor),
      vcc_meter_(vcc_meter) {
  last_state_ = process_.state();
}

void SerialProto::begin() {
  line_buffer_.reserve(96);
  target_ph_ = process_.targetPh();
  batch_volume_l_ = process_.batchVolumeLiters();
  dose_a_ml_per_l_ = 0.0f;
  dose_b_ml_per_l_ = 0.0f;
  storage::loadOrDefaults();
  device_id_ = storage::config().device_id;
  last_status_ms_ = millis();
}

void SerialProto::update() {
  unsigned long now = millis();
  if (mix_override_active_ && isFinished(now, mix_deadline_ms_)) {
    setMixPump1(false);
    setMixPump2(false);
    mix_override_active_ = false;
    mix_deadline_ms_ = 0;
  }
  if (manual_pump_.active && isFinished(now, manual_pump_.due_ms)) {
    stopManualPump();
  }

  readSerial();
  checkProcessState(now);
  maybeSendStatus(now);
}

void SerialProto::readSerial() {
  while (io_.available() > 0) {
    char c = static_cast<char>(io_.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      String line = line_buffer_;
      line_buffer_.remove(0);
      line.trim();
      if (line.length() > 0) {
        handleLine(line);
      }
      continue;
    }
    if (line_buffer_.length() < 120) {
      line_buffer_ += c;
    }
  }
}

void SerialProto::handleLine(const String& line) {
  String sensor_message;
  sensors::CommandResult sensor_result = ph_sensor_.handleCommand(line, sensor_message);
  if (sensor_result != sensors::CommandResult::kNotHandled) {
    if (sensor_result == sensors::CommandResult::kOk) {
      respondOk(sensor_message);
    } else {
      respondError(ErrorCode::kCalibrationError, sensor_message);
    }
    return;
  }

  sensor_result = ec_sensor_.handleCommand(line, sensor_message);
  if (sensor_result != sensors::CommandResult::kNotHandled) {
    if (sensor_result == sensors::CommandResult::kOk) {
      respondOk(sensor_message);
    } else if (sensor_result == sensors::CommandResult::kError) {
      respondError(ErrorCode::kCalibrationError, sensor_message);
    } else {
      respondError(ErrorCode::kUnknownCommand, sensor_message);
    }
    return;
  }

  String tokens[4];
  int count = 0;
  tokenize(line, tokens, 4, &count);
  if (count == 0) {
    return;
  }

  String command_upper = tokens[0];
  command_upper.toUpperCase();

  if (command_upper == "CONFIG_DUMP") {
    storage::dump(io_);
    respondOk();
    return;
  }

  if (command_upper == "SET_TARGET_PH") {
    if (count < 2) {
      respondError(ErrorCode::kInvalidArgument, F("SET_TARGET_PH requires value"));
      return;
    }
    float value = tokens[1].toFloat();
    if (!isfinite(value) || value < 2.0f || value > 12.0f) {
      respondError(ErrorCode::kInvalidArgument, F("target pH must be 2-12"));
      return;
    }
    target_ph_ = value;
    respondOk(String(F("TARGET_PH=")) + String(target_ph_, 2));
    return;
  }

  if (command_upper == "SET_BATCH_L") {
    if (count < 2) {
      respondError(ErrorCode::kInvalidArgument, F("SET_BATCH_L requires value"));
      return;
    }
    float value = tokens[1].toFloat();
    if (!isfinite(value) || value < 1.0f || value > 250.0f) {
      respondError(ErrorCode::kInvalidArgument, F("batch volume 1-250L"));
      return;
    }
    batch_volume_l_ = value;
    respondOk(String(F("BATCH_L=")) + String(batch_volume_l_, 1));
    return;
  }

  if (command_upper == "SET_DOSE_A_ML_PER_L") {
    if (count < 2) {
      respondError(ErrorCode::kInvalidArgument, F("SET_DOSE_A_ML_PER_L requires value"));
      return;
    }
    float value = tokens[1].toFloat();
    if (!isfinite(value) || value < 0.0f) {
      respondError(ErrorCode::kInvalidArgument, F("dose must be >=0"));
      return;
    }
    dose_a_ml_per_l_ = value;
    respondOk(String(F("DOSE_A_ML_PER_L=")) + String(dose_a_ml_per_l_, 2));
    return;
  }

  if (command_upper == "SET_DOSE_B_ML_PER_L") {
    if (count < 2) {
      respondError(ErrorCode::kInvalidArgument, F("SET_DOSE_B_ML_PER_L requires value"));
      return;
    }
    float value = tokens[1].toFloat();
    if (!isfinite(value) || value < 0.0f) {
      respondError(ErrorCode::kInvalidArgument, F("dose must be >=0"));
      return;
    }
    dose_b_ml_per_l_ = value;
    respondOk(String(F("DOSE_B_ML_PER_L=")) + String(dose_b_ml_per_l_, 2));
    return;
  }

  if (command_upper == "SET_PUMP_RATE") {
    if (count < 3) {
      respondError(ErrorCode::kInvalidArgument, F("SET_PUMP_RATE channel value"));
      return;
    }
    process::PumpChannel channel;
    if (!parsePumpChannel(tokens[1], &channel)) {
      respondError(ErrorCode::kInvalidArgument, F("unknown pump channel"));
      return;
    }
    float rate = tokens[2].toFloat();
    if (!isfinite(rate) || rate <= 0.0f) {
      respondError(ErrorCode::kInvalidArgument, F("rate must be >0"));
      return;
    }
    process_.setPumpCalibration(channel, rate);
    String channel_upper = tokens[1];
    channel_upper.toUpperCase();
    respondOk(String(F("PUMP_RATE_SET=")) + channel_upper + F(" ") + String(rate, 3));
    return;
  }

  if (command_upper == "SET_ID") {
    if (count < 2) {
      respondError(ErrorCode::kInvalidArgument, F("SET_ID requires value"));
      return;
    }
    long value = tokens[1].toInt();
    if (value < 0 || value > 65535) {
      respondError(ErrorCode::kInvalidArgument, F("ID 0-65535"));
      return;
    }
    storage::Config& cfg = storage::config();
    cfg.device_id = static_cast<uint16_t>(value);
    storage::save();
    device_id_ = cfg.device_id;
    respondOk(String(F("ID=")) + String(device_id_));
    return;
  }

  if (command_upper == "GET_ID") {
    respondOk(String(F("ID=")) + String(device_id_));
    return;
  }

  if (command_upper == "START") {
    handleStart();
    return;
  }

  if (command_upper == "ABORT") {
    String remainder;
    if (line.length() > 5) {
      remainder = line.substring(5);
    }
    handleAbort(remainder);
    return;
  }

  if (command_upper == "MIX_ONLY") {
    handleMixOnly(tokens, count);
    return;
  }

  if (command_upper == "TEST_PUMP") {
    handleTestPump(tokens, count);
    return;
  }

  if (command_upper == "READ_NOW") {
    handleReadNow();
    return;
  }

  respondError(ErrorCode::kUnknownCommand, F("command not recognized"));
}

void SerialProto::respondOk(const String& message) {
  io_.print(F("OK"));
  if (message.length() > 0) {
    io_.print(' ');
    io_.print(message);
  }
  io_.println();
}

void SerialProto::respondError(ErrorCode code, const String& message) {
  io_.print(F("ERR "));
  io_.print(static_cast<uint8_t>(code));
  io_.print(' ');
  io_.println(message);
}

void SerialProto::handleStart() {
  if (process_.isActive()) {
    respondError(ErrorCode::kProcessBusy, F("process already running"));
    return;
  }
  if (manual_pump_.active || mix_override_active_) {
    respondError(ErrorCode::kPumpBusy, F("service output active"));
    return;
  }
  process_.start(batch_volume_l_, target_ph_, dose_a_ml_per_l_, dose_b_ml_per_l_);
  respondOk(F("STARTED"));
  last_status_ms_ = 0;
}

void SerialProto::handleAbort(const String& remainder) {
  if (process_.state() == process::State::kIdle) {
    respondOk(F("IDLE"));
    return;
  }
  String reason = remainder;
  reason.trim();
  if (reason.length() == 0) {
    reason = F("User abort");
  } else {
    reason = String(F("User abort: ")) + reason;
  }
  process_.abort(reason);
  respondOk(F("ABORTED"));
}

void SerialProto::handleMixOnly(const String* tokens, int count) {
  if (count < 2) {
    respondError(ErrorCode::kInvalidArgument, F("MIX_ONLY duration_ms"));
    return;
  }
  if (process_.isActive()) {
    respondError(ErrorCode::kProcessBusy, F("process running"));
    return;
  }
  if (manual_pump_.active) {
    respondError(ErrorCode::kPumpBusy, F("pump test active"));
    return;
  }
  long duration_long = tokens[1].toInt();
  if (duration_long <= 0) {
    respondError(ErrorCode::kInvalidArgument, F("duration must be >0"));
    return;
  }
  unsigned long duration = static_cast<unsigned long>(duration_long);
  setMixPump1(true);
  setMixPump2(true);
  mix_override_active_ = true;
  mix_deadline_ms_ = millis() + duration;
  respondOk(F("MIXING"));
}

void SerialProto::handleTestPump(const String* tokens, int count) {
  if (count < 3) {
    respondError(ErrorCode::kInvalidArgument, F("TEST_PUMP channel ml"));
    return;
  }
  if (process_.isActive()) {
    respondError(ErrorCode::kProcessBusy, F("process running"));
    return;
  }
  if (mix_override_active_) {
    respondError(ErrorCode::kPumpBusy, F("mix pumps active"));
    return;
  }
  if (manual_pump_.active) {
    respondError(ErrorCode::kPumpBusy, F("pump already dosing"));
    return;
  }

  process::PumpChannel channel;
  if (!parsePumpChannel(tokens[1], &channel)) {
    respondError(ErrorCode::kInvalidArgument, F("unknown pump channel"));
    return;
  }
  String channel_upper = tokens[1];
  channel_upper.toUpperCase();

  float volume_ml = tokens[2].toFloat();
  if (!isfinite(volume_ml) || volume_ml <= 0.0f) {
    respondError(ErrorCode::kInvalidArgument, F("volume must be >0"));
    return;
  }

  float rate = process_.getPumpCalibration(channel);
  if (rate <= 0.0f) {
    respondError(ErrorCode::kSensorError, F("pump calibration missing"));
    return;
  }

  unsigned long duration_ms = static_cast<unsigned long>((volume_ml / rate) * 1000.0f + 0.5f);
  if (duration_ms == 0) {
    duration_ms = 1;
  }

  applyPumpOutput(channel, true);
  manual_pump_.active = true;
  manual_pump_.channel = channel;
  manual_pump_.due_ms = millis() + duration_ms;
  String message(F("PUMP_TEST "));
  message += channel_upper;
  message += F(" ");
  message += String(volume_ml, 2);
  respondOk(message);
}

void SerialProto::stopManualPump() {
  if (!manual_pump_.active) {
    return;
  }
  applyPumpOutput(manual_pump_.channel, false);
  manual_pump_.active = false;
  manual_pump_.due_ms = 0;
}

void SerialProto::handleReadNow() {
  respondOk();
  sendStatusBlock(F("STATUS"));
  last_status_ms_ = millis();
}

void SerialProto::maybeSendStatus(unsigned long now) {
  if (STATUS_INTERVAL_MS == 0) {
    return;
  }
  if (now - last_status_ms_ < STATUS_INTERVAL_MS) {
    return;
  }
  sendStatusBlock(F("STATUS"));
  last_status_ms_ = now;
}

void SerialProto::checkProcessState(unsigned long now) {
  process::State state = process_.state();
  if (state == last_state_) {
    return;
  }
  last_state_ = state;
  switch (state) {
    case process::State::kDone:
      sendStatusBlock(F("DONE"));
      break;
    case process::State::kFault:
      sendFaultReport(now);
      break;
    default:
      sendStatusBlock(F("STATUS"));
      break;
  }
  last_status_ms_ = now;
}

void SerialProto::sendStatusBlock(const __FlashStringHelper* prefix) {
  float ph = ph_sensor_.hasReading() ? ph_sensor_.getPh() : NAN;
  float ec_ms = ec_sensor_.hasReading() ? ec_sensor_.getEcMilliSiemens() : NAN;
  float temp_c = ec_sensor_.hasExternalTemperature() ? ec_sensor_.getTemperatureC() : NAN;
  float tds_ppm = ec_sensor_.hasReading() ? ec_sensor_.getTdsPpm() : NAN;
  bool has_vcc = vcc_meter_.hasReading();
  uint16_t vcc_mv = has_vcc ? vcc_meter_.getVccMilliVolts() : 0;

  float target = process_.isActive() ? process_.targetPh() : target_ph_;
  float delta = (isfinite(ph) && isfinite(target)) ? (target - ph) : NAN;

  io_.print(prefix);
  io_.print(F(" PH:"));
  if (isfinite(ph)) {
    io_.print(ph, 2);
  } else {
    io_.print(F("--"));
  }
  io_.print(F(" EC:"));
  if (isfinite(ec_ms)) {
    io_.print(ec_ms, 2);
  } else {
    io_.print(F("--"));
  }
  io_.print(F(" T:"));
  if (isfinite(temp_c)) {
    io_.print(temp_c, 1);
  } else {
    io_.print(F("--"));
  }
  io_.print(F(" TDS:"));
  if (isfinite(tds_ppm)) {
    io_.print(static_cast<long>(tds_ppm + 0.5f));
  } else {
    io_.print(F("--"));
  }
  io_.print(F(" VCC:"));
  if (has_vcc) {
    io_.print(vcc_mv);
  } else {
    io_.print(F("--"));
  }
  io_.println();

  io_.print(F("       STATE:"));
  io_.print(stateToString(process_.state()));
  io_.print(F(" TARGET_PH:"));
  if (isfinite(target)) {
    io_.print(target, 2);
  } else {
    io_.print(F("--"));
  }
  io_.print(F(" DELTA_PH:"));
  if (isfinite(delta)) {
    io_.print(delta, 2);
  } else {
    io_.print(F("--"));
  }
  io_.println();

  const process::DosingLog& log = process_.log();
  io_.print(F("       DOSE_A:"));
  io_.print(log.fert_a_ml, 1);
  io_.print(F(" DOSE_B:"));
  io_.print(log.fert_b_ml, 1);
  io_.print(F(" DOSE_UP:"));
  io_.print(log.ph_up_ml, 1);
  io_.print(F(" DOSE_DOWN:"));
  io_.print(log.ph_down_ml, 1);
  io_.println();
}

void SerialProto::sendFaultReport(unsigned long now) {
  String reason = process_.faultReason();
  reason.trim();
  if (reason.length() == 0) {
    reason = F("unknown");
  }
  uint8_t code = faultCodeFromReason(reason);
  io_.print(F("FAULT code:"));
  io_.print(code);
  io_.print(F(" msg:\""));
  io_.print(reason);
  io_.println(F("\""));
  sendStatusBlock(F("FAULT"));
}

String SerialProto::stateToString(process::State state) const {
  switch (state) {
    case process::State::kIdle:
      return F("IDLE");
    case process::State::kMix:
      return F("MIX");
    case process::State::kPhCoarse:
      return F("PH_COARSE");
    case process::State::kPhFine:
      return F("PH_FINE");
    case process::State::kFertA:
      return F("FERT_A");
    case process::State::kFertB:
      return F("FERT_B");
    case process::State::kDone:
      return F("DONE");
    case process::State::kFault:
      return F("FAULT");
  }
  return F("UNKNOWN");
}

bool SerialProto::parsePumpChannel(const String& token, process::PumpChannel* channel) const {
  String upper = token;
  upper.toUpperCase();
  if (upper == "PH_DOWN") {
    *channel = process::PumpChannel::kPhDown;
    return true;
  }
  if (upper == "PH_UP") {
    *channel = process::PumpChannel::kPhUp;
    return true;
  }
  if (upper == "A") {
    *channel = process::PumpChannel::kFertA;
    return true;
  }
  if (upper == "B") {
    *channel = process::PumpChannel::kFertB;
    return true;
  }
  return false;
}

void SerialProto::applyPumpOutput(process::PumpChannel channel, bool on) {
  switch (channel) {
    case process::PumpChannel::kPhDown:
      setPumpPhDown(on);
      break;
    case process::PumpChannel::kPhUp:
      setPumpPhUp(on);
      break;
    case process::PumpChannel::kFertA:
      setPumpA(on);
      break;
    case process::PumpChannel::kFertB:
      setPumpB(on);
      break;
  }
}

uint8_t SerialProto::faultCodeFromReason(const String& reason) const {
  String lower = reason;
  lower.toLowerCase();
  if (lower.indexOf("ph out of range") >= 0) {
    return 1;
  }
  if (lower.indexOf("ec invalid") >= 0) {
    return 2;
  }
  if (lower.indexOf("tds invalid") >= 0) {
    return 3;
  }
  if (lower.indexOf("temperature missing") >= 0) {
    return 4;
  }
  if (lower.indexOf("timeout") >= 0) {
    return 5;
  }
  if (lower.indexOf("ph dosing limit") >= 0 || lower.indexOf("ph total limit") >= 0) {
    return 6;
  }
  if (lower.indexOf("pump calibration") >= 0) {
    return 7;
  }
  if (lower.indexOf("abort") >= 0) {
    return 8;
  }
  return 0;
}

void SerialProto::tokenize(const String& line, String* tokens, int max_tokens, int* count) const {
  *count = 0;
  int length = line.length();
  int index = 0;
  while (index < length && *count < max_tokens) {
    while (index < length && isspace(static_cast<unsigned char>(line[index]))) {
      ++index;
    }
    if (index >= length) {
      break;
    }
    int start = index;
    while (index < length && !isspace(static_cast<unsigned char>(line[index]))) {
      ++index;
    }
    tokens[*count] = line.substring(start, index);
    ++(*count);
  }
}

}  // namespace proto

