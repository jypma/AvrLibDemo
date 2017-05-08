#pragma once

#include <stdint.h>

struct EEPROM {
    uint8_t id;
    uint8_t inverted;
} __attribute__((packed));
