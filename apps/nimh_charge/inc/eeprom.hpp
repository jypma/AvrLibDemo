#pragma once

#include <stdint.h>

struct EEPROM {
    uint16_t bandgapVoltage;
} __attribute__((packed));
