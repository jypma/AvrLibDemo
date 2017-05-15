#include "GardenValve/GardenValve.hpp"

struct EEPROM {
    uint16_t bandgapVoltage;
};

RUN_APP(GardenValve::GardenValve<&EEPROM::bandgapVoltage>)

LOGGING_TO(app.pinTX)
