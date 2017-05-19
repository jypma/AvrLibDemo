#include <stdint.h>
#include <stdio.h>
#include "eeprom.hpp"

EEPROM eeprom;

int main() {
	eeprom.bandgapVoltage = 1080;
	eeprom.id = 'b';
    fwrite(&eeprom, sizeof(EEPROM), 1, stdout);
    return 0;
}
