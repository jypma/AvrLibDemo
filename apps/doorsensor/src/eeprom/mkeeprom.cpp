#include <stdint.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include "eeprom.hpp"

EEPROM eeprom;

int main(int argc, char * argv[]) {
	if (argc != 3) {
		std::cerr << "Usage: mkeeprom id bandgapVoltage" << (argc-1)<< std::endl;
		return 1;
	}
	eeprom.bandgapVoltage = std::stoi(argv[1]);
	eeprom.id = argv[2][0];
    fwrite(&eeprom, sizeof(EEPROM), 1, stdout);
    return 0;
}
