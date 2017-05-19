#ifndef EEPROM_EEPROM_HPP

#include <stdint.h>

struct EEPROM {
    uint16_t bandgapVoltage;
    uint8_t id;
} __attribute__((packed));

#endif
