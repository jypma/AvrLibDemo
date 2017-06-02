MCU=atmega328p
F_CPU=16000000UL
CC=avr-gcc
CXX=avr-g++
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
AVRSIZE=avr-size
AVRDUDE=avrdude
PROGRAMMER_TYPE=arduino
PROGRAMMER_ARGS=-P/dev/ttyUSB0 -b57600 
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
CPPFLAGS = -DF_CPU=$(F_CPU) -std=gnu++14 -fno-use-cxa-atexit 
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
disasm: disassemble

size:  $(TARGET).elf
	$(AVRSIZE) -C --mcu=$(MCU) $(TARGET).elf

flash: $(TARGET).hex size
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -U flash:w:$<

flash_eeprom: $(EEPROM_FILE)
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -U eeprom:w:$<

upload: $(TARGET).hex $(EEPROM_FILE) size
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -U flash:w:$(TARGET).hex -U eeprom:w:$(EEPROM_FILE)

fuses: 
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) $(FUSE_STRING)

show_fuses:
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -nv	

BAUD=57600
serial:
	stty -F /dev/ttyUSB0 $(BAUD) raw -clocal -echo
	cat /dev/ttyUSB0 | awk '{ print strftime("%c: "), $$0; fflush(); }' | tee target/serial.log

## Set the EESAVE fuse byte to preserve EEPROM across flashes
set_eeprom_save_fuse: HFUSE = 0xD7
set_eeprom_save_fuse: FUSE_STRING = -U hfuse:w:$(HFUSE):m
set_eeprom_save_fuse: fuses

## Clear the EESAVE fuse byte
clear_eeprom_save_fuse: FUSE_STRING = -U hfuse:w:$(HFUSE):m
clear_eeprom_save_fuse: fuses

-include $(BUILDDIR)/*.d

