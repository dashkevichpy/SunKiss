#include <Arduino.h>

#include "proto/SerialProto.h"
#include "process/Process.h"
#include "sensors/Sensors.h"
#include "storage/Config.h"

sensors::VccMeter gVccMeter;
sensors::PhSensor gPhSensor;
sensors::EcSensor gEcSensor;
sensors::SensorPollingManager gSensors(gVccMeter, gPhSensor, gEcSensor);
process::Process gProcess(gPhSensor, gEcSensor);
proto::SerialProto gProto(Serial, gProcess, gPhSensor, gEcSensor, gVccMeter);

void setup() {
  Serial.begin(115200);
  storage::loadOrDefaults();
  gSensors.begin();
  gProcess.begin();
  gProto.begin();
}

void loop() {
  gSensors.update();
  gProcess.onTimer();
  gProcess.onSensorTick();
  gProto.update();
}
