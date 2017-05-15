#!/bin/bash

set -e

g++ -std=c++11 mkeeprom.cpp -o mkeeprom

./mkeeprom 1111 2 > eeprom.bin
avr-objcopy -I binary -O ihex eeprom.bin livingroom.hex

./mkeeprom 1080 4 > eeprom.bin
avr-objcopy -I binary -O ihex eeprom.bin bedroom.hex

./mkeeprom 1101 5 > eeprom.bin
avr-objcopy -I binary -O ihex eeprom.bin studio.hex

./mkeeprom 1080 6 > eeprom.bin
avr-objcopy -I binary -O ihex eeprom.bin office.hex

./mkeeprom 1080 7 > eeprom.bin
avr-objcopy -I binary -O ihex eeprom.bin kidsroom.hex

