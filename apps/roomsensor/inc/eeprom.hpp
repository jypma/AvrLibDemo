#pragma once

#include <stdint.h>

struct EEPROM {
    uint16_t bandgapVoltage;
    uint8_t id;
} __attribute__((packed));
