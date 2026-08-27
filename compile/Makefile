SRC_DIR=../src
INCLUDE_DIR=$(SRC_DIR)/include
BUILD_DIR=build

CROSS=xtensa-lx106-elf-
CC=$(CROSS)gcc
LD=$(CROSS)ld
AS=$(CROSS)as

CFLAGS=-MMD -P -g -std=gnu99 -Wall -Werror -Wno-missing-braces -O0 \
       -ffreestanding -fzero-initialized-in-bss -I $(INCLUDE_DIR) \
       -ffunction-sections -fdata-sections -mlongcalls
LDFLAGS=-nostdlib -nostartfiles --gc-sections
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

AP_SECRETS=$(INCLUDE_DIR)/ap_secrets.h

# Untracked, per-checkout AP credentials. Generated from the environment when
# absent; the build refuses to proceed rather than bake in a default network.
$(AP_SECRETS):
	@if [ -n "$$WIFI_SSID" ] && [ -n "$$WIFI_PASS" ] && [ -n "$$WIFI_BSSID" ]; then \
	  { echo '#ifndef AP_SECRETS_H'; echo '#define AP_SECRETS_H'; \
	    printf '#define AP_SSID  "%s"\n' "$$WIFI_SSID"; \
	    printf '#define AP_PASS  "%s"\n' "$$WIFI_PASS"; \
	    printf '#define AP_BSSID "%s"\n' "$$WIFI_BSSID"; \
	    printf '#define NET_IP   "%s"\n' "$${WIFI_NET_IP:-0.0.0.0}"; \
	    printf '#define NET_GW   "%s"\n' "$${WIFI_NET_GW:-0.0.0.0}"; \
	    echo '#endif'; } > $@; \
	  echo "generated $@ from environment"; \
	else \
	  echo "ERROR: $@ is missing."; \
	  echo "  Set WIFI_SSID, WIFI_PASS and WIFI_BSSID (e.g. WIFI_BSSID='02:00:00:00:00:01'),"; \
	  echo "  or copy $(AP_SECRETS).example to $@ and fill it in."; \
	  exit 1; \
	fi

# Host-compiled crypto self-test (RFC/FIPS vectors); needs no hardware.
test:
	cc -std=c99 -Wall -I $(INCLUDE_DIR) ../tests/crypto_test.c $(SRC_DIR)/device/wifi/wifi_crypto.c -o build/crypto_test
	./build/crypto_test

all: flash monitor

flash: prog.bin
	@test -n "$(USB)" || { echo "ERROR: no serial port found — plug the board in or set USB=/dev/..."; exit 1; }
	$(ESPTOOL) --port $(USB) write_flash -fm qio 0x00000 prog.bin 0x10000 prog.irom.bin

monitor:
	picocom $(USB) -b 76800

prog.bin: prog.elf
	@echo "Generating $@ from $<"
	@$(ESPTOOL) --chip esp8266 elf2image $< -o $@
	@mv $@0x00000.bin $@
	@mv $@0x10000.bin prog.irom.bin

prog.elf: $(OBJ) prog.ld peri.ld
	@echo "Linking $@"
	@$(LD) $(LDFLAGS) -T prog.ld peri.ld $(OBJ) -o $@

$(OBJ): | $(AP_SECRETS)

$(BUILD_DIR)/device/wifi/wifi_crypto.o $(BUILD_DIR)/device/wifi/wifi_ccmp.o \
$(BUILD_DIR)/device/net/net.o $(BUILD_DIR)/device/wifi/wifi_wpa.o \
$(BUILD_DIR)/device/wifi/wifi_ap_scan.o: CFLAGS += -Os

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
