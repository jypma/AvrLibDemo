#pragma once

#include <stdint.h>

struct EEPROM {
    uint8_t id;
    uint8_t inverted;     // bits 0..3: whether outputs 0..3 should be inverted before applying, i.e. send 0 when output should be high
    uint8_t auto_toggle;  // bits in groups of 2, one group for each channel: 00 = no autotoggle, 01 = toggle on AC, 02 = toggle on DC, 03 = toggle on DC going low
} __attribute__((packed));
