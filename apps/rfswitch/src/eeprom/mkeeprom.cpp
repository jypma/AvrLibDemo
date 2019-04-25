#include <stdint.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include "eeprom.hpp"

EEPROM eeprom;

int main(int argc, char * argv[]) {
  if (argc != 10) {
    std::cerr << "Usage: mkeeprom id input0 input1 input2 input3 output0 output1 output2 output3 " << std::endl;
    return 1;
  }
  eeprom.id = argv[1][0];
  eeprom.input0 = std::stoi(argv[2]);
  eeprom.input1 = std::stoi(argv[3]);
  eeprom.input2 = std::stoi(argv[4]);
  eeprom.input3 = std::stoi(argv[5]);
  eeprom.output0 = std::stoi(argv[6]);
  eeprom.output1 = std::stoi(argv[7]);
  eeprom.output2 = std::stoi(argv[8]);
  eeprom.output3 = std::stoi(argv[9]);

  fwrite(&eeprom, sizeof(EEPROM), 1, stdout);
  return 0;
}
