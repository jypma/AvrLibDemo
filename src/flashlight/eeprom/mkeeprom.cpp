#include <stdint.h>
#include <stdio.h>

struct EEPROM {
    uint16_t bandgapVoltage = 1080;
} eeprom;

int main() {
    fwrite(&eeprom, sizeof(EEPROM), 1, stdout);
    return 0;
}
