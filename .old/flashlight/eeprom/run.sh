#!/bin/bash

gcc -std=c++11 mkeeprom.cpp -o mkeeprom
./mkeeprom > eeprom1.bin
avr-objcopy -I binary -O ihex eeprom1.bin eeprom1.hex
