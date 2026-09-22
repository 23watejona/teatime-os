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

# find returns directory order, so sort to keep link order the same on every host
SRC_C=$(sort $(shell find $(SRC_DIR) -name '*.c'))
SRC_S=$(sort $(shell find $(SRC_DIR) -name '*.s'))
SRC_S_CAP=$(sort $(shell find $(SRC_DIR) -name '*.S'))

OBJ=$(SRC_C:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) \
    $(SRC_S:$(SRC_DIR)/%.s=$(BUILD_DIR)/%.o) \
    $(SRC_S_CAP:$(SRC_DIR)/%.S=$(BUILD_DIR)/%.o)
DEPENDS=$(OBJ:%.o=%.d)

USB?=$(firstword $(wildcard /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART* \
                            /dev/tty.usbserial-* /dev/ttyUSB* /dev/ttyACM*))

# esptool before v5 installs under its script name, so fall back to that
ESPTOOL?=$(shell command -v esptool 2>/dev/null || echo esptool.py)
HOSTCC?=cc
PICOCOM?=picocom
BAUD?=76800

CHECK_USB=@test -n "$(USB)" || { echo "ERROR: no serial port found — plug the board in or set USB=/dev/..."; exit 1; }

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
# -iquote keeps this dir off the <> search path, so the host libc headers do not
# pick up our freestanding stdarg.h
test:
	@mkdir -p $(BUILD_DIR)
	$(HOSTCC) -std=c99 -Wall -iquote $(INCLUDE_DIR) ../tests/crypto_test.c $(SRC_DIR)/device/wifi/wifi_crypto.c -o $(BUILD_DIR)/crypto_test
	./$(BUILD_DIR)/crypto_test

all: flash monitor

flash: prog.bin
	$(CHECK_USB)
	$(ESPTOOL) --chip esp8266 --port $(USB) write_flash -fm qio 0x00000 prog.bin 0x10000 prog.irom.bin

monitor:
	$(CHECK_USB)
	$(PICOCOM) $(USB) -b $(BAUD)

prog.bin: prog.elf
	@echo "Generating $@ from $<"
	@$(ESPTOOL) --chip esp8266 elf2image $< -o $@
	@mv $@0x00000.bin $@
	@mv $@0x10000.bin prog.irom.bin

prog.elf: $(OBJ) prog.ld peri.ld
	@echo "Linking $@"
	@$(LD) $(LDFLAGS) -T prog.ld peri.ld $(OBJ) -o $@

$(OBJ): | $(AP_SECRETS)


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
	@rm -rf $(BUILD_DIR) prog.bin prog.irom.bin prog.elf

-include $(DEPENDS)
