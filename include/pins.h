#ifndef SUNKISS_PINS_H
#define SUNKISS_PINS_H

#include <Arduino.h>

// I2C LCD (PCF8574 backpack)
constexpr uint8_t LCD_SDA_PIN = A4;
constexpr uint8_t LCD_SCL_PIN = A5;

// Analog inputs
constexpr uint8_t TDS_SENSOR_PIN = A0;
constexpr uint8_t PH_SENSOR_PIN = A1;
constexpr uint8_t LEVEL_SENSOR_1_PIN = A2;  // Reserved for future use
constexpr uint8_t LEVEL_SENSOR_2_PIN = A3;  // Reserved for future use

// Digital outputs - AC mixing pumps (via SSR, potentially active-low)
constexpr uint8_t MIX_PUMP_1_PIN = 5;
constexpr uint8_t MIX_PUMP_2_PIN = 6;

// Digital outputs - dosing pumps (via MOSFET drivers, active-high)
constexpr uint8_t PUMP_PH_DOWN_PIN = 7;
constexpr uint8_t PUMP_PH_UP_PIN = 8;
constexpr uint8_t PUMP_A_PIN = 9;
constexpr uint8_t PUMP_B_PIN = 10;

// Temperature sensor (reserved)
constexpr uint8_t TEMP_SENSOR_PIN = 4;

// Relay logic configuration
#define RELAY_ACTIVE_LOW true

#endif  // SUNKISS_PINS_H
