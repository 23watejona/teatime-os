BASE_DIR=..
BUILD_DIR=$(BASE_DIR)/compile/build
SRC_DIR=$(BASE_DIR)/src
INCLUDE_DIR=$(SRC_DIR)/include

CC=xtensa-lx106-elf-gcc 
C_FLAGS=-MMD -P -Wall -Werror -O0 -ffreestanding 
LD=xtensa-lx106-elf-ld
LD_FLAGS=-nostdlib -nostartfiles -T
ASM=xtensa-lx106-elf-as
ASM_FLAGS=--warn --fatal-warnings

# try $(wildcard)
SRC_C=$(shell ./list_src.sh c)
SRC_S=$(shell ./list_src.sh s)

OBJ_C=$(SRC_C:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJ_S=$(SRC_S:$(SRC_DIR)/%.s=$(BUILD_DIR)/%.o)
OBJ=$(OBJ_C) $(OBJ_S)
DEPENDS=$(OBJ_C:%.o %.d)

ESPTOOL=esptool
ESPELF=elf2image
ESPFLASH=--port /dev/ttyUSB0 write_flash
ESPFLASHFLAGS=-fm qio 0x00000

.PHONY: all flash clean
default: prog.bin
all: flash monitor

flash: prog.bin
	$(ESPTOOL) $(ESPFLASH) $(ESPFLASHFLAGS) $<
monitor:
	picocom /dev/ttyUSB0 -b 76800

prog.bin: prog.elf
	@echo "Generating $@ from $<"
	@$(ESPTOOL) $(ESPELF) $< -o $@
	@echo "Renaming to $@"
	@mv $@0x00000.bin $@

prog.elf: $(OBJ)
	@echo "Generating $@"
	@$(LD) $(LD_FLAGS) prog.ld $^ -o $@

.SECONDEXPANSION:

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $$(@D)/
	@echo "Compiling $<"
	@$(CC) -o $@ -I $(INCLUDE_DIR) -c $(C_FLAGS) $<
-include $(DEPENDS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $$(@D)/
	@echo "Assembling $<"
	@$(ASM) -o $@ -I $(INCLUDE_DIR) -c $(ASN_FLAGS) $<

$(BUILD_DIR)%/:
	@echo "Generating build dir $@"
	@mkdir -p $@

clean:
	@rm $(OBJ) prog.bin prog.elf
