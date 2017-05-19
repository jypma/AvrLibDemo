#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "eeprom.hpp"

EEPROM eeprom;

int main() {
	strcpy(eeprom.apn, "AnimalZoo2");
	strcpy(eeprom.password, "6pRgMTzq9DE1ZNSPw4m7EdQT");
	strcpy(eeprom.remoteIP, "192.168.0.198"); // .255 broadcast works just fine.
	eeprom.remotePort = 4123;
    fwrite(&eeprom, sizeof(EEPROM), 1, stdout);
    return 0;
}
