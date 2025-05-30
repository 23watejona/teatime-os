BASE_DIR = ..

CC=xtensa-lx106-elf-gcc 
C_FLAGS=-Wall -Werror -O0 -ffreestanding 
LD=xtensa-lx106-elf-ld
LD_FLAGS=-nostdlib -nostartfiles -T
ASM=xtensa-lx106-elf-as
ASM_FLAGS=--warn --fatal-warnings


BUILD=./build
SRC=$(shell ./srcc.sh)
SRCASM=$(shell ./srcasm.sh)
OBJ=$(SRC:$(BASE_DIR)/%.c=$(BUILD)/%.o)
OBJ+=$(SRCASM:$(BASE_DIR)/%.s=$(BUILD)/%.o)
INCLUDE_DIR=$(BASE_DIR)/include
ESPTOOL=../../ESP8266_RTOS_SDK/components/esptool_py/esptool/esptool.py
ESPELF=elf2image
ESPFLASH=--port /dev/ttyUSB0 write_flash
ESPFLASHFLAGS=-fm qio 0x00000

default: prog.bin
all: flash

flash: prog.bin
	$(ESPTOOL) $(ESPFLASH) $(ESPFLASHFLAGS) $<

prog.bin: prog.elf
	$(ESPTOOL) $(ESPELF) $< -o $@

prog.elf: $(OBJ)
	$(LD) $(LD_FLAGS) prog.ld $^ -o $@
	
$(BUILD)/%.o: $(BASE_DIR)/%.c
	$(CC) -o $@ -I $(INCLUDE_DIR) -c $(C_FLAGS) $<

$(BUILD)/%.o: $(BASE_DIR)/%.s
	$(ASM) -o $@ -I $(INCLUDE_DIR) -c $(ASN_FLAGS) $<

clean:
	rm $(OBJ) prog.bin prog.elf
