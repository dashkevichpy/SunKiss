#pragma once

#ifdef SIMULATION

#include <Arduino.h>

namespace sensors {
class PhSensor;
class EcSensor;
}  // namespace sensors

namespace process {
enum class PumpChannel : uint8_t;
}  // namespace process

namespace sim {

class FluidSim {
 public:
  void reset(float batch_volume_l, float initial_ph, float target_ph);
  void notifyDose(process::PumpChannel channel, float volume_ml, float batch_volume_l,
                  float target_ph);
  void update(unsigned long now, sensors::PhSensor& ph_sensor, sensors::EcSensor& ec_sensor);

 private:
  float batch_volume_l_{10.0f};
  float current_ph_{6.5f};
  float target_ph_{6.5f};
  float pending_delta_{0.0f};
  float simulated_ec_ms_{1.2f};
  float simulated_tds_{600.0f};
  unsigned long last_update_ms_{0};
};

FluidSim& instance();

}  // namespace sim

#endif  // SIMULATION

