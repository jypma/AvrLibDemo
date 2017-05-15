#include <stdint.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include "eeprom.hpp"

EEPROM eeprom;

int main(int argc, char * argv[]) {
	if (argc != 2) {
		std::cerr << "need 1 args, not " << (argc-1)<< std::endl;
		return 1;
	}
	//eeprom.bandgapVoltage = std::stoi(argv[1]);
	eeprom.id = argv[1][0];
    fwrite(&eeprom, sizeof(EEPROM), 1, stdout);
    return 0;
}
