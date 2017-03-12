#include "GardenSensor/GardenSensor.hpp"
#include "eeprom/eeprom.hpp"

typedef GardenSensor::GardenSensor<&EEPROM::bandgapVoltage, &EEPROM::id> TheApp;
RUN_APP(TheApp)

LOGGING_TO(app.pinTX)
