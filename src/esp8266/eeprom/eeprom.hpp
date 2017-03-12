#ifndef EEPROM_EEPROM_HPP

struct EEPROM {
    char apn[32];
    char password[64];
    char remoteIP[15];
    uint16_t remotePort;
} __attribute__((packed));

#endif
