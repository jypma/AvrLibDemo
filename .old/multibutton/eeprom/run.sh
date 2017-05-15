#!/bin/bash

set -e

g++ -std=c++11 mkeeprom.cpp -o mkeeprom

./mkeeprom 1108 1 > eeprom.bin
avr-objcopy -I binary -O ihex eeprom.bin one.hex
