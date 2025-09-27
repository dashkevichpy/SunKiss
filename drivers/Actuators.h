#ifndef SUNKISS_DRIVERS_ACTUATORS_H
#define SUNKISS_DRIVERS_ACTUATORS_H

#include <Arduino.h>

#include "pins.h"

void pumpsAllOff();

void setMixPump1(bool on);
void setMixPump2(bool on);

void setPumpPhDown(bool on);
void setPumpPhUp(bool on);

void setPumpA(bool on);
void setPumpB(bool on);

#endif  // SUNKISS_DRIVERS_ACTUATORS_H
