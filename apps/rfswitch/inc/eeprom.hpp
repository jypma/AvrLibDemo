#pragma once

#include <stdint.h>

struct EEPROM {
  uint8_t id;
  uint8_t input_0;
  uint8_t input_1;
  uint8_t input_2;
  uint8_t input_3;
  uint8_t output_0;
  uint8_t output_1;
  uint8_t output_2;
  uint8_t output_3;
} __attribute__((packed));
