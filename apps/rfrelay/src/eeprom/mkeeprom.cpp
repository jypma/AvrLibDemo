#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include "eeprom.hpp"

EEPROM eeprom;

int main(int argc, char * argv[]) {
    if (argc != 5) {
        std::cerr << "need 4 args, not " << (argc-1)<< std::endl;
        return 1;
    }
	strncpy(eeprom.apn, argv[1], 32);
	strncpy(eeprom.password, argv[2], 64);
	strncpy(eeprom.remoteIP, argv[3], 15); // .255 broadcast works just fine.
	eeprom.remotePort = std::stoi(argv[4]);
    fwrite(&eeprom, sizeof(EEPROM), 1, stdout);
    return 0;
}
