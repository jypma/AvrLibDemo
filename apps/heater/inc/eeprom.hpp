#pragma once

#include <stdint.h>

struct EEPROM {
  uint8_t id;
  /**
   * bits 0..11:
   * whether outputs 0..11 should be inverted before applying, i.e. send 0 when output should be high
   */
  uint16_t inverted;
} __attribute__((packed));
