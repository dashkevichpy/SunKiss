#include "sim/FluidSim.h"

#ifdef SIMULATION

#include <math.h>

#include "process/Process.h"
#include "sensors/Sensors.h"

namespace sim {
namespace {
FluidSim g_instance;

float clampVolume(float volume) { return volume < 0.0f ? 0.0f : volume; }

}  // namespace

FluidSim& instance() { return g_instance; }

void FluidSim::reset(float batch_volume_l, float initial_ph, float target) {
  batch_volume_l_ = max(1.0f, batch_volume_l);
  current_ph_ = initial_ph;
  target_ph_ = target;
  pending_delta_ = 0.0f;
  simulated_ec_ms_ = 1.2f;
  simulated_tds_ = simulated_ec_ms_ * 500.0f;
  last_update_ms_ = 0;
}

void FluidSim::notifyDose(process::PumpChannel channel, float volume_ml, float batch_volume_l,
                          float target) {
  batch_volume_l_ = max(1.0f, batch_volume_l);
  target_ph_ = target;
  volume_ml = clampVolume(volume_ml);
  switch (channel) {
    case process::PumpChannel::kPhDown: {
      float response = -0.12f * (volume_ml / batch_volume_l_);
      pending_delta_ += response;
      break;
    }
    case process::PumpChannel::kPhUp: {
      float response = 0.12f * (volume_ml / batch_volume_l_);
      pending_delta_ += response;
      break;
    }
    case process::PumpChannel::kFertA:
    case process::PumpChannel::kFertB: {
      float ec_gain = (volume_ml / batch_volume_l_) * 0.4f;
      simulated_ec_ms_ += ec_gain;
      simulated_tds_ = simulated_ec_ms_ * 500.0f;
      break;
    }
  }
}

void FluidSim::update(unsigned long now, sensors::PhSensor& ph_sensor, sensors::EcSensor& ec_sensor) {
  if (last_update_ms_ == 0) {
    last_update_ms_ = now;
  }
  unsigned long delta_ms = now - last_update_ms_;
  last_update_ms_ = now;
  float dt = static_cast<float>(delta_ms) / 1000.0f;
  if (dt <= 0.0f) {
    dt = 0.05f;
  }
  float apply = pending_delta_ * min(dt * 0.35f, 1.0f);
  current_ph_ += apply;
  pending_delta_ -= apply;
  float drift = (target_ph_ - current_ph_) * 0.02f * dt;
  current_ph_ += drift;
  current_ph_ = constrain(current_ph_, 3.5f, 9.5f);

  ph_sensor.setSimulatedPh(current_ph_);
  ec_sensor.setSimulatedSolution(simulated_ec_ms_, simulated_tds_);
}

}  // namespace sim

#endif  // SIMULATION

