#include <stdint.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include "eeprom.hpp"

EEPROM eeprom;

int main(int argc, char * argv[]) {
	if (argc != 4) {
		std::cerr << "Usage: mkeeprom id inverted autotoggle" << (argc-1)<< std::endl;
		return 1;
	}
	eeprom.id = argv[1][0];
    eeprom.inverted = std::stoi(argv[2]);
    eeprom.auto_toggle = std::stoi(argv[3]);
    fwrite(&eeprom, sizeof(EEPROM), 1, stdout);
    return 0;
}
