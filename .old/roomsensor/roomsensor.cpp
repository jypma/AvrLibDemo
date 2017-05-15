#include "RoomSensor/RoomSensor.hpp"
#include "eeprom/eeprom.hpp"

typedef RoomSensor::RoomSensor<&EEPROM::bandgapVoltage, &EEPROM::id> TheApp;
RUN_APP(TheApp)

LOGGING_TO(app.pinTX)
