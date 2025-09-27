#pragma once

#include <Arduino.h>
#include <Stream.h>

namespace storage {

constexpr uint16_t kConfigVersion = 1;
constexpr size_t kPumpChannelCount = 4;
constexpr size_t kPhReferenceCount = 3;

struct Config {
  float ph_gain{-5.70f};
  float ph_offset{21.34f};
  float ph_reference_voltages[kPhReferenceCount] = {NAN, NAN, NAN};
  uint8_t ph_valid_mask{0};

  float ec_alpha{0.02f};
  int32_t ec_tds_factor{500};
  float ec_k_cell{1.0f};

  float pump_ml_per_sec[kPumpChannelCount] = {1.0f, 1.0f, 3.0f, 3.0f};

  bool relay_active_low{true};
  uint16_t device_id{1};

  float coarse_threshold{0.3f};
  float fine_threshold{0.1f};
  unsigned long coarse_pause_ms{60000UL};
  unsigned long fine_pause_ms{180000UL};
  unsigned long fert_pause_ms{180000UL};
  float max_ph_dose_per_pulse_per_liter{0.5f};
  float max_ph_total_per_liter{5.0f};
  float dose_gain_up{0.2f};
  float dose_gain_down{0.2f};
  float fine_dose_scale{0.25f};
  unsigned long process_timeout_ms{3600000UL};
};

Config& config();
void loadOrDefaults();
bool save();
void dump(Stream& stream);

uint16_t crc16(const uint8_t* data, size_t length);

}  // namespace storage

