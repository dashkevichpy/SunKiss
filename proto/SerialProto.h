#pragma once

#include <Arduino.h>
#include <Stream.h>

#include "../drivers/Actuators.h"
#include "../process/Process.h"
#include "../sensors/Sensors.h"
#include "../storage/Config.h"

namespace proto {

enum class ErrorCode : uint8_t {
  kUnknownCommand = 1,
  kInvalidArgument = 2,
  kProcessBusy = 3,
  kPumpBusy = 4,
  kSensorError = 5,
  kOperationFailed = 6,
  kCalibrationError = 7,
};

class SerialProto {
 public:
  SerialProto(Stream& io, process::Process& process, sensors::PhSensor& ph_sensor,
              sensors::EcSensor& ec_sensor, sensors::VccMeter& vcc_meter);

  void begin();
  void update();

  float targetPhSetting() const { return target_ph_; }
  float batchVolumeSetting() const { return batch_volume_l_; }
  float doseASetting() const { return dose_a_ml_per_l_; }
  float doseBSetting() const { return dose_b_ml_per_l_; }
  uint16_t deviceId() const { return device_id_; }

 private:
  static constexpr unsigned long STATUS_INTERVAL_MS = 5000UL;

  struct ManualPumpRun {
    bool active{false};
    process::PumpChannel channel{process::PumpChannel::kPhDown};
    unsigned long due_ms{0};
  };

  void readSerial();
  void handleLine(const String& line);
  void respondOk(const String& message = String());
  void respondError(ErrorCode code, const String& message);

  void handleStart();
  void handleAbort(const String& remainder);
  void handleMixOnly(const String* tokens, int count);
  void handleTestPump(const String* tokens, int count);
  void handleReadNow();

  void maybeSendStatus(unsigned long now);
  void checkProcessState(unsigned long now);
  void sendStatusBlock(const __FlashStringHelper* prefix);
  void sendFaultReport(unsigned long now);

  String stateToString(process::State state) const;
  bool parsePumpChannel(const String& token, process::PumpChannel* channel) const;
  void applyPumpOutput(process::PumpChannel channel, bool on);
  void stopManualPump();
  uint8_t faultCodeFromReason(const String& reason) const;

  void tokenize(const String& line, String* tokens, int max_tokens, int* count) const;

  Stream& io_;
  process::Process& process_;
  sensors::PhSensor& ph_sensor_;
  sensors::EcSensor& ec_sensor_;
  sensors::VccMeter& vcc_meter_;

  String line_buffer_;
  unsigned long last_status_ms_{0};
  process::State last_state_;

  float target_ph_{6.5f};
  float batch_volume_l_{10.0f};
  float dose_a_ml_per_l_{0.0f};
  float dose_b_ml_per_l_{0.0f};

  uint16_t device_id_{1};

  unsigned long mix_deadline_ms_{0};
  bool mix_override_active_{false};

  ManualPumpRun manual_pump_;
};

}  // namespace proto

