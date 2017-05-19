#include <stdint.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include "eeprom.hpp"

EEPROM eeprom;

int main(int argc, char * argv[]) {
	if (argc != 3) {
		std::cerr << "need 2 args, not " << (argc-1)<< std::endl;
		return 1;
	}
	eeprom.bandgapVoltage = std::stoi(argv[1]);
    eeprom.id = std::stoi(argv[2]);
    fwrite(&eeprom, sizeof(EEPROM), 1, stdout);
    return 0;
}
