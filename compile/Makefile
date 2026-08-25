SRC_DIR=../src
INCLUDE_DIR=$(SRC_DIR)/include
BUILD_DIR=build

CROSS=xtensa-lx106-elf-
CC=$(CROSS)gcc
LD=$(CROSS)ld
AS=$(CROSS)as

CFLAGS=-MMD -P -g -std=gnu99 -Wall -Werror -Wno-missing-braces -O0 \
       -ffreestanding -fzero-initialized-in-bss -I $(INCLUDE_DIR)
LDFLAGS=-nostdlib -nostartfiles
ASFLAGS=--warn --fatal-warnings -I $(INCLUDE_DIR)

SRC_C=$(shell find $(SRC_DIR) -name '*.c')
SRC_S=$(shell find $(SRC_DIR) -name '*.s')
SRC_S_CAP=$(shell find $(SRC_DIR) -name '*.S')

OBJ=$(SRC_C:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) \
    $(SRC_S:$(SRC_DIR)/%.s=$(BUILD_DIR)/%.o) \
    $(SRC_S_CAP:$(SRC_DIR)/%.S=$(BUILD_DIR)/%.o)
DEPENDS=$(OBJ:%.o=%.d)

# Auto-discover the board's serial port (macOS CP210x / SiLabs, then Linux). Override with USB=...
USB?=$(shell ls /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART* /dev/tty.usbserial-* /dev/ttyUSB* 2>/dev/null | head -n1)
ESPTOOL=esptool

.PHONY: default all flash monitor clean test

default: prog.bin

# Host-compiled crypto self-test (RFC/FIPS vectors); needs no hardware.
test:
	cc -std=c99 -Wall -I $(INCLUDE_DIR) ../tests/crypto_test.c $(SRC_DIR)/device/wifi/wifi_crypto.c -o build/crypto_test
	./build/crypto_test

all: flash monitor

flash: prog.bin
	$(ESPTOOL) --port $(USB) write_flash -fm qio 0x00000 $<

monitor:
	picocom $(USB) -b 76800

prog.bin: prog.elf
	@echo "Generating $@ from $<"
	@$(ESPTOOL) --chip esp8266 elf2image $< -o $@
	@mv $@0x00000.bin $@

prog.elf: $(OBJ) prog.ld peri.ld
	@echo "Linking $@"
	@$(LD) $(LDFLAGS) -T prog.ld peri.ld $(OBJ) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<"
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	@echo "Assembling $<"
	@mkdir -p $(@D)
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S
	@echo "Assembling $<"
	@mkdir -p $(@D)
	@$(CC) -I $(INCLUDE_DIR) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) prog.bin prog.elf

-include $(DEPENDS)
