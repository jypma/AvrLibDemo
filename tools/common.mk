MCU=atmega328p
F_CPU=16000000UL
CC=avr-gcc
CXX=avr-g++
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
AVRSIZE=avr-size
AVRDUDE=avrdude
FUSE_PROGRAMMER_TYPE=usbasp
PROGRAMMER_TYPE=arduino
BAUD=115200
PBAUD=115200
PROGRAMMER_PORT=$(shell echo /dev/ttyUSB*)
SERIAL_PORT=$(PROGRAMMER_PORT)
#PROGRAMMER_PORT=/dev/ttyUSB0
PROGRAMMER_ARGS=-P$(PROGRAMMER_PORT) -b$(PBAUD)
FUSE_PROGRAMMER_ARGS=-B 46.88
BOOTLOADER=../../tools/optiboot_atmega328.hex
LFUSE = 0xFF
HFUSE = 0xDE
EFUSE = 0x05

AVRLIB=../../../AvrLib
INCLUDES=-I$(AVRLIB)/inc -Iinc

SOURCEDIR=src/avr
BUILDDIR=target/$(MCU)
MKEEPROM=target/mkeeprom
EEPROM=example 
EEPROM_FILE=$(BUILDDIR)/$(addsuffix .eeprom_hex, $(EEPROM))

#TARGET=$(BUILDDIR)/hello
TARGET=$(BUILDDIR)/$(lastword $(subst /, ,$(CURDIR)))

SOURCES=$(wildcard $(SOURCEDIR)/*.cpp)
OBJECTS=$(patsubst $(SOURCEDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))

EEPROM_SOURCEDIR=src/eeprom
EEPROMS=$(wildcard $(EEPROM_SOURCEDIR)/*.eeprom)
EEPROM_BINS=$(patsubst $(EEPROM_SOURCEDIR)/%.eeprom,$(BUILDDIR)/%.eeprom_bin,$(EEPROMS))

## Compilation options, type man avr-gcc if you're curious.
CPPFLAGS = -DF_CPU=$(F_CPU) -std=gnu++17 -fno-use-cxa-atexit 
CFLAGS = -Os -g -Wall
## Use short (8-bit) data types 
CFLAGS += -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums 
## Splits up object files per function
CFLAGS += -ffunction-sections -fdata-sections 

LDFLAGS = -Wl,-Map,$(TARGET).map 
## Optional, but often ends up with smaller code
LDFLAGS += -Wl,--gc-sections 
## Relax shrinks code even more, but makes disassembly messy
## LDFLAGS += -Wl,--relax
## LDFLAGS += -Wl,-u,vfprintf -lprintf_flt -lm  ## for floating-point printf
## LDFLAGS += -Wl,-u,vfprintf -lprintf_min      ## for smaller printf

#LDLIBS=
LDLIBS = -L$(AVRLIB)/target/$(MCU) -lAvrLib

TARGET_ARCH = -mmcu=$(MCU)

FUSE_STRING = -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m -U efuse:w:$(EFUSE):m 

all: directories $(TARGET).hex size

##  To make .o files from .cpp files 
$(OBJECTS): $(BUILDDIR)/%.o: $(SOURCEDIR)/%.cpp Makefile
	$(CXX) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) $(INCLUDES) -MMD -c $< -o $@

$(TARGET).elf: $(OBJECTS)
	$(MAKE) -C $(AVRLIB)
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LDLIBS) -o $@

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

$(MKEEPROM): src/eeprom/mkeeprom.cpp inc/eeprom.hpp Makefile
	g++ -std=c++11 -I inc/ src/eeprom/mkeeprom.cpp -o $@

$(EEPROM_BINS): $(BUILDDIR)/%.eeprom_bin: $(EEPROM_SOURCEDIR)/%.eeprom $(MKEEPROM)
	$(MKEEPROM) $(shell cat $< ) > $@

%.eeprom_hex: %.eeprom_bin
	avr-objcopy -I binary -O ihex $< $@

%.lst: %.elf
	$(OBJDUMP) -S $< > $@

## These targets don't have files named after them
.PHONY: all directories disassemble disasm eeprom size flash fuses serial

directories: $(BUILDDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

disassemble: $(TARGET).lst
	less $(TARGET).lst

size:  $(TARGET).elf
	$(AVRSIZE) -C --mcu=$(MCU) $(TARGET).elf

flash: $(TARGET).hex size
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -U flash:w:$<

flash_ext: $(TARGET).hex size
	$(AVRDUDE) -c $(FUSE_PROGRAMMER_TYPE) -p $(MCU) $(FUSE_PROGRAMMER_ARGS) -U flash:w:$<

eeprom: $(EEPROM_FILE)
	$(AVRDUDE) -c $(FUSE_PROGRAMMER_TYPE) -p $(MCU) $(FUSE_PROGRAMMER_ARGS) -U eeprom:w:$<

#need to find the old bootloader that supports eeprom first
#upload: $(TARGET).hex $(EEPROM_FILE) size
#	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -U eeprom:w:$(EEPROM_FILE) -U flash:w:$(TARGET).hex 

fuses: 
	$(AVRDUDE) -c $(FUSE_PROGRAMMER_TYPE) -p $(MCU) $(FUSE_PROGRAMMER_ARGS) $(FUSE_STRING)

show_fuses:
	$(AVRDUDE) -c $(FUSE_PROGRAMMER_TYPE) -p $(MCU) $(FUSE_PROGRAMMER_ARGS) -nv	

serial:
	stty -F $(SERIAL_PORT) $(BAUD) raw -clocal -echo -iexten
	cat $(SERIAL_PORT) | awk '{ print strftime("%c: "), $$0; fflush(); }' | tee target/serial.log

hex_serial:
	stty -F $(SERIAL_PORT) $(BAUD) raw -clocal -echo -iexten
	hexdump -v -e '1/1 "%02x\n"' $(SERIAL_PORT)


## Set the EESAVE fuse byte to preserve EEPROM across flashes
set_eeprom_save_fuse: HFUSE = 0xD6
set_eeprom_save_fuse: FUSE_STRING = -U hfuse:w:$(HFUSE):m
set_eeprom_save_fuse: fuses

## Clear the EESAVE fuse byte
clear_eeprom_save_fuse: FUSE_STRING = -U hfuse:w:$(HFUSE):m
clear_eeprom_save_fuse: fuses

-include $(BUILDDIR)/*.d

rtags:
	$(MAKE) -nk $(OBJECTS) | rc -c -

bootloader:
	$(AVRDUDE) -c $(FUSE_PROGRAMMER_TYPE) -p $(MCU) $(FUSE_PROGRAMMER_ARGS) -B 46.88 \
	-e -Ulock:w:0x3F:m -Uefuse:w:0xFD:m -Uhfuse:w:0xDE:m -Ulfuse:w:0xFF:m

	$(AVRDUDE) -c $(FUSE_PROGRAMMER_TYPE) -p $(MCU) $(FUSE_PROGRAMMER_ARGS) \
	-Uflash:w:$(BOOTLOADER):i -Ulock:w:0x0F:m

