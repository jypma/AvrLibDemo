#pragma once

#include <stdint.h>

struct EEPROM {
    uint8_t id;
    uint8_t inverted;     // bits 0..3: whether outputs 0..3 should be inverted before applying, i.e. incoming 0 means high.
    uint8_t auto_toggle;   // bits 0..3: whether to automatically toggle output 0..3 when input 0..3 is toggled
} __attribute__((packed));
