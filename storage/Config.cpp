#include "storage/Config.h"

#include <EEPROM.h>
#include <math.h>

namespace storage {
namespace {
struct StoredConfig {
  uint16_t version;
  Config config;
  uint16_t crc;
};

Config makeDefaultConfig() {
  Config cfg;
  cfg.ph_gain = -5.70f;
  cfg.ph_offset = 21.34f;
  for (size_t i = 0; i < kPhReferenceCount; ++i) {
    cfg.ph_reference_voltages[i] = NAN;
  }
  cfg.ph_valid_mask = 0;
  cfg.ec_alpha = 0.02f;
  cfg.ec_tds_factor = 500;
  cfg.ec_k_cell = 1.0f;
  cfg.pump_ml_per_sec[0] = 1.0f;
  cfg.pump_ml_per_sec[1] = 1.0f;
  cfg.pump_ml_per_sec[2] = 3.0f;
  cfg.pump_ml_per_sec[3] = 3.0f;
  cfg.relay_active_low = true;
  cfg.device_id = 1;
  cfg.coarse_threshold = 0.3f;
  cfg.fine_threshold = 0.1f;
  cfg.coarse_pause_ms = 60000UL;
  cfg.fine_pause_ms = 180000UL;
  cfg.fert_pause_ms = 180000UL;
  cfg.max_ph_dose_per_pulse_per_liter = 0.5f;
  cfg.max_ph_total_per_liter = 5.0f;
  cfg.dose_gain_up = 0.2f;
  cfg.dose_gain_down = 0.2f;
  cfg.fine_dose_scale = 0.25f;
  cfg.process_timeout_ms = 3600000UL;
  return cfg;
}

Config g_config = makeDefaultConfig();
bool g_loaded = false;

void sanitize(Config& cfg) {
  if (!isfinite(cfg.ph_gain) || fabs(cfg.ph_gain) < 0.01f) {
    cfg.ph_gain = -5.70f;
  }
  if (!isfinite(cfg.ph_offset)) {
    cfg.ph_offset = 21.34f;
  }
  for (size_t i = 0; i < kPhReferenceCount; ++i) {
    if (!isfinite(cfg.ph_reference_voltages[i])) {
      cfg.ph_reference_voltages[i] = NAN;
      cfg.ph_valid_mask &= ~(1 << i);
    }
  }
  if (!isfinite(cfg.ec_alpha) || cfg.ec_alpha < 0.0f || cfg.ec_alpha > 0.2f) {
    cfg.ec_alpha = 0.02f;
  }
  if (cfg.ec_tds_factor <= 0 || cfg.ec_tds_factor > 2000) {
    cfg.ec_tds_factor = 500;
  }
  if (!isfinite(cfg.ec_k_cell) || cfg.ec_k_cell <= 0.0f || cfg.ec_k_cell > 10.0f) {
    cfg.ec_k_cell = 1.0f;
  }
  for (size_t i = 0; i < kPumpChannelCount; ++i) {
    if (!isfinite(cfg.pump_ml_per_sec[i]) || cfg.pump_ml_per_sec[i] <= 0.0f ||
        cfg.pump_ml_per_sec[i] > 100.0f) {
      cfg.pump_ml_per_sec[i] = (i < 2) ? 1.0f : 3.0f;
    }
  }
  cfg.coarse_threshold = constrain(cfg.coarse_threshold, 0.05f, 2.0f);
  cfg.fine_threshold = constrain(cfg.fine_threshold, 0.01f, cfg.coarse_threshold);
  cfg.coarse_pause_ms = constrain(cfg.coarse_pause_ms, 1000UL, 15UL * 60UL * 1000UL);
  cfg.fine_pause_ms = constrain(cfg.fine_pause_ms, 1000UL, 20UL * 60UL * 1000UL);
  cfg.fert_pause_ms = constrain(cfg.fert_pause_ms, 1000UL, 20UL * 60UL * 1000UL);
  if (!isfinite(cfg.max_ph_dose_per_pulse_per_liter) || cfg.max_ph_dose_per_pulse_per_liter <= 0.0f) {
    cfg.max_ph_dose_per_pulse_per_liter = 0.5f;
  }
  if (!isfinite(cfg.max_ph_total_per_liter) || cfg.max_ph_total_per_liter <= 0.0f) {
    cfg.max_ph_total_per_liter = 5.0f;
  }
  if (!isfinite(cfg.dose_gain_up) || cfg.dose_gain_up <= 0.0f) {
    cfg.dose_gain_up = 0.2f;
  }
  if (!isfinite(cfg.dose_gain_down) || cfg.dose_gain_down <= 0.0f) {
    cfg.dose_gain_down = 0.2f;
  }
  if (!isfinite(cfg.fine_dose_scale) || cfg.fine_dose_scale <= 0.0f || cfg.fine_dose_scale > 1.0f) {
    cfg.fine_dose_scale = 0.25f;
  }
  if (cfg.process_timeout_ms < 60000UL || cfg.process_timeout_ms > 6UL * 60UL * 60UL * 1000UL) {
    cfg.process_timeout_ms = 3600000UL;
  }
}

}  // namespace

Config& config() { return g_config; }

uint16_t crc16(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

void loadOrDefaults() {
  StoredConfig stored;
  EEPROM.get(0, stored);
  bool valid = stored.version == kConfigVersion;
  if (valid) {
    uint16_t expected = stored.crc;
    stored.crc = 0;
    uint16_t actual = crc16(reinterpret_cast<const uint8_t*>(&stored), sizeof(stored));
    if (actual == expected) {
      g_config = stored.config;
      sanitize(g_config);
      g_loaded = true;
      return;
    }
  }
  g_config = makeDefaultConfig();
  sanitize(g_config);
  g_loaded = true;
}

bool save() {
  sanitize(g_config);
  StoredConfig stored;
  stored.version = kConfigVersion;
  stored.config = g_config;
  stored.crc = 0;
  stored.crc = crc16(reinterpret_cast<const uint8_t*>(&stored), sizeof(stored));
  EEPROM.put(0, stored);
  return true;
}

void dump(Stream& stream) {
  if (!g_loaded) {
    loadOrDefaults();
  }
  stream.println(F("=== SunKiss Config ==="));
  stream.print(F("Version: "));
  stream.println(kConfigVersion);
  stream.print(F("Device ID: "));
  stream.println(g_config.device_id);
  stream.print(F("Relay active low: "));
  stream.println(g_config.relay_active_low ? F("true") : F("false"));

  stream.println(F("-- pH calibration"));
  stream.print(F("  gain: "));
  stream.println(g_config.ph_gain, 6);
  stream.print(F("  offset: "));
  stream.println(g_config.ph_offset, 6);
  const float references[3] = {4.01f, 6.86f, 9.18f};
  for (size_t i = 0; i < kPhReferenceCount; ++i) {
    stream.print(F("  ref "));
    stream.print(references[i], 2);
    stream.print(F(": "));
    if ((g_config.ph_valid_mask >> i) & 0x01) {
      stream.println(g_config.ph_reference_voltages[i], 4);
    } else {
      stream.println(F("--"));
    }
  }

  stream.println(F("-- EC/TDS"));
  stream.print(F("  alpha: "));
  stream.println(g_config.ec_alpha, 5);
  stream.print(F("  K cell: "));
  stream.println(g_config.ec_k_cell, 4);
  stream.print(F("  TDS factor: "));
  stream.println(g_config.ec_tds_factor);

  stream.println(F("-- Pumps ml/sec"));
  const char* labels[kPumpChannelCount] = {"pH down", "pH up", "A", "B"};
  for (size_t i = 0; i < kPumpChannelCount; ++i) {
    stream.print(F("  "));
    stream.print(labels[i]);
    stream.print(F(": "));
    stream.println(g_config.pump_ml_per_sec[i], 3);
  }

  stream.println(F("-- Process"));
  stream.print(F("  coarse threshold: "));
  stream.println(g_config.coarse_threshold, 3);
  stream.print(F("  fine threshold: "));
  stream.println(g_config.fine_threshold, 3);
  stream.print(F("  coarse pause ms: "));
  stream.println(g_config.coarse_pause_ms);
  stream.print(F("  fine pause ms: "));
  stream.println(g_config.fine_pause_ms);
  stream.print(F("  fert pause ms: "));
  stream.println(g_config.fert_pause_ms);
  stream.print(F("  max pulse ml/L: "));
  stream.println(g_config.max_ph_dose_per_pulse_per_liter, 3);
  stream.print(F("  max total ml/L: "));
  stream.println(g_config.max_ph_total_per_liter, 3);
  stream.print(F("  dose gain up: "));
  stream.println(g_config.dose_gain_up, 4);
  stream.print(F("  dose gain down: "));
  stream.println(g_config.dose_gain_down, 4);
  stream.print(F("  fine dose scale: "));
  stream.println(g_config.fine_dose_scale, 3);
  stream.print(F("  timeout ms: "));
  stream.println(g_config.process_timeout_ms);
}

}  // namespace storage

