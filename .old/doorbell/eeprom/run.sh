#!/bin/bash

set -e

g++ -std=c++11 mkeeprom.cpp -o mkeeprom

./mkeeprom 1080 1 > eeprom.bin
avr-objcopy -I binary -O ihex eeprom.bin main_door.hex

./mkeeprom 1080 2 > eeprom.bin
avr-objcopy -I binary -O ihex eeprom.bin guest_bathroom.hex

